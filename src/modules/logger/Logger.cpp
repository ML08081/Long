#include "modules/logger/Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace patrol {

namespace {
std::mutex g_mutex;
LogLevel   g_level = LogLevel::Info;
std::FILE* g_file  = nullptr;   // 可选日志文件

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

bool Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
    if (path.empty()) return true;
    g_file = std::fopen(path.c_str(), "a");   // 追加写入
    return g_file != nullptr;
}

void Logger::closeLogFile() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
}

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (static_cast<int>(level) < static_cast<int>(g_level)) return;

    // 完整日期时间戳：YYYY-MM-DD HH:MM:SS（便于日志归档整理）
    char ts[80];
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lk(g_mutex);
    // 输出到 stderr（systemd journald 捕获）
    std::fprintf(stderr, "[%s][%s][%s:%d] %s\n",
                 ts, levelTag(level), baseName(file), line, msg);
    std::fflush(stderr);
    // 同时落盘到日志文件
    if (g_file) {
        std::fprintf(g_file, "[%s][%s][%s:%d] %s\n",
                     ts, levelTag(level), baseName(file), line, msg);
        std::fflush(g_file);
    }
}

} // namespace patrol