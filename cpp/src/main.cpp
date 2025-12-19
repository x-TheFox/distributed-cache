#include <iostream>
#include "cache/cache.h"
#include "network/transport.h"
#include "network/tcp_server.h"
#include "network/udp_server.h"

int main(int argc, char* argv[]) {
    // Parse eviction policy from CLI (--policy lru|lfu) or env var EVICTION_POLICY
    EvictionPolicyType policy = EvictionPolicyType::LRU;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--policy" || a == "-p") && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "lfu") policy = EvictionPolicyType::LFU;
        }
        // Allow --mode=tcp or --mode=udp
        if (a.rfind("--mode=", 0) == 0) {
            // handled later
        }
    }
    const char* envp = std::getenv("EVICTION_POLICY");
    if (envp) {
        std::string envs(envp);
        if (envs == "lfu") policy = EvictionPolicyType::LFU;
    }

    // Initialize the cache system with a default capacity and selected policy
    Cache cache(1024, policy);

    // Start the server (TCP or UDP based on command line argument)
    bool tcp = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "tcp") tcp = true;
        if (a.rfind("--mode=", 0) == 0) {
            std::string m = a.substr(strlen("--mode="));
            if (m == "tcp") tcp = true;
        }
    }

    if (tcp) {
        TCPServer tcpServer(8080);
        tcpServer.start();
    } else {
        UDPServer udpServer(8080);
        udpServer.start();
    }

    return 0;
}