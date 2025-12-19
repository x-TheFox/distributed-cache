#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include <mutex>

template<typename K, typename V>
class LRUCache {
public:
    LRUCache(size_t capacity);
    V get(const K& key);
    void put(const K& key, const V& value);

private:
    size_t capacity_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> cache_map_;
    std::list<std::pair<K, V>> cache_list_;
    std::mutex mutex_;
    
    void moveToFront(const K& key, const V& value);
};

template<typename K, typename V>
LRUCache<K, V>::LRUCache(size_t capacity) : capacity_(capacity) {}

template<typename K, typename V>
V LRUCache<K, V>::get(const K& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        return V(); // Return default value if not found
    }
    // Move the accessed item to the front of the list
    auto value = it->second->second;
    moveToFront(key, value);
    return value;
}

template<typename K, typename V>
void LRUCache<K, V>::put(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        // Update existing item and move to front
        moveToFront(key, value);
    } else {
        if (cache_list_.size() >= capacity_) {
            // Remove the least recently used item
            auto last = cache_list_.back();
            cache_map_.erase(last.first);
            cache_list_.pop_back();
        }
        // Insert the new item
        cache_list_.emplace_front(key, value);
        cache_map_[key] = cache_list_.begin();
    }
}

template<typename K, typename V>
void LRUCache<K, V>::moveToFront(const K& key, const V& value) {
    auto it = cache_map_[key];
    cache_list_.erase(it);
    cache_list_.emplace_front(key, value);
    cache_map_[key] = cache_list_.begin();
}

#endif // LRU_H