#include <gtest/gtest.h>
#include "replication/mock_replicator.h"

TEST(ReplicationFailover, NodeDownAndCatchUp) {
    MockReplicatorOptions opts;
    opts.peer_latency = std::chrono::milliseconds(10);
    auto r1 = std::make_shared<MockReplicator>(opts);
    auto r2 = std::make_shared<MockReplicator>(opts);
    auto r3 = std::make_shared<MockReplicator>(opts);

    // r1 peers: r2 only (r3 is down)
    r1->add_peer(r2);

    // r3 is down (not added yet). Do writes on r1
    EXPECT_TRUE(r1->replicate("k1","v1"));
    std::string v;

    // poll for r2 to receive async replication
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    bool ok2 = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (r2->get_local("k1", v) && v == "v1") { ok2 = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(ok2);

    // Now simulate r3 joining and performing catch-up by copying from r1
    r1->add_peer(r3);
    // run a manual sync: copy keys from r1 to r3 by reading r1 store (not exposed publicly in prod, but for test we use get_local from r1)
    std::string v1;
    ASSERT_TRUE(r1->get_local("k1", v1));
    EXPECT_TRUE(r3->replicate("k1", v1));

    std::string v3;
    // poll for r3 to get the value
    auto deadline2 = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    bool ok3 = false;
    while (std::chrono::steady_clock::now() < deadline2) {
        if (r3->get_local("k1", v3) && v3 == "v1") { ok3 = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(ok3);

    r1->stop(); r2->stop(); r3->stop();
}