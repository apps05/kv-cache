#include "stats.h"
#include <sstream>

std::string Stats::format() const {
    std::ostringstream oss;
    oss << "hits=" << hits.load(std::memory_order_relaxed)
        << " misses=" << misses.load(std::memory_order_relaxed)
        << " evictions=" << evictions.load(std::memory_order_relaxed)
        << " expirations=" << expirations.load(std::memory_order_relaxed);
    return oss.str();
}
