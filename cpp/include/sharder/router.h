#ifndef ROUTER_H
#define ROUTER_H

#include "sharder/consistent_hash.h"
#include <string>
#include <unordered_map>
#include <memory>

struct LocalNodeInfo {
    std::string id;
    std::string ip;
    int port;
};

class Router {
public:
    enum class RouteType { LOCAL, REMOTE };
    struct Route {
        RouteType type;
        std::string ip;
        int port;
        uint64_t slot;
    };

    explicit Router(const ConsistentHash &ring, const LocalNodeInfo &local);

    // Add a node's address information (node id -> ip:port)
    void add_node_address(const std::string &node_id, const std::string &ip, int port);
    void remove_node_address(const std::string &node_id);

    // Look up route for a key
    Route lookup(const std::string &key) const;

    // Helpers to set/get a global default router (convenience for protocol integration)
    static void set_default(std::shared_ptr<Router> r);
    static std::shared_ptr<Router> get_default();

private:
    ConsistentHash ring_;
    LocalNodeInfo local_;
    std::unordered_map<std::string, std::pair<std::string,int>> addr_map_;
    static std::shared_ptr<Router> default_router_;
};

#endif // ROUTER_H
