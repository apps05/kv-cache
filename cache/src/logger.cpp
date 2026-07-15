#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

std::mutex Logger::mutex_;

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[" << currentTimestamp() << "] INFO: " << msg << std::endl;
}

void Logger::logError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "[" << currentTimestamp() << "] ERROR: " << msg << std::endl;
}
