#include "cache/lru.h"
#include <mutex>

template <typename K, typename V>
class LRUCache {
public:
    LRUCache(size_t capacity) : capacity(capacity) {}

    V get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) {
            return V(); // Return default value if not found
        }
        // Move the accessed node to the front of the list
        touch(it);
        return it->second->value;
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            // Update the value and move to front
            it->second->value = value;
            touch(it);
        } else {
            if (cacheMap.size() >= capacity) {
                // Evict the least recently used item
                evict();
            }
            // Insert the new item
            auto newNode = std::make_shared<Node>(key, value);
            cacheList.push_front(newNode);
            cacheMap[key] = cacheList.begin();
        }
    }

private:
    struct Node {
        K key;
        V value;
        Node(const K& k, const V& v) : key(k), value(v) {}
    };

    void touch(typename std::unordered_map<K, typename std::list<std::shared_ptr<Node>>::iterator>::iterator it) {
        cacheList.splice(cacheList.begin(), cacheList, it->second);
    }

    void evict() {
        auto last = cacheList.back();
        cacheMap.erase(last->key);
        cacheList.pop_back();
    }

    size_t capacity;
    std::list<std::shared_ptr<Node>> cacheList;
    std::unordered_map<K, typename std::list<std::shared_ptr<Node>>::iterator> cacheMap;
    std::mutex mutex;
};