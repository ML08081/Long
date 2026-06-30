#ifndef PATROL_CORE_EVENTBUS_H
#define PATROL_CORE_EVENTBUS_H

#include <functional>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <cstddef>

namespace patrol {

// 同步发布/订阅事件总线，线程安全。
class EventBus {
public:
    using Handler = std::function<void(const std::string&, const void*, size_t)>;

    int subscribe(const std::string& event, Handler h) {
        std::lock_guard<std::mutex> lk(mu_);
        int id = nextId_++;
        subs_[event].push_back({id, std::move(h)});
        return id;
    }

    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& kv : subs_) {
            auto& v = kv.second;
            for (size_t i = 0; i < v.size(); ++i) {
                if (v[i].id == id) { v.erase(v.begin() + static_cast<long>(i)); return; }
            }
        }
    }

    void publish(const std::string& event, const void* data = nullptr, size_t size = 0) {
        std::vector<Handler> hs;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = subs_.find(event);
            if (it == subs_.end()) return;
            for (auto& s : it->second) hs.push_back(s.handler);
        }
        for (auto& h : hs) h(event, data, size);
    }

    template <typename T>
    void publish(const std::string& event, const T& payload) {
        publish(event, &payload, sizeof(T));
    }

    static EventBus& global() { static EventBus inst; return inst; }

private:
    struct Sub { int id; Handler handler; };
    std::mutex mu_;
    std::map<std::string, std::vector<Sub>> subs_;
    int nextId_ = 0;
};

} // namespace patrol

#endif // PATROL_CORE_EVENTBUS_H