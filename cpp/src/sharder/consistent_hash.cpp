#include "sharder/consistent_hash.h"
#include <functional>
#include <sstream>

ConsistentHash::ConsistentHash(size_t virtual_nodes) : virtual_nodes_(virtual_nodes) {}

ConsistentHash::ConsistentHash(const ConsistentHash& other) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    virtual_nodes_ = other.virtual_nodes_;
    ring_ = other.ring_;
    node_hashes_ = other.node_hashes_;
}

ConsistentHash& ConsistentHash::operator=(const ConsistentHash& other) {
    if (this == &other) return *this;
    std::lock_guard<std::mutex> lock_this(mutex_);
    std::lock_guard<std::mutex> lock_other(other.mutex_);
    virtual_nodes_ = other.virtual_nodes_;
    ring_ = other.ring_;
    node_hashes_ = other.node_hashes_;
    return *this;
}

// Portable 64-bit hash using std::hash with a simple mixing step
uint64_t mix_u64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

uint64_t ConsistentHash::hash_str(const std::string &s) const {
    std::hash<std::string> hasher;
    uint64_t h = static_cast<uint64_t>(hasher(s));
    return mix_u64(h);
}

void ConsistentHash::add_node(const std::string &node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (node_hashes_.find(node_id) != node_hashes_.end()) return; // already added
    std::vector<uint64_t> hashes;
    for (size_t i = 0; i < virtual_nodes_; ++i) {
        std::ostringstream oss;
        oss << node_id << "#" << i;
        uint64_t h = hash_str(oss.str());
        ring_[h] = node_id;
        hashes.push_back(h);
    }
    node_hashes_[node_id] = std::move(hashes);
}

void ConsistentHash::remove_node(const std::string &node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = node_hashes_.find(node_id);
    if (it == node_hashes_.end()) return;
    for (auto h : it->second) ring_.erase(h);
    node_hashes_.erase(it);
}

std::string ConsistentHash::get_node(const std::string &key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ring_.empty()) return std::string();
    uint64_t h = hash_str(key);
    auto it = ring_.lower_bound(h);
    if (it == ring_.end()) it = ring_.begin();
    return it->second;
}

std::vector<std::string> ConsistentHash::nodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    for (auto &p : node_hashes_) out.push_back(p.first);
    return out;
}
