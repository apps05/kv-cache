#ifndef TTL_SWEEPER_H
#define TTL_SWEEPER_H

#include "kv_cache.h"
#include <thread>
#include <atomic>
#include <chrono>

class TtlSweeper {
public:
    TtlSweeper(KVCache& cache, int intervalMs = 1000);
    ~TtlSweeper();

    void start();
    void stop();

private:
    void run();

    KVCache& cache_;
    int intervalMs_;
    std::atomic<bool> running_;
    std::thread thread_;
};

#endif 
