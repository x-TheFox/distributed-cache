#ifndef MEMBERSHIP_SERVICE_H
#define MEMBERSHIP_SERVICE_H

#include "sharder/consistent_hash.h"
#include "sharder/router.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>

class MembershipService {
public:
    explicit MembershipService(const LocalNodeInfo &local, size_t virtual_nodes = 100);

    // Phase A: load static seed list in format "IP:PORT" (node id will be ip:port)
    void LoadSeedList(const std::vector<std::string> &seeds);

    // Phase B: dynamic membership
    void OnNodeJoin(const std::string &node_id, const std::string &ip, int port);
    void OnNodeLeave(const std::string &node_id);

    // Inspect current router and ring
    std::shared_ptr<Router> getRouter() const;
    std::vector<std::string> nodes() const;

private:
    void rebuildRouterAndSwap();

    LocalNodeInfo local_;
    mutable std::mutex mutex_;
    ConsistentHash ring_;
    std::map<std::string, std::pair<std::string,int>> addr_map_; // node_id -> (ip,port)
    std::atomic<std::shared_ptr<Router>> router_;
};

#endif // MEMBERSHIP_SERVICE_H
