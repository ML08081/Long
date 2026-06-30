#ifndef PATROL_BUSINESS_PATROLCONTROLLER_H
#define PATROL_BUSINESS_PATROLCONTROLLER_H

#include "model/Task.h"
#include <vector>
#include <functional>

namespace patrol {

// 巡检路径执行控制。待运动控制串口就绪后实现。
class PatrolController {
public:
    using DoneCallback = std::function<void(bool success)>;

    PatrolController() = default;

    bool start(const std::vector<Waypoint>& path, DoneCallback cb = nullptr);
    void pause();
    void resume();
    void abort();

    bool   isRunning()       const { return running_; }
    size_t currentWaypoint() const { return currentWp_; }

    void tick();   // 主循环调用

private:
    std::vector<Waypoint> path_;
    size_t      currentWp_ = 0;
    bool        running_   = false;
    DoneCallback doneCb_;
};

} // namespace patrol

#endif // PATROL_BUSINESS_PATROLCONTROLLER_H