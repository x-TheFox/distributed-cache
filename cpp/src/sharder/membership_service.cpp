#include "sharder/membership_service.h"
#include <sstream>
#include <iostream>

MembershipService::MembershipService(const LocalNodeInfo &local, size_t virtual_nodes)
    : local_(local), ring_(virtual_nodes) {
    // build initial router from empty ring
    auto r = std::make_shared<Router>(ring_, local_);
    router_.store(r);
    Router::set_default(r);
}

static bool parse_ip_port(const std::string &s, std::string &ip, int &port) {
    auto pos = s.find(':');
    if (pos == std::string::npos) return false;
    ip = s.substr(0, pos);
    try {
        port = std::stoi(s.substr(pos+1));
    } catch (...) { return false; }
    return true;
}

void MembershipService::LoadSeedList(const std::vector<std::string> &seeds) {
    for (const auto &s : seeds) {
        std::string ip; int port;
        if (!parse_ip_port(s, ip, port)) {
            std::cerr << "MembershipService: invalid seed entry '" << s << "'\n";
            continue;
        }
        std::string node_id = ip + ":" + std::to_string(port);
        OnNodeJoin(node_id, ip, port);
    }
}

void MembershipService::OnNodeJoin(const std::string &node_id, const std::string &ip, int port) {
    std::lock_guard<std::mutex> lg(mutex_);
    addr_map_[node_id] = {ip, port};
    ring_.add_node(node_id);
    rebuildRouterAndSwap();
}

void MembershipService::OnNodeLeave(const std::string &node_id) {
    std::lock_guard<std::mutex> lg(mutex_);
    addr_map_.erase(node_id);
    ring_.remove_node(node_id);
    rebuildRouterAndSwap();
}

void MembershipService::rebuildRouterAndSwap() {
    // Build a new Router instance from current ring and addr map
    auto new_router = std::make_shared<Router>(ring_, local_);
    for (const auto &kv : addr_map_) {
        new_router->add_node_address(kv.first, kv.second.first, kv.second.second);
    }
    router_.store(new_router);
    // Also update global default router so existing code paths pick it up
    Router::set_default(new_router);
}

std::shared_ptr<Router> MembershipService::getRouter() const {
    return router_.load();
}

std::vector<std::string> MembershipService::nodes() const {
    // ConsistentHash already protects its internal structures; we just ask for nodes()
    std::vector<std::string> out;
    // If ConsistentHash exposes nodes(), we could call it directly. Fallback: peek at addr_map_
    std::lock_guard<std::mutex> lg(mutex_);
    out.reserve(addr_map_.size());
    for (const auto &kv : addr_map_) out.push_back(kv.first);
    return out;
}
