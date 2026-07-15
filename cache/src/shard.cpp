#include "shard.h"
#include <mutex>

Shard::Shard(size_t maxEntries, Stats& globalStats) 
    : maxEntries_(maxEntries), stats_(globalStats) {}

bool Shard::isExpired(const CacheEntry& entry) const {
    if (entry.expiresAt == std::chrono::steady_clock::time_point::max()) {
        return false;
    }
    return std::chrono::steady_clock::now() >= entry.expiresAt;
}

bool Shard::get(const std::string& key, std::string& outValue) {
    bool needsLruUpdate = false;
    uint64_t now_ms = 0;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            if (!isExpired(it->second)) {
                outValue = it->second.value;
                auto now = std::chrono::steady_clock::now().time_since_epoch();
                now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                uint64_t last = it->second.lruNode->lastLruUpdate.load(std::memory_order_relaxed);
                if (now_ms - last > 500) {
                    needsLruUpdate = true;
                } else {
                    stats_.hits.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            } else {
                needsLruUpdate = true; 
            }
        } else {
            stats_.misses.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        if (isExpired(it->second)) {
            
            lruList_.remove(it->second.lruNode);
            delete it->second.lruNode;
            map_.erase(it);
            stats_.expirations.fetch_add(1, std::memory_order_relaxed);
            stats_.misses.fetch_add(1, std::memory_order_relaxed);
            return false;
        } else {
            
            outValue = it->second.value;
            lruList_.moveToFront(it->second.lruNode);
            it->second.lruNode->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
            stats_.hits.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    
    stats_.misses.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void Shard::evictIfFull() {
    while (map_.size() >= maxEntries_ && !lruList_.isEmpty()) {
        LRUNode* lru = lruList_.popBack();
        if (lru) {
            map_.erase(lru->key);
            delete lru;
            stats_.evictions.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void Shard::set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = map_.find(key);
    
    auto expiresAt = std::chrono::steady_clock::time_point::max();
    if (ttlSeconds.has_value()) {
        expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds.value());
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    if (it != map_.end()) {
        it->second.value = value;
        it->second.expiresAt = expiresAt;
        lruList_.moveToFront(it->second.lruNode);
        it->second.lruNode->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
    } else {
        evictIfFull();
        LRUNode* node = lruList_.pushFront(key);
        node->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
        map_[key] = CacheEntry{value, node, expiresAt};
    }
}

bool Shard::incr(const std::string& key, std::optional<int> ttlSeconds, int64_t& outValue) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = map_.find(key);
    
    auto expiresAt = std::chrono::steady_clock::time_point::max();
    if (ttlSeconds.has_value()) {
        expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds.value());
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    if (it != map_.end() && !isExpired(it->second)) {
        try {
            int64_t current = std::stoll(it->second.value);
            current++;
            it->second.value = std::to_string(current);
            if (ttlSeconds.has_value()) {
                it->second.expiresAt = expiresAt;
            }
            lruList_.moveToFront(it->second.lruNode);
            it->second.lruNode->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
            outValue = current;
            return true;
        } catch (...) {
            return false; 
        }
    } else {
        outValue = 1;
        if (it != map_.end()) {
            it->second.value = "1";
            it->second.expiresAt = expiresAt;
            lruList_.moveToFront(it->second.lruNode);
            it->second.lruNode->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
        } else {
            evictIfFull();
            LRUNode* node = lruList_.pushFront(key);
            node->lastLruUpdate.store(now_ms, std::memory_order_relaxed);
            map_[key] = CacheEntry{"1", node, expiresAt};
        }
        return true;
    }
}

bool Shard::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        lruList_.remove(it->second.lruNode);
        delete it->second.lruNode;
        map_.erase(it);
        return true;
    }
    return false;
}

size_t Shard::sweepExpired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    size_t removed = 0;
    
    for (auto it = map_.begin(); it != map_.end(); ) {
        if (isExpired(it->second)) {
            lruList_.remove(it->second.lruNode);
            delete it->second.lruNode;
            it = map_.erase(it);
            removed++;
            stats_.expirations.fetch_add(1, std::memory_order_relaxed);
        } else {
            ++it;
        }
    }
    return removed;
}

size_t Shard::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return map_.size();
}
