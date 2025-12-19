#include "sharder/consistent_hash.h"
#include <openssl/sha.h>
#include <sstream>

ConsistentHash::ConsistentHash(size_t virtual_nodes) : virtual_nodes_(virtual_nodes) {}

static uint64_t sha1_hash64(const std::string &s) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), digest);
    // fold first 8 bytes into uint64_t
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | digest[i];
    return v;
}

uint64_t ConsistentHash::hash_str(const std::string &s) const {
    return sha1_hash64(s);
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
