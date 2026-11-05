#pragma once

#include <string>

namespace neo {

enum class LogLevel {
    Info,
    Warn,
    Error,
    Debug,
};

class Logger {
public:
    static Logger& instance();

    void initialize();
    void write(LogLevel level, const std::string& message);
    void writef(LogLevel level, const char* fmt, ...);

private:
    Logger() = default;
    bool initialized_ = false;
};

#define NEO_LOG_INFO(...) ::neo::Logger::instance().writef(::neo::LogLevel::Info, __VA_ARGS__)
#define NEO_LOG_WARN(...) ::neo::Logger::instance().writef(::neo::LogLevel::Warn, __VA_ARGS__)
#define NEO_LOG_ERROR(...) ::neo::Logger::instance().writef(::neo::LogLevel::Error, __VA_ARGS__)
#define NEO_LOG_DEBUG(...) ::neo::Logger::instance().writef(::neo::LogLevel::Debug, __VA_ARGS__)

} // namespace neo
