#ifndef PATROL_BUSINESS_MISSIONMANAGER_H
#define PATROL_BUSINESS_MISSIONMANAGER_H

#include "model/Task.h"
#include <vector>
#include <functional>
#include <cstdint>

namespace patrol {

// 任务队列管理：优先级调度、状态上报。
class MissionManager {
public:
    using TaskCallback = std::function<void(const Task&)>;

    MissionManager() = default;

    uint32_t  enqueue(Task task);
    bool      cancel(uint32_t taskId);
    const Task* currentTask() const;
    void      onTaskChanged(TaskCallback cb) { onChanged_ = std::move(cb); }
    void      tick();   // 主循环调用

private:
    std::vector<Task> queue_;
    uint32_t          nextId_    = 1;
    TaskCallback      onChanged_;
};

} // namespace patrol

#endif // PATROL_BUSINESS_MISSIONMANAGER_H