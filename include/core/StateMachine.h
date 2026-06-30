#ifndef PATROL_CORE_STATEMACHINE_H
#define PATROL_CORE_STATEMACHINE_H

#include <functional>
#include <map>
#include <mutex>

namespace patrol {

// 泛型扁平状态机。State/Event 使用枚举整型。
template <typename State, typename Event>
class StateMachine {
public:
    using Action = std::function<void(State from, Event ev, State to)>;
    using Guard  = std::function<bool(State from, Event ev)>;

    explicit StateMachine(State initial) : cur_(initial) {}

    State current() const { std::lock_guard<std::mutex> lk(mu_); return cur_; }

    void addTransition(State from, Event ev, State to,
                       Action action = nullptr, Guard guard = nullptr) {
        Key k{from, ev};
        trans_[k] = {to, std::move(action), std::move(guard)};
    }

    void onEnter(State s, std::function<void()> cb) { enterCb_[s] = std::move(cb); }
    void onExit (State s, std::function<void()> cb) { exitCb_[s]  = std::move(cb); }

    // 返回 true 表示发生了跳转
    bool process(Event ev) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = trans_.find(Key{cur_, ev});
        if (it == trans_.end()) return false;
        const Info& t = it->second;
        if (t.guard && !t.guard(cur_, ev)) return false;
        State prev = cur_;
        auto eit = exitCb_.find(prev); if (eit != exitCb_.end() && eit->second) eit->second();
        cur_ = t.to;
        if (t.action) t.action(prev, ev, cur_);
        auto nit = enterCb_.find(cur_); if (nit != enterCb_.end() && nit->second) nit->second();
        return true;
    }

private:
    struct Key {
        State from; Event ev;
        bool operator<(const Key& o) const {
            if (static_cast<int>(from) != static_cast<int>(o.from))
                return static_cast<int>(from) < static_cast<int>(o.from);
            return static_cast<int>(ev) < static_cast<int>(o.ev);
        }
    };
    struct Info { State to; Action action; Guard guard; };
    mutable std::mutex mu_;
    State cur_;
    std::map<Key, Info> trans_;
    std::map<State, std::function<void()>> enterCb_, exitCb_;
};

} // namespace patrol

#endif // PATROL_CORE_STATEMACHINE_H