#include "business/RobotController.h"
#include "modules/logger/Logger.h"
#include "version.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace patrol {

namespace {
uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
uint32_t gainQ1000(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 4000000.0f) v = 4000000.0f;
    return static_cast<uint32_t>(v * 1000.0f + 0.5f);
}
uint16_t clampMaxDelta(uint16_t v) {
    return static_cast<uint16_t>(std::max<uint16_t>(1, std::min<uint16_t>(2000, v)));
}
const char* modeName(uint8_t m) {
    switch (m) {
        case serial_proto::MODE_MANUAL:   return "手动";
        case serial_proto::MODE_AUTO:     return "自动";
        case serial_proto::MODE_OBSTACLE: return "避障";
        case serial_proto::MODE_CRUISE:   return "巡检";
        default:                          return "未知";
    }
}
} // namespace

// 兼容旧接口：等价于 type=uart。
bool RobotController::init(const std::string& device, int baud,
                           const std::string& thermalDevice, int thermalBaud) {
    LinkConfig lc;
    lc.type = "uart";
    lc.device = device; lc.baud = baud;
    lc.thermalDevice = thermalDevice; lc.thermalBaud = thermalBaud;
    return init(lc);
}

bool RobotController::init(const LinkConfig& link) {
    useCan_ = (link.type == "can");
    ctrlDevice_ = link.device;   // 记住设备/波特率/接口名，供上行断流时重开自恢复
    ctrlBaud_   = link.baud;
    canIf_      = link.canIf;

    if (useCan_) {
        if (!can_.open(link.canIf)) {
            LOG_ERROR("RobotController: CAN 接口 %s 打开失败，运动控制不可用"
                      "（检查 can0 是否 up / 内核 CAN 子系统）", link.canIf.c_str());
            return false;
        }
        registerCanCallbacks();
        LOG_INFO("RobotController 就绪（传输层=CAN, %s @ 500kbps）", link.canIf.c_str());
    } else {
        if (!serial_.open(link.device, link.baud)) {
            LOG_ERROR("RobotController: 串口 %s 打开失败，运动控制不可用", link.device.c_str());
            return false;
        }
        registerControlCallbacks();
        LOG_INFO("RobotController 就绪（传输层=UART, %s @ %d）", link.device.c_str(), link.baud);
    }

    const std::string& thermalDevice = link.thermalDevice;
    const int thermalBaud = link.thermalBaud;

    // 热成像专用串口（双串口方案）：F4 USART1 → 龙芯 thermalDevice。
    // 收 0x5B(F4已解算,旧) 或 0x5D原始帧+0x5E EEPROM(龙芯解算,新)——两种都支持。
    // 与控制口物理隔离，热成像大数据不影响运动控制实时性。打开失败不影响控制。
    if (!thermalDevice.empty()) {
        if (thermalSerial_.open(thermalDevice, thermalBaud)) {
            thermalSerial_.setThermalCallback(
                [this](const int16_t* t, int c, int r) { onThermal(t, c, r); });
            thermalSerial_.setRawThermalCallback(
                [this](const uint16_t* p, int n) { onRawThermal(p, n); });
            thermalSerial_.setEepromCallback(
                [this](const uint16_t* p, int n) { onEeprom(p, n); });
            LOG_INFO("热成像串口就绪（%s @ %d，龙芯解算）", thermalDevice.c_str(), thermalBaud);
        } else {
            LOG_ERROR("热成像串口 %s 打开失败（热成像不可用，不影响控制）", thermalDevice.c_str());
        }
    }
    // 启动热成像解算独立线程：把重解算(ExtractParameters/CalculateTo)从控制线程剥离，
    // 保证控制线程严格实时（每 tick 快速下发命令帧，始终满足 F4 的 ~510ms 心跳）。
    if (!solverRun_.exchange(true))
        solverThread_ = std::thread([this] { solverLoop(); });

    return true;
}

// 给 can_ 挂全部回调：遥测/环境/PID 复用与串口完全相同的消费函数（只换"帧从哪来"），
// 另加 CAN 专有的巡检事件/扫描剖面/心跳回调。
void RobotController::registerCanCallbacks() {
    can_.setTelemetryCallback([this](const serial_proto::Telemetry& t) { onTelemetry(t); });
    can_.setEnvCallback([this](const serial_proto::EnvData& e) { onEnv(e); });
    can_.setPidTeleCallback([this](const serial_proto::PidTele& t) { onPidTele(t); });
    can_.setPatrolEventCallback([this](const can_proto::PatrolEvent& ev) { onPatrolEvent(ev); });
    can_.setAvoidProfileCallback([this](const can_proto::AvoidProfile& pr) { onAvoidProfile(pr); });
    can_.setHeartbeatCallback([this](const can_proto::Heartbeat& hb) { onHeartbeat(hb); });
}

void RobotController::onPatrolEvent(const can_proto::PatrolEvent& ev) {
    std::lock_guard<std::mutex> lk(mtx_);
    patrolEvt_ = ev;
    lastPatrolEvtMs_ = nowMs();
    ++patrolEvtRxCnt_;
    // 本轮只缓存事件供上报/日志；到点编排(MissionManager)在下一轮实现。
    LOG_INFO("[巡检事件] event=%u pointSeq=%u state=%u", ev.event, ev.pointSeq, ev.state);
    // ---- 到点编排（基础版）----
    // F4 到路口会停车并报 EVT_ARRIVE，等龙芯给下一步动作，否则会一直停着。
    // 完整版应在此处：解码 ArUco 认点号 → 查路线表 → 决定直行/左/右/结束。
    // 本轮先给"默认直行放行"，保证循线能连续跑通；路线表接入见 MissionManager。
    if (ev.event == can_proto::EVT_ARRIVE) {
        patrolActPoint_   = ev.pointSeq;
        patrolActAction_  = can_proto::ACT_STRAIGHT;
        patrolActPending_ = true;
        LOG_INFO("[巡检] 到点 seq=%u -> 放行(直行)", ev.pointSeq);
    }
}

void RobotController::onAvoidProfile(const can_proto::AvoidProfile& pr) {
    std::lock_guard<std::mutex> lk(mtx_);
    avoidProf_ = pr;
}

void RobotController::onHeartbeat(const can_proto::Heartbeat& hb) {
    std::lock_guard<std::mutex> lk(mtx_);
    f4HbState_ = hb.state;
    lastHbMs_  = nowMs();
}

// 给控制口挂全部回调（init 与串口重开后共用，保证重开后回调不丢）。
// 控制口也挂热成像回调，兼容 F4 单串口发送(0x5B/0x5D/0x5E)的情形。
void RobotController::registerControlCallbacks() {
    serial_.setTelemetryCallback([this](const serial_proto::Telemetry& t) { onTelemetry(t); });
    serial_.setEnvCallback([this](const serial_proto::EnvData& e) { onEnv(e); });
    serial_.setThermalCallback([this](const int16_t* t, int c, int r) { onThermal(t, c, r); });
    serial_.setRawThermalCallback([this](const uint16_t* p, int n) { onRawThermal(p, n); });
    serial_.setEepromCallback([this](const uint16_t* p, int n) { onEeprom(p, n); });
    serial_.setPidTeleCallback([this](const serial_proto::PidTele& t) { onPidTele(t); });
}

// 遥测长时间断流(端口仍开)时重开控制串口：自愈 fd 卡死/线缆抖动等异常。
// 只在控制线程(tick)内调用，与 serial_ 的 poll/send 同线程，无并发。
void RobotController::reopenControlSerial() {
    if (useCan_) {
        // CAN：内核层已有 restart-ms 自动重上线；此处兜底重建 socket(fd 卡死/接口抖动)。
        if (can_.reopen()) {
            registerCanCallbacks();
            LOG_INFO("F4 CAN 链路已重建: %s", canIf_.c_str());
        } else {
            LOG_ERROR("F4 CAN 链路重建失败: %s（下次周期重试）", canIf_.c_str());
        }
        return;
    }
    LOG_WARN("F4 控制串口 %s 遥测断流 >3s，重开串口自恢复...", ctrlDevice_.c_str());
    serial_.close();
    if (serial_.open(ctrlDevice_, ctrlBaud_)) {
        registerControlCallbacks();
        LOG_INFO("F4 控制串口已重开: %s @ %d", ctrlDevice_.c_str(), ctrlBaud_);
    } else {
        LOG_ERROR("F4 控制串口重开失败: %s（下次周期重试）", ctrlDevice_.c_str());
    }
}

RobotController::~RobotController() { close(); }

void RobotController::close() {
    // 先停解算线程（它独占轮询 thermalSerial_），再关串口，避免 use-after-close。
    solverRun_.store(false);
    if (solverThread_.joinable()) solverThread_.join();
    serial_.close();
    can_.close();
    thermalSerial_.close();
}

// 热成像解算独立线程：轮询热成像专线 + 消费待解算暂存（EEPROM 提参/原始帧解算）。
// 与控制线程物理隔离——解算再慢也不会拖延 F4 命令帧下发（不触发 F4 心跳超时掉线）。
void RobotController::solverLoop() {
    LOG_INFO("热成像解算线程启动（独立于控制线程，保证 F4 心跳实时）");
    std::vector<uint16_t> ee, raw;
    while (solverRun_.load()) {
        // 热成像专线读取+组帧；回调 onRawThermal/onEeprom 只把数据投入 pending（廉价）。
        thermalSerial_.poll();

        bool haveEe = false, haveRaw = false;
        {
            std::lock_guard<std::mutex> lk(rawMtx_);
            if (pendingEeNew_)  { ee.swap(pendingEe_);   pendingEeNew_  = false; haveEe  = true; }
            if (pendingRawNew_) { raw.swap(pendingRaw_); pendingRawNew_ = false; haveRaw = true; }
        }
        // 提参在前（solve 需先就绪），再解算最新一帧原始数据。
        if (haveEe && static_cast<int>(ee.size()) >= ThermalSolver::EE_WORDS)
            thermalSolver_.setEeprom(ee.data());
        if (haveRaw && static_cast<int>(raw.size()) >= ThermalSolver::FRAME_WORDS &&
            thermalSolver_.solve(raw.data(), thermalSolved_))
            onThermal(thermalSolved_, serial_proto::THERMAL_COLS, serial_proto::THERMAL_ROWS);

        // 无待处理数据时小睡，避免空转占 CPU（热成像仅 ~2Hz，10ms 轮询足够跟手）。
        if (!haveEe && !haveRaw)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_INFO("热成像解算线程退出");
}

float RobotController::ema(float prev, float sample, bool& init, float alpha) {
    if (!init) { init = true; return sample; }
    return prev + alpha * (sample - prev);
}

void RobotController::updateEncoderSpeed(int32_t enc1, int32_t enc2) {
    uint32_t now = nowMs();
    if (!encInit_) {
        encInit_ = true; lastEnc1_ = enc1; lastEnc2_ = enc2; lastEncMs_ = now;
        return;
    }
    uint32_t dt = now - lastEncMs_;
    if (dt < 50) return;   // 采样太密，累积到 >=50ms 再算，降低量化噪声
    auto clampI16 = [](double v) -> int16_t {
        return static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, v)));
    };
    double f = 1000.0 / dt;   // -> counts/s
    encSpeed1_ = clampI16((enc1 - lastEnc1_) * f);
    encSpeed2_ = clampI16((enc2 - lastEnc2_) * f);
    lastEnc1_ = enc1; lastEnc2_ = enc2; lastEncMs_ = now;
}

void RobotController::onTelemetry(const serial_proto::Telemetry& t) {
    std::lock_guard<std::mutex> lk(mtx_);
    // 扩展帧携带距离/编码器；旧帧仅速度/转向/模式，不覆盖已有距离
    if (t.hasDistance) {
        telem_ = t;
        // 距离 EMA 滤波（0 表示无回波/超量程，不参与滤波，避免把有效值拉向 0）
        if (t.dist_cm != 0)
            distFiltCm_ = ema(distFiltCm_, static_cast<float>(t.dist_cm), distInit_, 0.4f);
        updateEncoderSpeed(t.enc1, t.enc2);   // 编码器 -> 真实轮速
    } else {
        telem_.speed    = t.speed;
        telem_.steering = t.steering;
        telem_.mode     = t.mode;
    }
    telemValid_  = true;
    lastTelemMs_ = nowMs();
    ++telemRxCnt_;
}

void RobotController::onEnv(const serial_proto::EnvData& e) {
    std::lock_guard<std::mutex> lk(mtx_);
    env_       = e;
    lastEnvMs_ = nowMs();
    ++envRxCnt_;
    // 激光距离 EMA（超量程值不参与滤波）
    if (e.vl53_mm != serial_proto::VL53_OUT_OF_RANGE)
        vl53FiltMm_ = ema(vl53FiltMm_, static_cast<float>(e.vl53_mm), vl53Init_, 0.4f);
}

void RobotController::onThermal(const int16_t* temps, int cols, int rows) {
    std::lock_guard<std::mutex> lk(thermalMtx_);
    thermal_.assign(temps, temps + static_cast<size_t>(cols) * rows);
    thermalCols_ = cols;
    thermalRows_ = rows;
    thermalNew_  = true;
    ++thermalRxCnt_;

    // 热点检测：整幅最高温（0.01°C 单位）-> 疑似火源/过热告警，联动风险等级。
    int16_t vmax = -32768;
    for (size_t i = 0, n = static_cast<size_t>(cols) * rows; i < n; ++i)
        vmax = std::max(vmax, temps[i]);
    int maxC10 = static_cast<int>(std::lround(vmax / 10.0));   // ×10 °C
    thermalMaxC10_.store(maxC10, std::memory_order_relaxed);
    thermalHotspot_.store(maxC10 >= static_cast<int>(kHotspotThreshC * 10),
                          std::memory_order_relaxed);
}

void RobotController::onEeprom(const uint16_t* ee, int words) {
    if (words < ThermalSolver::EE_WORDS) return;
    // ★不在串口/控制线程内做 ExtractParameters（重）——暂存交解算线程处理，保控制线程实时。
    std::lock_guard<std::mutex> lk(rawMtx_);
    pendingEe_.assign(ee, ee + ThermalSolver::EE_WORDS);
    pendingEeNew_ = true;
}

void RobotController::onRawThermal(const uint16_t* raw, int words) {
    if (words < ThermalSolver::FRAME_WORDS) return;
    // ★不在串口/控制线程内解算（CalculateTo 重）——只暂存最新一帧(新盖旧)交解算线程。
    std::lock_guard<std::mutex> lk(rawMtx_);
    pendingRaw_.assign(raw, raw + ThermalSolver::FRAME_WORDS);
    pendingRawNew_ = true;
}

bool RobotController::takeThermal(std::vector<int16_t>& out, int& cols, int& rows) {
    std::lock_guard<std::mutex> lk(thermalMtx_);
    if (!thermalNew_) return false;
    out  = thermal_;
    cols = thermalCols_;
    rows = thermalRows_;
    thermalNew_ = false;
    return true;
}

void RobotController::computeAvoid(uint16_t d, int16_t baseSpeed,
                                   int16_t& speed, int16_t& steering) const {
    // d==0：无回波/超量程，视为前方通畅
    if (d != 0 && d < av_.stopDistCm) {
        speed    = 0;                 // 太近：停止前进
        steering = av_.turnSteer;     // 原地向右打舵脱困
    } else if (d != 0 && d < av_.slowDistCm) {
        speed    = av_.slowSpeed;     // 接近：减速
        steering = static_cast<int16_t>(av_.turnSteer / 2);  // 轻微绕行
    } else {
        speed    = baseSpeed;         // 通畅：按基础速度直行
        steering = 0;
    }
}

void RobotController::tick() {
    // 1) 读取上行遥测（同线程回调 onTelemetry/onEnv/onPidTele，均为廉价拷贝）
    //    ★热成像专线(thermalSerial_)与解算已移到 solverThread_——控制线程不碰热成像，
    //      任何一 tick 都能快速下发命令帧，始终满足 F4 的 ~510ms 心跳(不触发掉线)。
    if (useCan_) can_.poll();
    else         serial_.poll();

    // 精简状态日志(~2s一次)：一行看清 版本/F4→龙芯链路计数/关键传感器/风险/视觉。
    //   RX 计数不涨 => F4 TX→龙芯 RX 没接通或跑旧版本; 涨但上位机空 => 转发/LongLook 问题。
    {
        static uint32_t s_dbgTick = 0;
        if (++s_dbgTick % 60 == 0)
            LOG_INFO("[状态] %s", statusLine().c_str());
    }

    // 2) 取共享状态快照
    uint8_t  mode; int16_t mSpd, mStr; bool estop;
    serial_proto::Telemetry t;
    uint16_t avoidDist;   // 用于避障的距离：优先滤波后的稳定值
    uint32_t lastTelem; bool telValid;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        mode = mode_; mSpd = manualSpeed_; mStr = manualSteer_;
        estop = estop_; t = telem_;
        avoidDist = distInit_ ? static_cast<uint16_t>(std::lround(distFiltCm_)) : t.dist_cm;
        lastTelem = lastTelemMs_; telValid = telemValid_;
    }

    // 2.5) 串口自恢复看门狗：端口开着、曾收到过遥测，却连续 >3s 断流→疑似 fd 卡死/
    //      线缆抖动，重开串口自愈（F4 已把遥测与心跳解耦，正常绝不停发，触发即真异常）。
    //      限流：最多每 5s 一次，避免真断线时反复重开刷屏。
    if (isOpen() && telValid) {
        uint32_t now = nowMs();
        if ((now - lastTelem) > 3000 && (now - lastReopenMs_) > 5000) {
            lastReopenMs_ = now;
            reopenControlSerial();
        }
    }
    (void)avoidDist;   // 距离仍用于风险判断/上报，但不再驱动龙芯侧转向（见下）

    // 3) 计算运动指令
    //    ⚠️ 实施方案 R10：运动决策权统一归 F4 本地快环（循线/避障/绕行），
    //       龙芯不再据距离算转向（原 computeAvoid 已停用），否则会与 F4 状态机抢控制。
    //       龙芯只负责：手动模式直传摇杆；非手动模式给"基础前进意图 + 直行"，
    //       实际转向/避障由 F4 依 mode 本地闭合（P3/P4 的 patrol.c/avoid.c）。
    int16_t speed = 0, steering = 0;
    if (estop) {
        speed = 0; steering = 0;
    } else if (mode == serial_proto::MODE_MANUAL) {
        speed = mSpd; steering = mStr;
    } else {
        // 非手动：只给基础前进速度，转向恒 0（交 F4 本地决策）。
        speed    = (mode == serial_proto::MODE_OBSTACLE) ? av_.obstacleSpeed : av_.cruiseSpeed;
        steering = 0;
    }

    // 4) 下发命令帧（CAN 0x201 / UART 0xAA，兼作 F4 510ms 心跳源）
    if (useCan_) can_.sendCommand(speed, steering, mode);
    else         serial_.sendCommand(speed, steering, mode);

    // 4.4) 冲刷巡检命令（模式联动启停 / 到点放行）
    //   ★ CanManager 约定 send* 只在控制线程调用；setMode 可能来自网络线程、
    //     onPatrolEvent 来自 poll 回调，故都只置 pending，在此统一下发。
    {
        bool cmdP = false, actP = false;
        uint8_t cmdV = 0, actPt = 0, actAc = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            cmdP = patrolCmdPending_; cmdV = patrolCmdVal_;  patrolCmdPending_ = false;
            actP = patrolActPending_; actPt = patrolActPoint_;
            actAc = patrolActAction_;                         patrolActPending_ = false;
        }
        if (useCan_ && cmdP) {
            can_.sendPatrolCmd(cmdV, 0);
            LOG_INFO("[巡检] 下发启停 cmd=%u", cmdV);
        }
        if (useCan_ && actP) {
            can_.sendPatrolAct(actPt, actAc, 1);
            LOG_INFO("[巡检] 下发放行 point=%u action=%u", actPt, actAc);
        }
    }

    // 4.5) 冲刷 PID 调试命令队列（网络线程入队 → 本控制线程独占串口写，保证线程安全）
    {
        std::vector<net::PidCommand> pending;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            pending.swap(pidPending_);
        }
        for (const auto& pc : pending) {
            if (useCan_) {
                if (pc.sub == 1) {
                    uint32_t abortCode = 0;
                    uint32_t kp = gainQ1000(pc.kp), ki = gainQ1000(pc.ki), kd = gainQ1000(pc.kd);
                    uint16_t maxDelta = clampMaxDelta(pc.maxDelta);
                    uint8_t closed = pc.closedLoop ? 1 : 0;
                    bool ok = can_.sdoWrite(can_proto::OD_PID, 1, &kp, sizeof(kp), &abortCode) &&
                              can_.sdoWrite(can_proto::OD_PID, 2, &ki, sizeof(ki), &abortCode) &&
                              can_.sdoWrite(can_proto::OD_PID, 3, &kd, sizeof(kd), &abortCode) &&
                              can_.sdoWrite(can_proto::OD_PID, 4, &maxDelta, sizeof(maxDelta), &abortCode) &&
                              can_.sdoWrite(can_proto::OD_PID, 5, &closed, sizeof(closed), &abortCode);
                    if (!ok)
                        LOG_WARN("CAN PID 参数 SDO 写失败 abort=0x%08X", abortCode);
                    else
                        LOG_INFO("CAN PID 参数已写入: kp=%.3f ki=%.3f kd=%.3f maxD=%u closed=%d",
                                 pc.kp, pc.ki, pc.kd, maxDelta, closed ? 1 : 0);
                } else if (pc.sub == 2) {
                    can_.sendPidTest(pc.testMode, pc.left, pc.right, pc.durationMs);
                }
            } else if (pc.sub == 1) {
                auto f = serial_proto::buildPidParam(pc.kp, pc.ki, pc.kd,
                                                     pc.maxDelta, pc.closedLoop);
                serial_.sendFrame(f.data(), f.size());
            } else if (pc.sub == 2) {
                auto f = serial_proto::buildPidTest(pc.testMode, pc.left,
                                                    pc.right, pc.durationMs);
                serial_.sendFrame(f.data(), f.size());
            }
        }
    }

    // 5) 更新状态
    std::lock_guard<std::mutex> lk(mtx_);
    // CAN 健康快照（本控制线程刚 poll 过 can_，此处在 mtx_ 下拷贝供其它线程无锁读，避免竞争）
    if (useCan_) canHealth_ = can_.health();
    if (estop_)                                     status_ = RobotStatus::EStop;
    else if (mode == serial_proto::MODE_MANUAL)     status_ = (speed != 0) ? RobotStatus::Running : RobotStatus::Idle;
    else                                            status_ = RobotStatus::Running;
}

void RobotController::handleCommand(const net::Command& cmd) {
    switch (cmd.cmdId) {
        case net::CMD_MODE:
            setMode(cmd.value);
            break;
        case net::CMD_ESTOP:
            emergencyStop();
            break;
        // F4 无以下执行器硬件：仅回显状态给上位机，不下发串口
        case net::CMD_FAN:    { std::lock_guard<std::mutex> lk(mtx_); actuators_.fan    = cmd.value != 0; } break;
        case net::CMD_BUZZER: { std::lock_guard<std::mutex> lk(mtx_); actuators_.buzzer = cmd.value != 0; } break;
        case net::CMD_RELAY:  { std::lock_guard<std::mutex> lk(mtx_); actuators_.relay  = cmd.value != 0; } break;
        case net::CMD_LED:    { std::lock_guard<std::mutex> lk(mtx_); actuators_.led    = cmd.value != 0; } break;
        default:
            LOG_WARN("未知下行命令 0x%02X value=%u", cmd.cmdId, cmd.value);
            break;
    }
}

void RobotController::setVision(const net::VisionResult& v) {
    std::lock_guard<std::mutex> lk(mtx_);
    vision_       = v;
    lastVisionMs_ = nowMs();
}

void RobotController::setPidCommand(const net::PidCommand& pc) {
    std::lock_guard<std::mutex> lk(mtx_);
    pidPending_.push_back(pc);
    if (pc.sub == 1)
        LOG_INFO("PID 参数入队: kp=%.3f ki=%.3f kd=%.3f maxD=%u closed=%d",
                 pc.kp, pc.ki, pc.kd, pc.maxDelta, pc.closedLoop ? 1 : 0);
    else
        LOG_INFO("PID 测试入队: mode=%u L=%d R=%d dur=%ums",
                 pc.testMode, pc.left, pc.right, pc.durationMs);
}

void RobotController::onPidTele(const serial_proto::PidTele& t) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (pidTeleQ_.size() >= kPidTeleQMax)               // 上限保护：丢最老
        pidTeleQ_.erase(pidTeleQ_.begin());
    pidTeleQ_.push_back(t);
    ++pidTeleRxCnt_;
}

size_t RobotController::takePidTele(std::vector<serial_proto::PidTele>& out) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (pidTeleQ_.empty()) return 0;
    out.swap(pidTeleQ_);
    pidTeleQ_.clear();
    return out.size();
}

// 龙芯"大脑"综合风险判断（须持 mtx_）：超声波/VL53 距离 / 环境报警 / 热点 / 视觉火焰 取最高。
uint8_t RobotController::computeRiskLocked(uint16_t distCm) const {
    uint8_t distRisk = (distCm != 0 && distCm < av_.stopDistCm) ? 3
                     : (distCm != 0 && distCm < av_.slowDistCm) ? 2 : 0;
    const bool vl53Ok = env_.valid && (env_.flags & serial_proto::ENV_FLAG_VL53_OK) &&
                        env_.vl53_mm != serial_proto::VL53_OUT_OF_RANGE;
    uint16_t laserCm = 0;
    if (vl53Ok) {
        laserCm = vl53Init_ ? static_cast<uint16_t>(std::lround(vl53FiltMm_ / 10.0f))
                            : static_cast<uint16_t>(env_.vl53_mm / 10);
    }
    uint8_t laserRisk = (laserCm != 0 && laserCm < av_.stopDistCm) ? 3
                      : (laserCm != 0 && laserCm < av_.slowDistCm) ? 2 : 0;
    uint8_t envRisk = 0;
    if (env_.valid) {
        if      (env_.alarm == serial_proto::ALARM_LV_FIRE) envRisk = 3;  // 火焰/气体
        else if (env_.alarm == serial_proto::ALARM_LV_WARN) envRisk = 2;  // 障碍确认/警告
    }
    uint8_t thermRisk = thermalHotspot_.load(std::memory_order_relaxed) ? 3 : 0;
    // 上位机视觉：识别到火焰=危险；识别到人=注意（仅 3s 内的新鲜结果参与判断）
    uint8_t visRisk = 0;
    if (lastVisionMs_ && (nowMs() - lastVisionMs_) < 3000) {
        if      (vision_.flags & net::VIS_FIRE)   visRisk = 3;
        else if (vision_.flags & net::VIS_PERSON) visRisk = 1;
    }
    return std::max({distRisk, laserRisk, envRisk, thermRisk, visRisk});
}

std::string RobotController::statusLine() const {
    std::lock_guard<std::mutex> lk(mtx_);
    int distCm  = distInit_ ? static_cast<int>(std::lround(distFiltCm_)) : telem_.dist_cm;
    std::string laser = vl53Init_
        ? std::to_string(static_cast<int>(std::lround(vl53FiltMm_ / 10.0))) + "cm" : "--";
    int maxC10 = thermalMaxC10_.load(std::memory_order_relaxed);
    uint8_t risk = computeRiskLocked(static_cast<uint16_t>(distCm));

    std::string s = std::string("v") + versionString()
        + (useCan_ ? " [CAN]" : " [UART]")
        + " RX[tele=" + std::to_string(telemRxCnt_)
        + " env="     + std::to_string(envRxCnt_)
        + " th="      + std::to_string(thermalRxCnt_)
        + " pid="     + std::to_string(pidTeleRxCnt_) + "]"
        + " dist="    + std::to_string(distCm) + "cm"
        + " laser="   + laser
        + " gas="     + std::to_string(env_.valid ? env_.gas_raw : 0)
        + " risk="    + std::to_string(risk);
    // CAN 链路健康（★ EMI 诊断）：错误帧/bus-off/TEC/REC —— 用数字量化掉线是否被治好。
    // 读 tick 在 mtx_ 下拷贝的快照 canHealth_（不直接读 can_，避免与控制线程 poll 写竞争）。
    if (useCan_) {
        const CanManager::Health& h = canHealth_;
        s += " CAN[err=" + std::to_string(h.errFrames)
           + " boff="    + std::to_string(h.busOff)
           + " tec="     + std::to_string(h.tec)
           + " rec="     + std::to_string(h.rec) + "]";
        bool hbFresh = lastHbMs_ && (nowMs() - lastHbMs_) < 3000;
        s += std::string(" hb=") + (hbFresh ? "ok" : "--");
    }
    if (maxC10 > -1000) s += " hot=" + std::to_string(maxC10 / 10) + "C";
    if (lastVisionMs_ && (nowMs() - lastVisionMs_) < 3000 && vision_.count)
        s += " vis=" + vision_.topClass + "(" + std::to_string(vision_.maxConf) + "%)";
    return s;
}

RobotController::LinkStatus RobotController::linkStatus() const {
    std::lock_guard<std::mutex> lk(mtx_);
    LinkStatus r;
    r.f4Open       = useCan_ ? can_.isOpen() : serial_.isOpen();
    r.telemCnt     = telemRxCnt_;
    r.envCnt       = envRxCnt_;
    r.thermalCnt   = thermalRxCnt_;
    uint32_t now   = nowMs();
    r.telemFresh   = lastTelemMs_ && (now - lastTelemMs_) < 2000;
    r.envFresh     = lastEnvMs_   && (now - lastEnvMs_)   < 2000;
    r.visionActive = lastVisionMs_ && (now - lastVisionMs_) < 3000 && vision_.count;
    r.visionCount  = vision_.count;
    r.visionConf   = vision_.maxConf;
    r.visionName   = vision_.topClass;
    r.thermalMaxC10 = thermalMaxC10_.load(std::memory_order_relaxed);
    r.hotspot      = thermalHotspot_.load(std::memory_order_relaxed);
    return r;
}

void RobotController::setManual(int16_t speed, int16_t steering) {
    std::lock_guard<std::mutex> lk(mtx_);
    manualSpeed_ = speed; manualSteer_ = steering;
}

void RobotController::driveManual(int16_t speed, int16_t steering) {
    std::lock_guard<std::mutex> lk(mtx_);
    mode_        = serial_proto::MODE_MANUAL;
    estop_       = false;
    manualSpeed_ = speed;
    manualSteer_ = steering;
}

void RobotController::setMode(uint8_t mode) {
    std::lock_guard<std::mutex> lk(mtx_);
    mode_  = mode & 0x03;
    estop_ = false;                 // 切换模式解除急停
    if (mode_ == serial_proto::MODE_MANUAL) { manualSpeed_ = 0; manualSteer_ = 0; }
    // 模式联动巡检启停：进循迹类模式即启动 F4 本地循线，回遥控即停止。
    // ★ 只置 pending，真正的 CAN 下发在 tick()（控制线程独占 socket）。
    if (mode_ == serial_proto::MODE_LINE_ONLY || mode_ == serial_proto::MODE_LINE_VISION)
        patrolCmdVal_ = can_proto::PC_START;
    else
        patrolCmdVal_ = can_proto::PC_STOP;
    patrolCmdPending_ = true;
    LOG_INFO("模式切换 -> %s (%u)", modeName(mode_), mode_);
}

void RobotController::emergencyStop() {
    std::lock_guard<std::mutex> lk(mtx_);
    estop_ = true;
    manualSpeed_ = 0; manualSteer_ = 0;
    status_ = RobotStatus::EStop;
    // 急停同时终止 F4 侧可能进行中的 PID 测试激励（0x61 mode=0）
    net::PidCommand stop;
    stop.sub = 2; stop.testMode = 0;
    pidPending_.push_back(stop);
    LOG_WARN("紧急停止触发！（含 PID 测试终止）");
}

void RobotController::fillSensorData(net::SensorData& s) const {
    std::lock_guard<std::mutex> lk(mtx_);
    s.timestamp_ms = lastTelemMs_ ? lastTelemMs_ : nowMs();
    // 距离上报滤波后的稳定值（无有效读数时回落到原始）
    s.distance_cm  = distInit_ ? static_cast<uint16_t>(std::lround(distFiltCm_)) : telem_.dist_cm;
    s.encoder1     = telem_.enc1;
    s.encoder2     = telem_.enc2;
    // 速度上报由编码器实测的轮速（counts/s），比 F4 回显的设定值更真实
    s.speed_L      = encInit_ ? encSpeed1_ : telem_.speed;
    s.speed_R      = encInit_ ? encSpeed2_ : telem_.speed;
    s.servo_us     = actuators_.servo_us;
    s.mode         = mode_;
    s.fault        = (telemValid_ && (nowMs() - lastTelemMs_) > 1000) ? 1 : 0;  // 遥测超时告警
    s.fan          = actuators_.fan    ? 1 : 0;
    s.relay        = actuators_.relay  ? 1 : 0;
    s.led          = actuators_.led    ? 1 : 0;

    // ---- 环境/安全传感器（来自 F4 0x5C 帧）----
    const bool envValid = env_.valid;
    if (envValid) {
        s.gas_ppm = env_.gas_raw;                 // MQ2 原始 ADC（0~4095，非真实 ppm）
        s.flags   = env_.flags;                   // 位: 气体/火焰/DHT/VL53/障碍/蜂鸣
        if (env_.flags & serial_proto::ENV_FLAG_DHT_OK) {
            s.temperature_01c = static_cast<int16_t>(env_.temp_c) * 10;   // 0.1°C
            s.humidity_01     = static_cast<uint16_t>(env_.humi) * 10;    // 0.1 %
        }
        // 激光测距（VL53L0X）：有效则换算为厘米（v3 扩展字段），上报滤波后的稳定值
        s.laser_cm = vl53Init_ ? static_cast<uint16_t>(std::lround(vl53FiltMm_ / 10.0))
                   : (env_.vl53_mm != serial_proto::VL53_OUT_OF_RANGE)
                       ? static_cast<uint16_t>(env_.vl53_mm / 10) : 0;
    }
    // 蜂鸣器：F4 实鸣状态 或 上位机执行器回显
    s.buzzer = ((envValid && (env_.flags & serial_proto::ENV_FLAG_BUZZER)) ||
                actuators_.buzzer) ? 1 : 0;

    // ---- 风险等级：龙芯"大脑"综合判断（距离/环境/热点/视觉）----
    s.risk_level = computeRiskLocked(s.distance_cm);
}

RobotMode RobotController::mode() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return static_cast<RobotMode>(mode_);
}

} // namespace patrol
