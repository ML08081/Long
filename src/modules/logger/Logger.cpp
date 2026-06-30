#include "modules/logger/Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace patrol {

namespace {
std::mutex g_mutex;
LogLevel   g_level = LogLevel::Info;

const char* levelTag(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

const char* baseName(const char* path) {
    const char* base = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}
} // namespace

void Logger::setLevel(LogLevel level) { g_level = level; }
LogLevel Logger::level() { return g_level; }

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (static_cast<int>(level) < static_cast<int>(g_level)) return;

    char ts[16];
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d",
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    char msg[4096];   // 原 1024，扩大到 4096 防止长消息截断
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lk(g_mutex);
    std::fprintf(stderr, "[%s][%s][%s:%d] %s\n",
                 ts, levelTag(level), baseName(file), line, msg);
    std::fflush(stderr);
}

} // namespace patrol