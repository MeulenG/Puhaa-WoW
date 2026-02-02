#include "core/logger.hpp"
#include <chrono>
#include <iomanip>
#include <ctime>

namespace wowee {
namespace core {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < minLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_r(&time, &tm);

    // Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
    std::cout << "["
              << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count()
              << "] [";

    switch (level) {
        case LogLevel::DEBUG:   std::cout << "DEBUG"; break;
        case LogLevel::INFO:    std::cout << "INFO "; break;
        case LogLevel::WARNING: std::cout << "WARN "; break;
        case LogLevel::ERROR:   std::cout << "ERROR"; break;
        case LogLevel::FATAL:   std::cout << "FATAL"; break;
    }

    std::cout << "] " << message << std::endl;
}

void Logger::setLogLevel(LogLevel level) {
    minLevel = level;
}

} // namespace core
} // namespace wowee
