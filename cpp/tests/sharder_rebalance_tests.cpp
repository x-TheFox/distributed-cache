#include <gtest/gtest.h>
#include "sharder/consistent_hash.h"
#include "sharder/rebalancer.h"
#include "replication/mock_replicator.h"

#include <unordered_map>

TEST(RebalancerTest, AddNodeMovesKeysToNewOwner) {
    using RepPtr = std::shared_ptr<MockReplicator>;

    // make three mock nodes (no peers for isolation)
    RepPtr n1 = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    RepPtr n2 = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    RepPtr n3 = std::make_shared<MockReplicator>(MockReplicatorOptions{});

    std::unordered_map<std::string, RepPtr> nodes {
        {"node1", n1},
        {"node2", n2},
        {"node3", n3}
    };

    // small ring for deterministic behavior
    ConsistentHash ring(10);
    ring.add_node("node1");
    ring.add_node("node2");

    // seed keys according to ring owner
    const int N = 200;
    for (int i = 0; i < N; ++i) {
        std::string key = "key-" + std::to_string(i);
        std::string value = "v-" + std::to_string(i);
        auto owner = ring.get_node(key);
        ASSERT_TRUE(nodes.count(owner));
        auto ok = nodes[owner]->replicate(key, value);
        ASSERT_TRUE(ok);
    }

    // verify each key lives only on its owner (no peers configured)
    for (int i = 0; i < N; ++i) {
        std::string key = "key-" + std::to_string(i);
        auto owner = ring.get_node(key);
        for (const auto &entry : nodes) {
            std::string v;
            bool has = entry.second->get_local(key, v);
            if (entry.first == owner) EXPECT_TRUE(has);
            else EXPECT_FALSE(has);
        }
    }

    // create rebalancer and compute expected migrations when adding node3
    Rebalancer rebalancer(ring, nodes);
    auto expected = rebalancer.compute_migrations_for_add("node3");
    EXPECT_FALSE(expected.empty()); // some keys should move to the new node

    // execute migrations
    size_t moved = rebalancer.execute_migrations_for_add("node3");
    EXPECT_EQ(moved, expected.size());

    // After migration: for each expected pair, new node has key and from node does not
    for (const auto &p : expected) {
        const auto &from = p.first;
        const auto &key = p.second;
        std::string v;
        EXPECT_TRUE(nodes["node3"]->get_local(key, v));
        EXPECT_FALSE(nodes[from]->get_local(key, v));
    }

    // cleanup
    n1->stop(); n2->stop(); n3->stop();
}
