#include "replication/raft.h"
#include <gtest/gtest.h>

using namespace replication;

TEST(Raft, ConstructStartStop) {
    RaftNode node("node1", {});
    EXPECT_FALSE(node.isLeader());
    node.start();
    // leader election not implemented yet; ensure node is running and stable
    EXPECT_FALSE(node.isLeader());
    node.stop();
}
