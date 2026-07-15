#ifndef SHARD_H
#define SHARD_H

#include "lru_list.h"
#include "stats.h"
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <optional>

struct CacheEntry {
    std::string value;
    LRUNode* lruNode;
    std::chrono::steady_clock::time_point expiresAt;
};

class Shard {
public:
    explicit Shard(size_t maxEntries, Stats& globalStats);

    bool get(const std::string& key, std::string& outValue);
    void set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds);
    bool incr(const std::string& key, std::optional<int> ttlSeconds, int64_t& outValue);
    bool del(const std::string& key);
    size_t sweepExpired();

    size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    size_t maxEntries_;
    Stats& stats_;
    std::unordered_map<std::string, CacheEntry> map_;
    LRUList lruList_;

    void evictIfFull();
    bool isExpired(const CacheEntry& entry) const;
};

#endif 
