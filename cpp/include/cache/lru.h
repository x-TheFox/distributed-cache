#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include <mutex>

template<typename K, typename V>
class LRUCache {
public:
    LRUCache(size_t capacity);
    // Returns true and sets value_out if found, otherwise returns false
    bool get(const K& key, V& value_out);
    void put(const K& key, const V& value);
    bool remove(const K& key);
    size_t size() const;

private:
    size_t capacity_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> cache_map_;
    std::list<std::pair<K, V>> cache_list_;
    mutable std::mutex mutex_;
    
    void moveToFront(const K& key, const V& value);
};

template<typename K, typename V>
LRUCache<K, V>::LRUCache(size_t capacity) : capacity_(capacity) {}

template<typename K, typename V>
bool LRUCache<K, V>::get(const K& key, V& value_out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        return false;
    }
    // Move the accessed item to the front of the list
    value_out = it->second->second;
    moveToFront(key, value_out);
    return true;
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
bool LRUCache<K, V>::remove(const K& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) return false;
    cache_list_.erase(it->second);
    cache_map_.erase(it);
    return true;
}

template<typename K, typename V>
size_t LRUCache<K, V>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_list_.size();
}

template<typename K, typename V>
void LRUCache<K, V>::moveToFront(const K& key, const V& value) {
    auto it = cache_map_[key];
    cache_list_.erase(it);
    cache_list_.emplace_front(key, value);
    cache_map_[key] = cache_list_.begin();
}

#endif // LRU_H