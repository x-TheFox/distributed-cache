#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include <mutex>
#include <string>
#include "eviction.h"

class LRUCache : public EvictionPolicyInterface {
public:
    explicit LRUCache(size_t capacity);
    bool get(const std::string& key, CacheEntry& value_out) override;
    void put(const std::string& key, const CacheEntry& value) override;
    bool remove(const std::string& key) override;
    size_t size() const override;

private:
    size_t capacity_;
    std::unordered_map<std::string, std::list<std::pair<std::string, CacheEntry>>::iterator> cache_map_;
    std::list<std::pair<std::string, CacheEntry>> cache_list_;
    mutable std::mutex mutex_;
    
    void moveToFront(const std::string& key, const CacheEntry& value);
};

inline LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {}

inline bool LRUCache::get(const std::string& key, CacheEntry& value_out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        return false;
    }
    value_out = it->second->second;
    moveToFront(key, value_out);
    return true;
}

inline void LRUCache::put(const std::string& key, const CacheEntry& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        moveToFront(key, value);
    } else {
        if (cache_list_.size() >= capacity_) {
            auto last = cache_list_.back();
            cache_map_.erase(last.first);
            cache_list_.pop_back();
        }
        cache_list_.emplace_front(key, value);
        cache_map_[key] = cache_list_.begin();
    }
}

inline bool LRUCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) return false;
    cache_list_.erase(it->second);
    cache_map_.erase(it);
    return true;
}

inline size_t LRUCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_list_.size();
}

inline void LRUCache::moveToFront(const std::string& key, const CacheEntry& value) {
    auto it = cache_map_[key];
    cache_list_.erase(it);
    cache_list_.emplace_front(key, value);
    cache_map_[key] = cache_list_.begin();
}

#endif // LRU_H