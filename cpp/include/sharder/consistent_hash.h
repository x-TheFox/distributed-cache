#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

// Simple consistent hashing ring with virtual nodes.
class ConsistentHash {
public:
    // weight = number of virtual nodes per physical node
    explicit ConsistentHash(size_t virtual_nodes = 100);

    // copyable safely (mutex protected)
    ConsistentHash(const ConsistentHash& other);
    ConsistentHash& operator=(const ConsistentHash& other);

    // add a node with given id (e.g., "node1")
    void add_node(const std::string& node_id);
    void remove_node(const std::string& node_id);

    // return node id responsible for a key
    std::string get_node(const std::string& key) const;

    // get all node ids in the ring
    std::vector<std::string> nodes() const;

private:
    uint64_t hash_str(const std::string& s) const;

    size_t virtual_nodes_;
    std::map<uint64_t, std::string> ring_; // hash -> node_id
    std::map<std::string, std::vector<uint64_t>> node_hashes_; // node_id -> list of hashes
    mutable std::mutex mutex_;
};

#endif // CONSISTENT_HASH_H
