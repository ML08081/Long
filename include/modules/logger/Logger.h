#ifndef PATROL_MODULES_LOGGER_LOGGER_H
#define PATROL_MODULES_LOGGER_LOGGER_H

#include <string>

// =============================================================================
//  Logger — 轻量级日志（输出到 stderr，带时间戳与级别，线程安全）
//
//  用法：
//     LOG_INFO("摄像头已打开: %s", dev.c_str());
//     LOG_ERROR("发送失败: %s", strerror(errno));
//
//  说明：当前阶段仅提供控制台日志，后续可扩展文件落盘 / 等级过滤。
// =============================================================================

namespace patrol {

enum class LogLevel { Debug = 0, Info, Warn, Error };

class Logger {
public:
    // 设置全局最低输出级别（低于此级别的日志被丢弃）
    static void setLevel(LogLevel level);
    static LogLevel level();

    // printf 风格输出；通常通过下方宏调用以自动带上文件:行号
    static void log(LogLevel level, const char* file, int line, const char* fmt, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 4, 5)))
#endif
        ;
};

} // namespace patrol

#define LOG_DEBUG(...) ::patrol::Logger::log(::patrol::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::patrol::Logger::log(::patrol::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::patrol::Logger::log(::patrol::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::patrol::Logger::log(::patrol::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

#endif // PATROL_MODULES_LOGGER_LOGGER_H
