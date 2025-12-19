#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <optional>
#include <mutex>
#include <chrono>
#include "cache/lru.h"

struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expiry;

    CacheEntry() = default;
    CacheEntry(const std::string& v, const std::chrono::steady_clock::time_point& e) : value(v), expiry(e) {}
    bool expired() const {
        if (expiry == std::chrono::steady_clock::time_point()) return false;
        return std::chrono::steady_clock::now() > expiry;
    }
};

class Cache {
public:
    Cache(size_t maxSize, EvictionPolicyType policy = EvictionPolicyType::LRU);
    ~Cache();

    // ttl_ms = 0 means no expiry
    void put(const std::string& key, const std::string& value, uint64_t ttl_ms = 0);
    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);
    size_t size();

    uint64_t hits() const;
    uint64_t misses() const;

private:
    std::unique_ptr<EvictionPolicyInterface> evictor_;
    mutable std::mutex mutex_;
    uint64_t hits_{0};
    uint64_t misses_{0};
};

#endif // CACHE_H