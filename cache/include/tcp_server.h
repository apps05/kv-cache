#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "kv_cache.h"
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

class TcpServer {
public:
    TcpServer(int port, KVCache& cache);
    ~TcpServer();

    void start();
    void stop();

private:
    void acceptLoop();
    void workerLoop();
    void handleClient(int clientFd);

    int port_;
    int listenFd_;
    KVCache& cache_;
    std::thread acceptThread_;
    std::atomic<bool> running_;
    
    
    std::vector<std::thread> workerThreads_;
    std::queue<int> clientQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;
};

#endif 
