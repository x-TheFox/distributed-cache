#ifndef LFU_H
#define LFU_H

#include <unordered_map>
#include <list>
#include <mutex>
#include <string>
#include <memory>
#include "eviction.h"

// Simple LFU cache implementation
class LFUEvictor : public EvictionPolicyInterface {
public:
    explicit LFUEvictor(size_t capacity);
    void put(const std::string& key, const CacheEntry& entry) override;
    bool get(const std::string& key, CacheEntry& out) override;
    bool remove(const std::string& key) override;
    size_t size() const override;

private:
    struct Node {
        std::string key;
        CacheEntry entry;
        size_t freq;
    };

    size_t capacity_;
    mutable std::mutex mutex_;
    // key -> iterator to list in freq_lists_[freq]
    std::unordered_map<std::string, std::list<Node>::iterator> nodes_;
    // freq -> list of Nodes (most recent at front)
    std::unordered_map<size_t, std::list<Node>> freq_lists_;
    size_t min_freq_ = 0;

    void touch(std::list<Node>::iterator it);
    void evict_one();
};

#endif // LFU_H