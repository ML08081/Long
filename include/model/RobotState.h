#ifndef PATROL_MODEL_ROBOTSTATE_H
#define PATROL_MODEL_ROBOTSTATE_H

#include <cstdint>

namespace patrol {

// 与 FrameProtocol CMD_MODE 值对应
enum class RobotMode : uint8_t {
    Manual   = 0,
    Auto     = 1,
    Obstacle = 2,
    Patrol   = 3,
};

enum class RobotStatus : uint8_t {
    Idle    = 0,
    Running = 1,
    Paused  = 2,
    Error   = 3,
    EStop   = 4,
};

struct SensorReading {
    uint32_t timestamp_ms    = 0;
    int16_t  temperature_01c = 0;   // 0.1 degC
    uint16_t humidity_01     = 0;   // 0.1 %
    uint16_t gas_ppm         = 0;
    uint32_t pressure_pa     = 0;
    uint16_t distance_cm     = 0;
    int32_t  encoder1        = 0;
    int32_t  encoder2        = 0;
    int16_t  speed_L         = 0;
    int16_t  speed_R         = 0;
    uint16_t voltage_mV      = 0;
};

struct ActuatorState {
    bool     fan      = false;
    bool     buzzer   = false;
    bool     relay    = false;
    bool     led      = false;
    uint16_t servo_us = 1500;
};

struct RobotState {
    RobotMode     mode       = RobotMode::Manual;
    RobotStatus   status     = RobotStatus::Idle;
    uint8_t       fault      = 0;
    uint8_t       risk_level = 0;
    SensorReading sensors;
    ActuatorState actuators;
};

} // namespace patrol

#endif // PATROL_MODEL_ROBOTSTATE_H