#ifndef KV_CACHE_H
#define KV_CACHE_H

#include "shard.h"
#include "stats.h"
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <functional>

class KVCache {
public:
    KVCache(size_t numShards, size_t maxEntriesPerShard);

    bool get(const std::string& key, std::string& outValue);
    void set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds = std::nullopt);
    bool incr(const std::string& key, std::optional<int> ttlSeconds, int64_t& outValue);
    bool del(const std::string& key);

    Stats getStats() const;
    void sweepExpiredAll();

private:
    std::vector<std::unique_ptr<Shard>> shards_;
    size_t numShards_;
    mutable Stats stats_;

    size_t shardFor(const std::string& key) const;
};

#endif 
