#ifndef REBALANCER_H
#define REBALANCER_H

#include "sharder/consistent_hash.h"
#include "replication/mock_replicator.h"
#include <string>
#include <vector>
#include <unordered_map>

// Rebalancer computes migrations when nodes are added/removed and can execute them
// against a set of MockReplicator nodes (used for tests/integration harness).
class Rebalancer {
public:
    using NodeMap = std::unordered_map<std::string, std::shared_ptr<MockReplicator>>;

    explicit Rebalancer(const ConsistentHash& ring, const NodeMap& nodes);

    // For a node addition, compute (from_node, key) pairs that need to move to new_node_id
    std::vector<std::pair<std::string, std::string>> compute_migrations_for_add(const std::string& new_node_id) const;

    // Execute migrations (replicate into new node then remove from old). Returns number of migrated keys.
    size_t execute_migrations_for_add(const std::string& new_node_id);

    // For a node removal, compute (to_node, key) pairs that need to move from removed_node_id
    std::vector<std::pair<std::string, std::string>> compute_migrations_for_remove(const std::string& removed_node_id) const;

    // Execute migrations for removal: replicate keys from removed node into new owners and remove locally; returns number moved
    size_t execute_migrations_for_remove(const std::string& removed_node_id);
private:
    ConsistentHash ring_;
    NodeMap nodes_;
};

#endif // REBALANCER_H
