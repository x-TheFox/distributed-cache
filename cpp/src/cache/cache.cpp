#include "cache/cache.h"
#include <chrono>

Cache::Cache(size_t maxSize, EvictionPolicyType policy) {
    switch (policy) {
        case EvictionPolicyType::LRU:
            // Use existing LRU implementation
            evictor_ = std::make_unique<LRUCache>(maxSize);
            break;
        case EvictionPolicyType::LFU:
            evictor_ = std::make_unique<LFUEvictor>(maxSize);
            break;
    }
    // initialize stripes without resizing (mutex is non-movable): emplace defaults
    stripes_.clear();
    for (size_t i = 0; i < stripe_count_; ++i) stripes_.push_back(std::make_unique<std::mutex>());

}

Cache::~Cache() = default;

void Cache::put(const std::string& key, const std::string& value, uint64_t ttl_ms) {
    std::chrono::steady_clock::time_point expiry;
    if (ttl_ms > 0) {
        expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ttl_ms);
    }
    CacheEntry entry(value, expiry);
    size_t idx = hasher_(key) % stripe_count_;
    std::lock_guard<std::mutex> lock(*stripes_[idx]);
    evictor_->put(key, entry);
}

std::optional<std::string> Cache::get(const std::string& key) {
    size_t idx = hasher_(key) % stripe_count_;
    std::lock_guard<std::mutex> lock(*stripes_[idx]);
    CacheEntry entry;
    if (!evictor_->get(key, entry)) {
        ++misses_;
        return std::nullopt;
    }
    if (entry.expired()) {
        // Remove expired entry
        evictor_->remove(key);
        ++misses_;
        return std::nullopt;
    }
    ++hits_;
    return entry.value;
}

bool Cache::remove(const std::string& key) {
    size_t idx = hasher_(key) % stripe_count_;
    std::lock_guard<std::mutex> lock(*stripes_[idx]);
    return evictor_->remove(key);
}

size_t Cache::size() {
    std::lock_guard<std::mutex> lock(size_mutex_);
    return evictor_->size();
}

uint64_t Cache::hits() const { return hits_.load(); }
uint64_t Cache::misses() const { return misses_.load(); }