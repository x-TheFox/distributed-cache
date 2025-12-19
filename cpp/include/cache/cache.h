#ifndef CACHE_H
#define CACHE_H

#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>

template <typename Key, typename Value>
class Cache {
public:
    Cache(size_t maxSize);
    ~Cache();

    void put(const Key& key, const Value& value);
    std::shared_ptr<Value> get(const Key& key);
    void remove(const Key& key);
    void clear();

private:
    size_t maxSize;
    std::unordered_map<Key, std::shared_ptr<Value>> cacheMap;
    std::mutex cacheMutex;
};

#include "lru.h"
#include "allocator.h"

#endif // CACHE_H