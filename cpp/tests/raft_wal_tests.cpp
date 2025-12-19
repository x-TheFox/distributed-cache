#include "replication/raft.h"
#include <gtest/gtest.h>
#include <cstdio>

using namespace replication;

static std::string tmpfile(const std::string& name) {
    return std::string("/tmp/") + name + ".wal";
}

TEST(RaftWAL, PersistAndReplay) {
    auto path = tmpfile("raft_wal_test");
    // remove any existing
    std::remove(path.c_str());

    RaftNode node("n1", {});
    node.setWalPath(path);
    node.setElectionTimeoutMs(10);
    node.start();
    // Wait up to 200ms for election
    for (int i = 0; i < 20 && !node.isLeader(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(node.isLeader());
    ASSERT_TRUE(node.appendEntry("value1"));
    node.stop();

    // New node reading same WAL should have the entry replayed
    RaftNode node2("n2", {});
    node2.setWalPath(path);
    node2.start();
    EXPECT_EQ(node2.logSize(), 1u);
    EXPECT_EQ(node2.getLogEntry(0), "value1");
    node2.stop();

    std::remove(path.c_str());
}
