#ifndef STATS_H
#define STATS_H

#include <atomic>
#include <string>

struct Stats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};

    Stats() = default;
    
    Stats(const Stats& other) {
        hits.store(other.hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
        misses.store(other.misses.load(std::memory_order_relaxed), std::memory_order_relaxed);
        evictions.store(other.evictions.load(std::memory_order_relaxed), std::memory_order_relaxed);
        expirations.store(other.expirations.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    
    Stats& operator=(const Stats& other) {
        if (this != &other) {
            hits.store(other.hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
            misses.store(other.misses.load(std::memory_order_relaxed), std::memory_order_relaxed);
            evictions.store(other.evictions.load(std::memory_order_relaxed), std::memory_order_relaxed);
            expirations.store(other.expirations.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    std::string format() const;
};

#endif 
