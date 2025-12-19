#include "replication/raft.h"
#include <gtest/gtest.h>
#include <thread>

using namespace replication;

TEST(Raft, ConstructStartStop) {
    RaftNode node("node1", {});
    EXPECT_FALSE(node.isLeader());
    node.start();
    node.stop();
}

TEST(Raft, SingleNodeBecomesLeader) {
    RaftNode node("node1", {});
    node.setElectionTimeoutMs(20); // short timeout for test
    node.start();
    // Wait up to 200ms for election
    for (int i = 0; i < 20 && !node.isLeader(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(node.isLeader());
    node.stop();
}
