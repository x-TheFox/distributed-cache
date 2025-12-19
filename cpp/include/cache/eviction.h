#ifndef EVICTION_H
#define EVICTION_H

#include <string>
#include <optional>
#include <cstddef>

struct CacheEntry;

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
