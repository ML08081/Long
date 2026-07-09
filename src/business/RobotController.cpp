#include "business/RobotController.h"
#include "modules/logger/Logger.h"
#include "version.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace patrol {

namespace {
uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
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

bool RobotController::init(const std::string& device, int baud,
                           const std::string& thermalDevice, int thermalBaud) {
    if (!serial_.open(device, baud)) {
        LOG_ERROR("RobotController: 串口 %s 打开失败，运动控制不可用", device.c_str());
        return false;
    }
    serial_.setTelemetryCallback([this](const serial_proto::Telemetry& t) { onTelemetry(t); });
    serial_.setEnvCallback([this](const serial_proto::EnvData& e) { onEnv(e); });
    // 控制口也挂热成像回调，兼容 F4 单串口发送的情形
    serial_.setThermalCallback([this](const int16_t* t, int c, int r) { onThermal(t, c, r); });
    serial_.setRawThermalCallback([this](const uint16_t* p, int n) { onRawThermal(p, n); });
    serial_.setEepromCallback([this](const uint16_t* p, int n) { onEeprom(p, n); });
    serial_.setPidTeleCallback([this](const serial_proto::PidTele& t) { onPidTele(t); });

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
    LOG_INFO("RobotController 就绪（串口 %s @ %d）", device.c_str(), baud);
    return true;
}

void RobotController::close() { serial_.close(); thermalSerial_.close(); }

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
    // 提取标定参数（一次性）。解算器就绪后原始帧才能解算。
    thermalSolver_.setEeprom(ee);
}

void RobotController::onRawThermal(const uint16_t* raw, int words) {
    if (words < ThermalSolver::FRAME_WORDS) return;
    // 龙芯端解算：原始帧 -> 768 个 int16 温度(0.01°C)。就绪(收到EEPROM)才有效。
    if (thermalSolver_.solve(raw, thermalSolved_))
        onThermal(thermalSolved_, serial_proto::THERMAL_COLS, serial_proto::THERMAL_ROWS);
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
    // 1) 读取串口遥测（同线程回调 onTelemetry）
    serial_.poll();
    thermalSerial_.poll();   // 热成像专线（0x5B → onThermal）；未打开时内部直接返回

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
    {
        std::lock_guard<std::mutex> lk(mtx_);
        mode = mode_; mSpd = manualSpeed_; mStr = manualSteer_;
        estop = estop_; t = telem_;
        avoidDist = distInit_ ? static_cast<uint16_t>(std::lround(distFiltCm_)) : t.dist_cm;
    }

    // 3) 计算运动指令
    int16_t speed = 0, steering = 0;
    if (estop) {
        speed = 0; steering = 0;
    } else {
        switch (mode) {
            case serial_proto::MODE_MANUAL:
                speed = mSpd; steering = mStr;
                break;
            case serial_proto::MODE_OBSTACLE:
                computeAvoid(avoidDist, av_.obstacleSpeed, speed, steering);
                break;
            case serial_proto::MODE_AUTO:
            case serial_proto::MODE_CRUISE:
            default:
                computeAvoid(avoidDist, av_.cruiseSpeed, speed, steering);
                break;
        }
    }

    // 4) 下发命令帧
    serial_.sendCommand(speed, steering, mode);

    // 4.5) 冲刷 PID 调试命令队列（网络线程入队 → 本控制线程独占串口写，保证线程安全）
    {
        std::vector<net::PidCommand> pending;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            pending.swap(pidPending_);
        }
        for (const auto& pc : pending) {
            if (pc.sub == 1) {
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

// 龙芯"大脑"综合风险判断（须持 mtx_）：距离 / 环境报警 / 热点 / 视觉火焰 取最高。
uint8_t RobotController::computeRiskLocked(uint16_t distCm) const {
    uint8_t distRisk = (distCm != 0 && distCm < av_.stopDistCm) ? 3
                     : (distCm != 0 && distCm < av_.slowDistCm) ? 2 : 0;
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
    return std::max({distRisk, envRisk, thermRisk, visRisk});
}

std::string RobotController::statusLine() const {
    std::lock_guard<std::mutex> lk(mtx_);
    int distCm  = distInit_ ? static_cast<int>(std::lround(distFiltCm_)) : telem_.dist_cm;
    std::string laser = vl53Init_
        ? std::to_string(static_cast<int>(std::lround(vl53FiltMm_ / 10.0))) + "cm" : "--";
    int maxC10 = thermalMaxC10_.load(std::memory_order_relaxed);
    uint8_t risk = computeRiskLocked(static_cast<uint16_t>(distCm));

    std::string s = std::string("v") + versionString()
        + " RX[tele=" + std::to_string(telemRxCnt_)
        + " env="     + std::to_string(envRxCnt_)
        + " th="      + std::to_string(thermalRxCnt_)
        + " pid="     + std::to_string(pidTeleRxCnt_) + "]"
        + " dist="    + std::to_string(distCm) + "cm"
        + " laser="   + laser
        + " gas="     + std::to_string(env_.valid ? env_.gas_raw : 0)
        + " risk="    + std::to_string(risk);
    if (maxC10 > -1000) s += " hot=" + std::to_string(maxC10 / 10) + "C";
    if (lastVisionMs_ && (nowMs() - lastVisionMs_) < 3000 && vision_.count)
        s += " vis=" + vision_.topClass + "(" + std::to_string(vision_.maxConf) + "%)";
    return s;
}

RobotController::LinkStatus RobotController::linkStatus() const {
    std::lock_guard<std::mutex> lk(mtx_);
    LinkStatus r;
    r.f4Open       = serial_.isOpen();
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
