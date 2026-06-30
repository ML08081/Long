#ifndef PATROL_UTILS_TIMEUTIL_H
#define PATROL_UTILS_TIMEUTIL_H

#include <cstdint>
#include <ctime>
#include <cstdio>
#include <string>
#include <chrono>

namespace patrol {
namespace time_util {

inline uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

inline uint64_t nowUs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

// 格式化当前时间为 "YYYY-MM-DD HH:MM:SS"
inline std::string nowStr() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

} // namespace time_util
} // namespace patrol

#endif // PATROL_UTILS_TIMEUTIL_H