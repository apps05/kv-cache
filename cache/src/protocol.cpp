#include "protocol.h"
#include <sstream>
#include <vector>

static std::vector<std::string> tokenize(const std::string& str) {
    std::vector<std::string> tokens;
    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;
    std::string currentToken;
    
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
        } else if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
        } else if (std::isspace(c) && !inDoubleQuotes && !inSingleQuotes) {
            if (!currentToken.empty()) {
                tokens.push_back(currentToken);
                currentToken.clear();
            }
        } else {
            currentToken += c;
        }
    }
    if (!currentToken.empty()) {
        tokens.push_back(currentToken);
    }
    return tokens;
}

ParsedCommand parseCommand(const std::string& line) {
    ParsedCommand cmd;
    cmd.type = CommandType::UNKNOWN;

    auto tokens = tokenize(line);
    if (tokens.empty()) return cmd;

    if (tokens[0] == "GET" && tokens.size() == 2) {
        cmd.type = CommandType::GET;
        cmd.key = tokens[1];
    } else if (tokens[0] == "DEL" && tokens.size() == 2) {
        cmd.type = CommandType::DEL;
        cmd.key = tokens[1];
    } else if (tokens[0] == "STATS" && tokens.size() == 1) {
        cmd.type = CommandType::STATS;
    } else if (tokens[0] == "SET" && tokens.size() >= 3) {
        cmd.type = CommandType::SET;
        cmd.key = tokens[1];
        cmd.value = tokens[2];
        if (tokens.size() == 5 && tokens[3] == "EX") {
            try {
                cmd.ttlSeconds = std::stoi(tokens[4]);
            } catch (...) {
                cmd.type = CommandType::UNKNOWN;
            }
        }
    } else if (tokens[0] == "INCR" && tokens.size() >= 2) {
        cmd.type = CommandType::INCR;
        cmd.key = tokens[1];
        if (tokens.size() == 4 && tokens[2] == "EX") {
            try {
                cmd.ttlSeconds = std::stoi(tokens[3]);
            } catch (...) {
                cmd.type = CommandType::UNKNOWN;
            }
        } else if (tokens.size() > 2) {
            cmd.type = CommandType::UNKNOWN;
        }
    }

    return cmd;
}

std::string formatOk() {
    return "+OK\r\n";
}

std::string formatValue(const std::string& value) {
    return "$" + value + "\r\n";
}

std::string formatMiss() {
    return "$-1\r\n";
}

std::string formatInt(long long n) {
    return ":" + std::to_string(n) + "\r\n";
}

std::string formatStats(const Stats& stats) {
    return "+" + stats.format() + "\r\n";
}

std::string formatError(const std::string& msg) {
    return "-" + msg + "\r\n";
}
