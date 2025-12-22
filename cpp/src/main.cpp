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

    // Optional debug log level via env var LOG_LEVEL=DEBUG|INFO|WARN|ERROR
    const char* ll = std::getenv("LOG_LEVEL");
    if (ll) {
        std::string lvl(ll);
        if (lvl == "DEBUG") replication::setLogLevel(replication::LogLevel::DEBUG);
        else if (lvl == "INFO") replication::setLogLevel(replication::LogLevel::INFO);
        else if (lvl == "WARN") replication::setLogLevel(replication::LogLevel::WARN);
        else if (lvl == "ERROR") replication::setLogLevel(replication::LogLevel::ERROR);
    }

    // Initialize the cache system with a default capacity and selected policy
    Cache cache(1024, policy);

    // Build membership service if seeds or node-id/port are provided
    LocalNodeInfo local{node_id, ip, port};
    MembershipService ms(local, 100);
    // always ensure local node is present in the ring
    ms.OnNodeJoin(local.id, local.ip, local.port);
    if (!seeds.empty()) ms.LoadSeedList(seeds);

    // Support a deterministic helper to find a key that maps to a given node id.
    // This is useful for demos and tests: it does a fast scan using the same ConsistentHash
    // and returns a key that will be owned by the requested node.
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--find-key-for-node" && i + 1 < argc) {
            std::string target = argv[++i];
            int max_tries = 100000;
            for (int j = 0; j < argc; ++j) {
                std::string b = argv[j];
                if (b == "--max-tries" && j + 1 < argc) {
                    max_tries = std::stoi(argv[j+1]);
                }
            }
            // Fast scan using the same ConsistentHash/ring via the global router
            auto router = Router::get_default();
            if (!router) {
                std::cerr << "No router available (ensure seeds are provided)\n";
                return 2;
            }
            for (int k = 0; k < max_tries; ++k) {
                std::string key = "demo_key_" + std::to_string(k);
                auto r = router->lookup(key);
                std::string owner = router->get_owner_node(key);
                // Match by node id (e.g., "nodeB") or by ip:port (e.g., "127.0.0.1:6385")
                std::string ipport = r.ip + ":" + std::to_string(r.port);
                if (target == owner || target == ipport || (target == local.id && r.type == Router::RouteType::LOCAL)) {
                    std::cout << key << std::endl;
                    return 0;
                }
            }
            std::cerr << "Failed to find key for node after " << max_tries << " tries\n";
            return 3;
        }
    }

    // Start the server (TCP by default) on the configured port
    LOG(replication::LogLevel::INFO, "starting server node=" << local.id << " port=" << port);
    TCPServer tcpServer(port, &cache);
    tcpServer.start();

    // Signal readiness (membership/routing initialized and server listening) so demos/tests can wait on it
    LOG(replication::LogLevel::INFO, "[server] READY AND ROUTING");

    return 0;
}