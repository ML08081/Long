#include "business/MissionManager.h"
#include "modules/logger/Logger.h"

namespace patrol {

uint32_t MissionManager::enqueue(Task task) {
    task.id     = nextId_++;
    task.status = TaskStatus::Pending;
    queue_.push_back(task);
    LOG_INFO("任务入队 id=%u type=%u desc=%s",
             task.id, static_cast<unsigned>(task.type), task.description.c_str());
    if (onChanged_) onChanged_(queue_.back());
    return task.id;
}

bool MissionManager::cancel(uint32_t taskId) {
    for (auto& t : queue_) {
        if (t.id == taskId && t.status != TaskStatus::Completed) {
            t.status = TaskStatus::Cancelled;
            if (onChanged_) onChanged_(t);
            LOG_INFO("任务 id=%u 已取消", taskId);
            return true;
        }
    }
    return false;
}

const Task* MissionManager::currentTask() const {
    for (const auto& t : queue_)
        if (t.status == TaskStatus::Running) return &t;
    return nullptr;
}

void MissionManager::tick() {
    // TODO: 调度 Pending -> Running，驱动 PatrolController
}

} // namespace patrol