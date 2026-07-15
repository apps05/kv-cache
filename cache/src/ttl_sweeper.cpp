#include "ttl_sweeper.h"
#include <iostream>

TtlSweeper::TtlSweeper(KVCache& cache, int intervalMs)
    : cache_(cache), intervalMs_(intervalMs), running_(false) {}

TtlSweeper::~TtlSweeper() {
    stop();
}

void TtlSweeper::start() {
    if (!running_.exchange(true)) {
        thread_ = std::thread(&TtlSweeper::run, this);
    }
}

void TtlSweeper::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

void TtlSweeper::run() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs_));
        if (!running_) break;
        cache_.sweepExpiredAll();
    }
}
