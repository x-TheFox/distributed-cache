#include "sharder/router.h"
#include <functional>
#include <cstdint>

std::shared_ptr<Router> Router::default_router_ = nullptr;
#include <atomic>

Router::Router(const ConsistentHash &ring, const LocalNodeInfo &local) : ring_(ring), local_(local) {
    // Ensure local node is present in ring
    if (ring_.get_node(std::string()) == std::string()) {
        // no-op
    }
}

void Router::add_node_address(const std::string &node_id, const std::string &ip, int port) {
    addr_map_[node_id] = {ip, port};
}

void Router::remove_node_address(const std::string &node_id) {
    addr_map_.erase(node_id);
}

Router::Route Router::lookup(const std::string &key) const {
    std::string owner = ring_.get_node(key);
    if (owner.empty() || owner == local_.id) {
        return Route{RouteType::LOCAL, "", 0, 0};
    }
    auto it = addr_map_.find(owner);
    std::string ip = "";
    int port = 0;
    if (it != addr_map_.end()) {
        ip = it->second.first;
        port = it->second.second;
    }
    // compute a simple slot as hash mod 16384
    uint64_t h = static_cast<uint64_t>(std::hash<std::string>{}(key));
    uint64_t slot = h % 16384;

    return Route{RouteType::REMOTE, ip, port, slot};
}

void Router::set_default(std::shared_ptr<Router> r) {
    std::atomic_store(&default_router_, r);
}

std::shared_ptr<Router> Router::get_default() {
    return std::atomic_load(&default_router_);
}
