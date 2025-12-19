#ifndef EVICTION_H
#define EVICTION_H

#include <string>
#include <optional>
#include <cstddef>
#include <chrono>

// CacheEntry is shared across eviction implementations
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

enum class EvictionPolicyType { LRU, LFU };

class EvictionPolicyInterface {
public:
    virtual ~EvictionPolicyInterface() = default;
    virtual void put(const std::string& key, const CacheEntry& entry) = 0;
    virtual bool get(const std::string& key, CacheEntry& out) = 0;
    virtual bool remove(const std::string& key) = 0;
    virtual size_t size() const = 0;
};

#endif // EVICTION_H
