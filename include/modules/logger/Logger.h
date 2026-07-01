#ifndef PATROL_MODULES_LOGGER_LOGGER_H
#define PATROL_MODULES_LOGGER_LOGGER_H

#include <string>

// =============================================================================
//  Logger -- 轻量级日志（stderr + 可选文件，带日期时间戳与级别，线程安全）
//
//  用法：
//     LOG_INFO("摄像头已打开: %s", dev.c_str());
//     Logger::setLogFile("/var/log/patrol/patrol.log");  // 同时落盘
// =============================================================================

namespace patrol {

enum class LogLevel { Debug = 0, Info, Warn, Error };

class Logger {
public:
    // 全局最低输出级别（低于此级别的日志被丢弃）
    static void setLevel(LogLevel level);
    static LogLevel level();

    // 设置日志文件（追加写入；同时仍输出到 stderr）。传空字符串关闭文件输出。
    // 返回是否成功打开文件。
    static bool setLogFile(const std::string& path);
    static void closeLogFile();

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