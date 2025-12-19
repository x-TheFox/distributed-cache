#include <gtest/gtest.h>
#include "replication/mock_replicator.h"

TEST(ReplicationSimulator, ReplicatesToPeers) {
    MockReplicatorOptions opts;
    opts.peer_latency = std::chrono::milliseconds(5);
    auto r1 = std::make_shared<MockReplicator>(opts);
    auto r2 = std::make_shared<MockReplicator>(opts);
    auto r3 = std::make_shared<MockReplicator>(opts);

    // wire peers: r1 -> [r2, r3]
    r1->add_peer(r2);
    r1->add_peer(r3);

    EXPECT_TRUE(r1->replicate("k1","v1"));

    std::string val;
    ASSERT_TRUE(r1->get_local("k1", val));
    EXPECT_EQ(val, "v1");

    // wait for async replication to complete (poll with timeout)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    bool ok2 = false, ok3 = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!ok2) ok2 = r2->get_local("k1", val) && val == "v1";
        if (!ok3) ok3 = r3->get_local("k1", val) && val == "v1";
        if (ok2 && ok3) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(ok2);
    EXPECT_TRUE(ok3);

    r1->stop(); r2->stop(); r3->stop();
}
