#include "business/RobotController.h"
#include "modules/logger/Logger.h"
#include "utils/TimeUtil.h"

namespace patrol {

bool RobotController::setSpeed(int16_t leftMmS, int16_t rightMmS) {
    state_.sensors.speed_L = leftMmS;
    state_.sensors.speed_R = rightMmS;
    LOG_DEBUG("setSpeed L=%d R=%d mm/s (串口待接入)", leftMmS, rightMmS);
    // TODO: serial_.sendCmd(CMD_SET_SPEED, ...)
    return false;
}

bool RobotController::setFan(bool on) {
    state_.actuators.fan = on;
    LOG_DEBUG("setFan %s", on ? "ON" : "OFF");
    return false;
}

bool RobotController::setBuzzer(bool on) {
    state_.actuators.buzzer = on;
    return false;
}

bool RobotController::setRelay(bool on) {
    state_.actuators.relay = on;
    return false;
}

bool RobotController::setLed(bool on) {
    state_.actuators.led = on;
    return false;
}

bool RobotController::setServo(uint16_t pulseUs) {
    state_.actuators.servo_us = pulseUs;
    return false;
}

bool RobotController::emergencyStop() {
    state_.status = RobotStatus::EStop;
    setSpeed(0, 0);
    LOG_WARN("紧急停止触发！");
    return false;
}

void RobotController::handleCommand(const net::Command& cmd) {
    switch (cmd.cmdId) {
        case net::CMD_FAN:    setFan(cmd.value != 0);    break;
        case net::CMD_BUZZER: setBuzzer(cmd.value != 0); break;
        case net::CMD_RELAY:  setRelay(cmd.value != 0);  break;
        case net::CMD_LED:    setLed(cmd.value != 0);    break;
        case net::CMD_MODE:
            state_.mode = static_cast<RobotMode>(cmd.value);
            LOG_INFO("模式切换 -> %u", cmd.value);
            break;
        case net::CMD_ESTOP:
            emergencyStop();
            break;
        default:
            LOG_WARN("未知命令 0x%02X value=%u", cmd.cmdId, cmd.value);
            break;
    }
}

} // namespace patrol