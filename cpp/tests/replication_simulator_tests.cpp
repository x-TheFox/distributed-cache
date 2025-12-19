#include <gtest/gtest.h>
#include "replication/mock_replicator.h"

TEST(ReplicationSimulator, ReplicatesToPeers) {
    auto r1 = std::make_shared<MockReplicator>();
    auto r2 = std::make_shared<MockReplicator>();
    auto r3 = std::make_shared<MockReplicator>();

    // wire peers: r1 -> [r2, r3]
    r1->add_peer(r2);
    r1->add_peer(r3);

    EXPECT_TRUE(r1->replicate("k1","v1"));

    std::string val;
    EXPECT_TRUE(r1->get_local("k1", val));
    EXPECT_EQ(val, "v1");

    EXPECT_TRUE(r2->get_local("k1", val));
    EXPECT_EQ(val, "v1");

    EXPECT_TRUE(r3->get_local("k1", val));
    EXPECT_EQ(val, "v1");
}
