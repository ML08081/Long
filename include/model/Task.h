#ifndef PATROL_MODEL_TASK_H
#define PATROL_MODEL_TASK_H

#include <cstdint>
#include <string>
#include <vector>

namespace patrol {

enum class TaskType : uint8_t {
    Patrol   = 0x01,
    Goto     = 0x02,
    Scan     = 0x03,
    Return   = 0x04,
    Idle     = 0xFF,
};

enum class TaskStatus : uint8_t {
    Pending   = 0,
    Running   = 1,
    Paused    = 2,
    Completed = 3,
    Failed    = 4,
    Cancelled = 5,
};

struct Waypoint {
    float    x           = 0.0f;
    float    y           = 0.0f;
    float    heading_deg = 0.0f;
    uint32_t dwell_ms    = 0;
};

struct Task {
    uint32_t              id          = 0;
    TaskType              type        = TaskType::Idle;
    TaskStatus            status      = TaskStatus::Pending;
    std::vector<Waypoint> waypoints;
    uint32_t              created_ms  = 0;
    uint32_t              started_ms  = 0;
    std::string           description;
};

} // namespace patrol

#endif // PATROL_MODEL_TASK_H