#ifndef PATROL_BUSINESS_ROBOTCONTROLLER_H
#define PATROL_BUSINESS_ROBOTCONTROLLER_H

#include "model/RobotState.h"
#include "modules/network/FrameProtocol.h"
#include <cstdint>

namespace patrol {

// 机器人底层控制：运动 + 执行器。串口模块就绪后对接 SerialManager。
class RobotController {
public:
    RobotController() = default;

    bool setSpeed(int16_t leftMmS, int16_t rightMmS);
    bool setFan(bool on);
    bool setBuzzer(bool on);
    bool setRelay(bool on);
    bool setLed(bool on);
    bool setServo(uint16_t pulseUs);
    bool emergencyStop();

    // 处理来自上位机的下行命令
    void handleCommand(const net::Command& cmd);

    const RobotState& state() const { return state_; }

private:
    RobotState state_;
};

} // namespace patrol

#endif // PATROL_BUSINESS_ROBOTCONTROLLER_H