#include <gtest/gtest.h>
#include "sharder/router.h"
#include "sharder/consistent_hash.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

using namespace std;

// Helper to build a router with N nodes and sequential ports starting at base_port
static shared_ptr<Router> build_router(int nodes, int base_port) {
    ConsistentHash ring(10);
    LocalNodeInfo local{"local", "127.0.0.1", base_port};
    for (int i = 0; i < nodes; ++i) {
        string id = "node" + to_string(i);
        ring.add_node(id);
    }
    auto r = make_shared<Router>(ring, local);
    for (int i = 0; i < nodes; ++i) {
        string id = "node" + to_string(i);
        r->add_node_address(id, "127.0.0.1", base_port + i + 1);
    }
    return r;
}

TEST(RouterConcurrency, SwapUnderLoad) {
    // Writer will swap routers rapidly; readers will continuously lookup
    const int iterations = 2000;
    const int readers = 8;
    atomic<bool> stop{false};

    // initial router
    auto r0 = build_router(3, 7000);
    Router::set_default(r0);

    // writer thread
    thread writer([&]{
        for (int i = 0; i < iterations; ++i) {
            auto nr = build_router(3 + (i % 5), 7000 + (i % 100));
            Router::set_default(nr);
            // brief pause to allow readers to exercise
            this_thread::sleep_for(chrono::microseconds(10));
        }
        stop.store(true);
    });

    // reader threads
    vector<thread> ths;
    atomic<size_t> checks{0};
    for (int t = 0; t < readers; ++t) {
        ths.emplace_back([&]{
            std::mt19937_64 rng(std::random_device{}());
            std::uniform_int_distribution<int> dist(1, 10000);
            while (!stop.load()) {
                // get router once
                auto router = Router::get_default();
                ASSERT_TRUE((bool)router);
                // pick a random key
                string key = "k" + to_string(dist(rng));
                auto r = router->lookup(key);
                // route should be valid: slot within range; if REMOTE then ip non-empty and port > 0
                EXPECT_LT(r.slot, static_cast<uint64_t>(16384));
                if (r.type == Router::RouteType::REMOTE) {
                    EXPECT_FALSE(r.ip.empty());
                    EXPECT_GT(r.port, 0);
                }
                ++checks;
            }
        });
    }

    writer.join();
    for (auto &t : ths) t.join();

    EXPECT_GT(checks.load(), 0u);
}
