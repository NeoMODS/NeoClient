#include "core/logger.h"

#include "core/paths.h"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <ctime>
#include <windows.h>

namespace neo {
namespace {

std::mutex g_mutex;
std::ofstream g_file;

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Debug: return "DEBUG";
    }
    return "INFO";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
    return buffer;
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize() {
    if (initialized_) {
        return;
    }

    std::lock_guard lock(g_mutex);
    if (initialized_) {
        return;
    }

    const auto logPath = paths::logFile();
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);

    g_file.open(logPath, std::ios::out | std::ios::app);
    initialized_ = true;
}

void Logger::write(LogLevel level, const std::string& message) {
    initialize();

    std::lock_guard lock(g_mutex);
    const std::string line = "[" + timestamp() + "] [" + levelName(level) + "] " + message + "\n";

    if (g_file.is_open()) {
        g_file << line;
        g_file.flush();
    }

    OutputDebugStringA(line.c_str());
}

void Logger::writef(LogLevel level, const char* fmt, ...) {
    char buffer[2048]{};

    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    write(level, buffer);
}

} // namespace neo
