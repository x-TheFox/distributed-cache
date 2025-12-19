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
    EXPECT_TRUE(r2->get_local("k1", v));
    EXPECT_EQ(v, "v1");

    // Now simulate r3 joining and performing catch-up by copying from r1
    r1->add_peer(r3);
    // run a manual sync: copy keys from r1 to r3 by reading r1 store (not exposed publicly in prod, but for test we use get_local from r1)
    std::string v1;
    ASSERT_TRUE(r1->get_local("k1", v1));
    EXPECT_TRUE(r3->replicate("k1", v1));

    std::string v3;
    EXPECT_TRUE(r3->get_local("k1", v3));
    EXPECT_EQ(v3, "v1");

    r1->stop(); r2->stop(); r3->stop();
}