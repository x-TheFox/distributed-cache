#include "sharder/rebalancer.h"
#include <unordered_set>

Rebalancer::Rebalancer(const ConsistentHash& ring, const NodeMap& nodes) : ring_(ring), nodes_(nodes) {}

std::vector<std::pair<std::string, std::string>> Rebalancer::compute_migrations_for_add(const std::string& new_node_id) const {
    std::vector<std::pair<std::string, std::string>> out;

    // copy ring and add the new node to see which keys would move
    ConsistentHash tmp = ring_;
    tmp.add_node(new_node_id);

    std::unordered_set<std::string> seen;
    for (const auto &entry : nodes_) {
        const std::string &from_node = entry.first;
        if (from_node == new_node_id) continue; // avoid migrating keys already on the new node
        auto node_ptr = entry.second;
        // list local keys
        auto keys = node_ptr->list_keys();
        for (const auto &k : keys) {
            if (seen.count(k)) continue;
            auto new_owner = tmp.get_node(k);
            if (new_owner == new_node_id) {
                out.emplace_back(from_node, k);
                seen.insert(k);
            }
        }
    }

    return out;
}

size_t Rebalancer::execute_migrations_for_add(const std::string& new_node_id) {
    auto migrations = compute_migrations_for_add(new_node_id);
    size_t moved = 0;

    auto it_new = nodes_.find(new_node_id);
    if (it_new == nodes_.end()) return 0;
    auto new_node = it_new->second;

    for (const auto &p : migrations) {
        const auto &from = p.first;
        const auto &key = p.second;
        auto it_from = nodes_.find(from);
        if (it_from == nodes_.end()) continue;
        auto from_node = it_from->second;

        // fetch value from old node
        std::string v;
        if (!from_node->get_local(key, v)) continue;

        // replicate into new node (which will apply locally and optionally replicate)
        if (!new_node->replicate(key, v)) continue;

        // remove from old node
        from_node->remove_local(key);
        ++moved;
    }

    return moved;
}

std::vector<std::pair<std::string, std::string>> Rebalancer::compute_migrations_for_remove(const std::string& removed_node_id) const {
    std::vector<std::pair<std::string, std::string>> out;

    // copy ring and remove the node to see new ownership
    ConsistentHash tmp = ring_;
    tmp.remove_node(removed_node_id);

    auto it_removed = nodes_.find(removed_node_id);
    if (it_removed == nodes_.end()) return out;

    auto keys = it_removed->second->list_keys();
    for (const auto &k : keys) {
        auto new_owner = tmp.get_node(k);
        if (!new_owner.empty() && new_owner != removed_node_id) {
            out.emplace_back(new_owner, k);
        }
    }

    return out;
}

size_t Rebalancer::execute_migrations_for_remove(const std::string& removed_node_id) {
    auto migrations = compute_migrations_for_remove(removed_node_id);
    size_t moved = 0;

    auto it_removed = nodes_.find(removed_node_id);
    if (it_removed == nodes_.end()) return 0;
    auto removed_node = it_removed->second;

    for (const auto &p : migrations) {
        const auto &to = p.first;
        const auto &key = p.second;
        auto it_to = nodes_.find(to);
        if (it_to == nodes_.end()) continue;
        auto to_node = it_to->second;

        std::string v;
        if (!removed_node->get_local(key, v)) continue;

        if (!to_node->replicate(key, v)) continue;

        removed_node->remove_local(key);
        ++moved;
    }

    return moved;
}
