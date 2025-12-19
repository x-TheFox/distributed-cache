#include "cache/cache.h"
#include "cache/lru.h"
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <string>

class Cache {
public:
    Cache(size_t maxSize) : lruCache(maxSize) {}

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        lruCache.put(key, value);
    }

    std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        return lruCache.get(key);
    }

private:
    LRUCache lruCache;
    std::mutex cacheMutex;
};