#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <iostream>

class Logger {
public:
    static void log(const std::string& msg);
    static void logError(const std::string& msg);

private:
    static std::mutex mutex_;
};

#endif 
