#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>

namespace pwow {
namespace core {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& getInstance();

    void log(LogLevel level, const std::string& message);
    void setLogLevel(LogLevel level);

    template<typename... Args>
    void debug(Args&&... args) {
        log(LogLevel::DEBUG, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(LogLevel::INFO, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warning(Args&&... args) {
        log(LogLevel::WARNING, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(LogLevel::ERROR, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void fatal(Args&&... args) {
        log(LogLevel::FATAL, format(std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template<typename... Args>
    std::string format(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args);
        return oss.str();
    }

    LogLevel minLevel = LogLevel::INFO;  // Changed from DEBUG to reduce log spam
    std::mutex mutex;
    std::ofstream fileStream;
    bool fileReady = false;
    void ensureFile();
};

// Convenience macros
#define LOG_DEBUG(...) pwow::core::Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...) pwow::core::Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARNING(...) pwow::core::Logger::getInstance().warning(__VA_ARGS__)
#define LOG_ERROR(...) pwow::core::Logger::getInstance().error(__VA_ARGS__)
#define LOG_FATAL(...) pwow::core::Logger::getInstance().fatal(__VA_ARGS__)

} // namespace core
} // namespace pwow
