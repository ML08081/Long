#ifndef PATROL_CORE_TASKSCHEDULER_H
#define PATROL_CORE_TASKSCHEDULER_H

#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace patrol {

// 协作式周期任务调度器。在主循环中调用 tick()。
class TaskScheduler {
public:
    using TaskFn = std::function<void()>;

    int addTask(uint32_t intervalMs, TaskFn fn, std::string name = "") {
        std::lock_guard<std::mutex> lk(mu_);
        int id = nextId_++;
        tasks_.push_back({id, intervalMs, std::move(fn), std::move(name),
                          std::chrono::steady_clock::now()});
        return id;
    }

    void removeTask(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (size_t i = 0; i < tasks_.size(); ++i) {
            if (tasks_[i].id == id) { tasks_.erase(tasks_.begin()+static_cast<long>(i)); return; }
        }
    }

    void tick() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& t : tasks_) {
            uint32_t ms = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - t.last).count());
            if (ms >= t.intervalMs) { t.last = now; if (t.fn) t.fn(); }
        }
    }

private:
    struct Task {
        int id; uint32_t intervalMs; TaskFn fn; std::string name;
        std::chrono::steady_clock::time_point last;
    };
    std::mutex mu_;
    std::vector<Task> tasks_;
    int nextId_ = 0;
};

} // namespace patrol

#endif // PATROL_CORE_TASKSCHEDULER_H