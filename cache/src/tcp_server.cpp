#include "tcp_server.h"
#include "protocol.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

TcpServer::TcpServer(int port, KVCache& cache) 
    : port_(port), listenFd_(-1), cache_(cache), running_(false) {}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::start() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        Logger::logError("Failed to create socket");
        return;
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(listenFd_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::logError("Failed to bind socket");
        close(listenFd_);
        listenFd_ = -1;
        return;
    }

    if (listen(listenFd_, 1024) < 0) {
        Logger::logError("Failed to listen on socket");
        close(listenFd_);
        listenFd_ = -1;
        return;
    }

    running_ = true;
    Logger::log("Server listening on port " + std::to_string(port_));
    
    
    for (int i = 0; i < 50; ++i) {
        workerThreads_.emplace_back(&TcpServer::workerLoop, this);
    }
    
    acceptThread_ = std::thread(&TcpServer::acceptLoop, this);
}

void TcpServer::stop() {
    running_ = false;
    if (listenFd_ >= 0) {
        close(listenFd_);
        listenFd_ = -1;
    }
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    
    
    queueCond_.notify_all();
    
    for (auto& th : workerThreads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    workerThreads_.clear();
}

void TcpServer::acceptLoop() {
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientFd < 0) {
            if (running_) {
                Logger::logError("Accept failed");
            }
            continue;
        }

        int flag = 1;
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            clientQueue_.push(clientFd);
        }
        queueCond_.notify_one();
    }
}

void TcpServer::workerLoop() {
    while (running_) {
        int clientFd = -1;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCond_.wait(lock, [this]() { return !running_ || !clientQueue_.empty(); });
            if (!running_ && clientQueue_.empty()) {
                return;
            }
            clientFd = clientQueue_.front();
            clientQueue_.pop();
        }
        
        if (clientFd >= 0) {
            handleClient(clientFd);
        }
    }
}

void TcpServer::handleClient(int clientFd) {
    char buffer[1024];
    std::string currentLine;
    const size_t MAX_LINE_LENGTH = 131072; 
    
    while (running_) {
        ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            break; 
        }
        
        for (ssize_t i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\n') {
                if (!currentLine.empty() && currentLine.back() == '\r') {
                    currentLine.pop_back();
                }
                
                ParsedCommand cmd = parseCommand(currentLine);
                std::string response;
                
                if (cmd.type == CommandType::GET) {
                    std::string value;
                    if (cache_.get(cmd.key, value)) {
                        response = formatValue(value);
                    } else {
                        response = formatMiss();
                    }
                } else if (cmd.type == CommandType::SET) {
                    cache_.set(cmd.key, cmd.value, cmd.ttlSeconds);
                    response = formatOk();
                } else if (cmd.type == CommandType::DEL) {
                    if (cache_.del(cmd.key)) {
                        response = formatInt(1);
                    } else {
                        response = formatInt(0);
                    }
                } else if (cmd.type == CommandType::INCR) {
                    int64_t newVal;
                    if (cache_.incr(cmd.key, cmd.ttlSeconds, newVal)) {
                        response = formatInt(newVal);
                    } else {
                        response = formatError("ERR value is not an integer or out of range");
                    }
                } else if (cmd.type == CommandType::STATS) {
                    response = formatStats(cache_.getStats());
                } else {
                    response = formatError("UNKNOWN COMMAND");
                }
                
                size_t totalWritten = 0;
                while (totalWritten < response.size()) {
                    ssize_t written = write(clientFd, response.c_str() + totalWritten, response.size() - totalWritten);
                    if (written < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EPIPE || errno == ECONNRESET) break;
                        break;
                    }
                    if (written == 0) break;
                    totalWritten += written;
                }
                
                currentLine.clear();
            } else {
                currentLine += buffer[i];
                if (currentLine.size() > MAX_LINE_LENGTH) {
                    Logger::logError("Client exceeded max line length");
                    close(clientFd);
                    return;
                }
            }
        }
    }
    close(clientFd);
}
