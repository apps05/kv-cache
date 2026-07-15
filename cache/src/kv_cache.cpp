#include "kv_cache.h"
#include <functional>

KVCache::KVCache(size_t numShards, size_t maxEntriesPerShard) 
    : numShards_(numShards) {
    for (size_t i = 0; i < numShards_; ++i) {
        shards_.push_back(std::make_unique<Shard>(maxEntriesPerShard, stats_));
    }
}

size_t KVCache::shardFor(const std::string& key) const {
    return std::hash<std::string>{}(key) % numShards_;
}

bool KVCache::get(const std::string& key, std::string& outValue) {
    return shards_[shardFor(key)]->get(key, outValue);
}

void KVCache::set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds) {
    shards_[shardFor(key)]->set(key, value, ttlSeconds);
}

bool KVCache::incr(const std::string& key, std::optional<int> ttlSeconds, int64_t& outValue) {
    return shards_[shardFor(key)]->incr(key, ttlSeconds, outValue);
}

bool KVCache::del(const std::string& key) {
    return shards_[shardFor(key)]->del(key);
}

Stats KVCache::getStats() const {
    
    Stats currentStats;
    currentStats.hits.store(stats_.hits.load(std::memory_order_relaxed));
    currentStats.misses.store(stats_.misses.load(std::memory_order_relaxed));
    currentStats.evictions.store(stats_.evictions.load(std::memory_order_relaxed));
    currentStats.expirations.store(stats_.expirations.load(std::memory_order_relaxed));
    return currentStats;
}

void KVCache::sweepExpiredAll() {
    for (auto& shard : shards_) {
        shard->sweepExpired();
    }
}
