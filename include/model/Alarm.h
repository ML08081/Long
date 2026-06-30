#ifndef PATROL_MODEL_ALARM_H
#define PATROL_MODEL_ALARM_H

#include <cstdint>
#include <string>

namespace patrol {

enum class AlarmLevel : uint8_t {
    Info     = 0,
    Warning  = 1,
    Error    = 2,
    Critical = 3,
};

enum class AlarmType : uint8_t {
    Temperature  = 0x01,
    Gas          = 0x02,
    Voltage      = 0x03,
    Obstacle     = 0x04,
    CameraFault  = 0x05,
    NetworkFault = 0x06,
    SerialFault  = 0x07,
    MotorFault   = 0x08,
    EStop        = 0xFF,
};

struct Alarm {
    AlarmType   type         = AlarmType::Temperature;
    AlarmLevel  level        = AlarmLevel::Info;
    uint32_t    timestamp_ms = 0;
    std::string message;
    bool        active       = true;
};

} // namespace patrol

#endif // PATROL_MODEL_ALARM_H