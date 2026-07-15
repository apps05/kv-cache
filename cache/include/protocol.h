#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "stats.h"
#include <string>
#include <optional>

enum class CommandType { GET, SET, DEL, STATS, INCR, UNKNOWN };

struct ParsedCommand {
    CommandType type;
    std::string key;
    std::string value;
    std::optional<int> ttlSeconds;
};

ParsedCommand parseCommand(const std::string& line);
std::string formatOk();
std::string formatValue(const std::string& value);
std::string formatMiss();
std::string formatInt(long long n);
std::string formatStats(const Stats& stats);
std::string formatError(const std::string& msg);

#endif 
