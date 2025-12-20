#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <optional>
#include <mutex>
#include <chrono>
#include "eviction.h"
#include "lru.h"
#include "lfu.h"

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

    // Lock striping to reduce contention: vector of mutexes guarded per key
    std::vector<std::unique_ptr<std::mutex>> stripes_;
    size_t stripe_count_ = 16; // default

    std::hash<std::string> hasher_;

    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};

    std::mutex size_mutex_; // used only for size() when necessary
};

#endif // CACHE_H