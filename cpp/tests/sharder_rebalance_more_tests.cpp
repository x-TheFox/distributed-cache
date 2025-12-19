#include <gtest/gtest.h>
#include "sharder/consistent_hash.h"
#include "sharder/rebalancer.h"
#include "replication/mock_replicator.h"

#include <unordered_map>
#include <thread>
#include <random>

TEST(RebalancerTest, RemoveNodeMovesKeysToNewOwners) {
    using RepPtr = std::shared_ptr<MockReplicator>;

    RepPtr n1 = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    RepPtr n2 = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    RepPtr n3 = std::make_shared<MockReplicator>(MockReplicatorOptions{});

    std::unordered_map<std::string, RepPtr> nodes {
        {"node1", n1},
        {"node2", n2},
        {"node3", n3}
    };

    ConsistentHash ring(10);
    ring.add_node("node1");
    ring.add_node("node2");
    ring.add_node("node3");

    // seed keys across owners
    const int N = 300;
    for (int i = 0; i < N; ++i) {
        std::string key = "rkey-" + std::to_string(i);
        std::string value = "rv-" + std::to_string(i);
        auto owner = ring.get_node(key);
        ASSERT_TRUE(nodes.count(owner));
        ASSERT_TRUE(nodes[owner]->replicate(key, value));
    }

    // remove node2 and expect its keys to be moved
    Rebalancer rebalancer(ring, nodes);
    auto expected = rebalancer.compute_migrations_for_remove("node2");
    EXPECT_FALSE(expected.empty());
    auto moved = rebalancer.execute_migrations_for_remove("node2");
    EXPECT_EQ(moved, expected.size());

    // ensure keys are on their new owners and not on node2
    for (const auto &p : expected) {
        const auto &to = p.first;
        const auto &k = p.second;
        std::string v;
        EXPECT_TRUE(nodes[to]->get_local(k, v));
        EXPECT_FALSE(nodes["node2"]->get_local(k, v));
    }

    n1->stop(); n2->stop(); n3->stop();
}

TEST(RebalancerTest, MigrationWithReplicatorPeers) {
    using RepPtr = std::shared_ptr<MockReplicator>;

    // Configure peers with wait_for_acks so replicates confirm propagation
    MockReplicatorOptions opts; opts.wait_for_acks = true; opts.peer_latency = std::chrono::milliseconds(5);

    RepPtr n1 = std::make_shared<MockReplicator>(opts);
    RepPtr n2 = std::make_shared<MockReplicator>(opts);
    RepPtr n3 = std::make_shared<MockReplicator>(opts);

    // make them all peers so replication propagates
    n1->add_peer(n2); n1->add_peer(n3);
    n2->add_peer(n1); n2->add_peer(n3);
    n3->add_peer(n1); n3->add_peer(n2);

    std::unordered_map<std::string, RepPtr> nodes {
        {"node1", n1},
        {"node2", n2},
        {"node3", n3}
    };

    ConsistentHash ring(10);
    ring.add_node("node1");
    ring.add_node("node2");

    const int N = 200;
    for (int i = 0; i < N; ++i) {
        std::string key = "pkey-" + std::to_string(i);
        std::string value = "pv-" + std::to_string(i);
        auto owner = ring.get_node(key);
        ASSERT_TRUE(nodes.count(owner));
        // use replicate so peers will also receive keys
        ASSERT_TRUE(nodes[owner]->replicate(key, value));
    }

    Rebalancer rebalancer(ring, nodes);
    auto expected = rebalancer.compute_migrations_for_add("node3");
    auto moved = rebalancer.execute_migrations_for_add("node3");
    EXPECT_EQ(moved, expected.size());

    // For each migrated key, ensure new node AND its peers have the key
    for (const auto &p : expected) {
        const auto &from = p.first; const auto &k = p.second;
        std::string v;
        EXPECT_TRUE(nodes["node3"]->get_local(k, v));
        // since node3 replicates to peers (wait_for_acks), peers should have it
        EXPECT_TRUE(nodes["node1"]->get_local(k, v) || nodes["node2"]->get_local(k, v));
        // original node should no longer have it
        EXPECT_FALSE(nodes[from]->get_local(k, v));
    }

    n1->stop(); n2->stop(); n3->stop();
}

TEST(RebalancerTest, LargeScaleRebalanceUnderLoad) {
    using RepPtr = std::shared_ptr<MockReplicator>;
    const int NODES = 8;
    const int KEYS = 2000;

    std::vector<RepPtr> vec;
    std::unordered_map<std::string, RepPtr> nodes;
    for (int i = 0; i < NODES; ++i) {
        auto p = std::make_shared<MockReplicator>(MockReplicatorOptions{});
        vec.push_back(p);
        nodes["n" + std::to_string(i)] = p;
    }

    ConsistentHash ring(50);
    for (int i = 0; i < NODES; ++i) ring.add_node("n" + std::to_string(i));

    // seed keys
    for (int i = 0; i < KEYS; ++i) {
        std::string k = "lk-" + std::to_string(i);
        std::string v = "lv-" + std::to_string(i);
        auto owner = ring.get_node(k);
        nodes[owner]->replicate(k, v);
    }

    // start background writer thread to simulate active traffic
    std::atomic<bool> stop{false};
    std::thread writer([&]{
        std::mt19937_64 rng(12345);
        std::uniform_int_distribution<int> keydist(0, KEYS-1);
        std::uniform_int_distribution<int> nodedist(0, NODES-1);
        while (!stop.load()) {
            int idx = keydist(rng);
            int nid = nodedist(rng);
            std::string k = "lk-" + std::to_string(idx);
            std::string v = "w-" + std::to_string(rng() % 100000);
            nodes["n" + std::to_string(nid)]->replicate(k, v);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // add a new node and rebalance
    auto new_node = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    nodes["n_new"] = new_node;
    ring.add_node("n_new");
    Rebalancer rebalancer(ring, nodes);
    auto moved = rebalancer.execute_migrations_for_add("n_new");

    // stop background writer
    stop.store(true);
    writer.join();

    // Basic sanity: ensure no keys remain on nodes that shouldn't own them
    for (int i = 0; i < KEYS; ++i) {
        std::string k = "lk-" + std::to_string(i);
        auto owner = ring.get_node(k);
        std::string v;
        EXPECT_TRUE(nodes[owner]->get_local(k, v));
    }

    new_node->stop();
    for (auto &p : vec) p->stop();
}
