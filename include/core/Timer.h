#ifndef PATROL_CORE_TIMER_H
#define PATROL_CORE_TIMER_H

#include <chrono>
#include <functional>
#include <cstdint>

namespace patrol {

// 单次 / 周期定时器。在主循环中调用 check()，到期时触发回调。线程不安全。
class Timer {
public:
    using Callback = std::function<void()>;

    Timer() = default;
    Timer(uint32_t intervalMs, Callback cb, bool repeat = false)
        : interval_(intervalMs), cb_(std::move(cb)), repeat_(repeat) {}

    void setInterval(uint32_t ms) { interval_ = ms; }
    void setCallback(Callback cb) { cb_ = std::move(cb); }
    void setRepeat(bool r)        { repeat_ = r; }

    void start() { last_ = std::chrono::steady_clock::now(); running_ = true; }
    void stop()  { running_ = false; }
    void reset() { last_ = std::chrono::steady_clock::now(); }
    bool running() const { return running_; }

    // 主循环中调用；回调触发时返回 true
    bool check() {
        if (!running_) return false;
        auto now = std::chrono::steady_clock::now();
        uint32_t elapsed = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count());
        if (elapsed < interval_) return false;
        last_ = now;
        if (!repeat_) running_ = false;
        if (cb_) cb_();
        return true;
    }

private:
    uint32_t   interval_ = 1000;
    Callback   cb_;
    bool       repeat_  = false;
    bool       running_ = false;
    std::chrono::steady_clock::time_point last_;
};

} // namespace patrol

#endif // PATROL_CORE_TIMER_H