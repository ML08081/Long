#include "business/RobotController.h"
#include "modules/logger/Logger.h"

#include <chrono>

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

bool RobotController::init(const std::string& device, int baud) {
    if (!serial_.open(device, baud)) {
        LOG_ERROR("RobotController: 串口 %s 打开失败，运动控制不可用", device.c_str());
        return false;
    }
    serial_.setTelemetryCallback([this](const serial_proto::Telemetry& t) { onTelemetry(t); });
    serial_.setThermalCallback(
        [this](const int16_t* t, int c, int r) { onThermal(t, c, r); });
    LOG_INFO("RobotController 就绪（串口 %s @ %d）", device.c_str(), baud);
    return true;
}

void RobotController::close() { serial_.close(); }

void RobotController::onTelemetry(const serial_proto::Telemetry& t) {
    std::lock_guard<std::mutex> lk(mtx_);
    // 扩展帧携带距离/编码器；旧帧仅速度/转向/模式，不覆盖已有距离
    if (t.hasDistance) {
        telem_ = t;
    } else {
        telem_.speed    = t.speed;
        telem_.steering = t.steering;
        telem_.mode     = t.mode;
    }
    telemValid_  = true;
    lastTelemMs_ = nowMs();
}

void RobotController::onThermal(const int16_t* temps, int cols, int rows) {
    std::lock_guard<std::mutex> lk(thermalMtx_);
    thermal_.assign(temps, temps + static_cast<size_t>(cols) * rows);
    thermalCols_ = cols;
    thermalRows_ = rows;
    thermalNew_  = true;
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

    // 2) 取共享状态快照
    uint8_t  mode; int16_t mSpd, mStr; bool estop;
    serial_proto::Telemetry t;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        mode = mode_; mSpd = manualSpeed_; mStr = manualSteer_;
        estop = estop_; t = telem_;
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
                computeAvoid(t.dist_cm, av_.obstacleSpeed, speed, steering);
                break;
            case serial_proto::MODE_AUTO:
            case serial_proto::MODE_CRUISE:
            default:
                computeAvoid(t.dist_cm, av_.cruiseSpeed, speed, steering);
                break;
        }
    }

    // 4) 下发命令帧
    serial_.sendCommand(speed, steering, mode);

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
    LOG_WARN("紧急停止触发！");
}

void RobotController::fillSensorData(net::SensorData& s) const {
    std::lock_guard<std::mutex> lk(mtx_);
    s.timestamp_ms = lastTelemMs_ ? lastTelemMs_ : nowMs();
    s.distance_cm  = telem_.dist_cm;
    s.encoder1     = telem_.enc1;
    s.encoder2     = telem_.enc2;
    s.speed_L      = telem_.speed;   // F4 单速度值，左右回显相同
    s.speed_R      = telem_.speed;
    s.servo_us     = actuators_.servo_us;
    s.mode         = mode_;
    s.fault        = (telemValid_ && (nowMs() - lastTelemMs_) > 1000) ? 1 : 0;  // 遥测超时告警
    s.fan          = actuators_.fan    ? 1 : 0;
    s.buzzer       = actuators_.buzzer ? 1 : 0;
    s.relay        = actuators_.relay  ? 1 : 0;
    s.led          = actuators_.led    ? 1 : 0;

    // 视觉风险等级：由前方距离推导（未接摄像头识别前的占位）
    uint16_t d = telem_.dist_cm;
    if      (d != 0 && d < av_.stopDistCm) s.risk_level = 3;   // 危险
    else if (d != 0 && d < av_.slowDistCm) s.risk_level = 2;   // 警告
    else                                   s.risk_level = 0;   // 安全
}

RobotMode RobotController::mode() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return static_cast<RobotMode>(mode_);
}

} // namespace patrol
