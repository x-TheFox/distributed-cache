#include <iostream>
#include <vector>
#include "cache/cache.h"
#include "network/transport.h"
#include "network/tcp_server.h"
#include "network/udp_server.h"
#include "sharder/membership_service.h"
#include "replication/log.h"

int main(int argc, char* argv[]) {
    // Parse eviction policy from CLI (--policy lru|lfu) or env var EVICTION_POLICY
    EvictionPolicyType policy = EvictionPolicyType::LRU;
    int port = 8080;
    std::string node_id = "node";
    const std::string ip = "127.0.0.1";
    std::vector<std::string> seeds;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--policy" || a == "-p") && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "lfu") policy = EvictionPolicyType::LFU;
            continue;
        }
        if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
            continue;
        }
        if (a == "--node-id" && i + 1 < argc) {
            node_id = argv[++i];
            continue;
        }
        if (a.rfind("--seed=", 0) == 0) {
            seeds.push_back(a.substr(strlen("--seed=")));
            continue;
        }
        if (a == "--seed" && i + 1 < argc) {
            seeds.push_back(argv[++i]);
            continue;
        }
        // Allow --mode=tcp or --mode=udp or bare 'tcp'
        if (a == "tcp") {
            // handled later
            continue;
        }
        if (a.rfind("--mode=", 0) == 0) {
            // handled later
            continue;
        }
    }

    const char* envp = std::getenv("EVICTION_POLICY");
    if (envp) {
        std::string envs(envp);
        if (envs == "lfu") policy = EvictionPolicyType::LFU;
    }

    // Initialize the cache system with a default capacity and selected policy
    Cache cache(1024, policy);

    // Build membership service if seeds or node-id/port are provided
    LocalNodeInfo local{node_id, ip, port};
    MembershipService ms(local, 100);
    // always ensure local node is present in the ring
    ms.OnNodeJoin(local.id, local.ip, local.port);
    if (!seeds.empty()) ms.LoadSeedList(seeds);

    // Start the server (TCP by default) on the configured port
    LOG(LogLevel::INFO, "starting server node=" << local.id << " port=" << port);
    TCPServer tcpServer(port, &cache);
    tcpServer.start();

    return 0;
}