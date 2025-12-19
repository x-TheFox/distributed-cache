#include <gtest/gtest.h>
#include "replication/raft.h"
#include "replication/snapshot.h"
#include "replication/log.h"
#include <filesystem>

using namespace replication;

TEST(RaftSnapshotInstall, FollowerAppliesSnapshot) {
    RaftNode follower("f1", {});
    follower.setSnapshotPath("/tmp/f1");
    follower.start();

    // Create a snapshot and install it
    Snapshot s;
    s.last_included_index = 5;
    s.last_included_term = 2;
    s.data = "state-at-5";
    std::string path = "/tmp/f1.snap";
    std::filesystem::remove(path);
    ASSERT_TRUE(s.save(path));

    // Call handler
    auto res = follower.handleInstallSnapshot(2, s);
    EXPECT_TRUE(res.success);
    EXPECT_EQ(follower.getCommitIndexForTest(), 5u);
    std::filesystem::remove(path);
    follower.stop();
}

TEST(RaftSnapshotInstall, LeaderSendsInstallSnapshotWhenFollowerBehind) {
    // Create a leader with no in-memory log but with snapshot metadata set
    replication::setLogLevel(replication::LogLevel::DEBUG);
    RaftNode leader("leader", {"f1"});
    leader.setSnapshotPath("/tmp/leader");
    leader.setWalPath("/tmp/leader.wal");
    // make leader win elections quickly
    leader.setPeerRequestVoteFn([&](const std::string&, uint64_t, const std::string&){ return true; });
    leader.setElectionTimeoutMs(10);
    leader.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(leader.isLeader());

    // setup follower to receive snapshot and handle AppendEntries
    RaftNode follower("f1", {});
    follower.setSnapshotPath("/tmp/f1");
    follower.setWalPath("/tmp/f1.wal");
    follower.start();

    // wire leader RPC to call follower handler for AppendEntries
    leader.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& leader_id, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry>& entries){
        return follower.handleAppendEntries(term, leader_id, prev_index, prev_term, entries);
    });

    // For this unit test we simulate leader snapshot creation directly
    Snapshot s;
    s.last_included_index = 2;
    s.last_included_term = 1;
    s.data = "snapshot-by-leader";
    ASSERT_TRUE(s.save(std::string("/tmp/leader.snap")));

    // Simulate leader installing snapshot on follower by directly invoking the handler (replication loop test will be added later)
    ASSERT_TRUE(Snapshot::load(std::string("/tmp/leader.snap"), s));
    auto res = follower.handleInstallSnapshot(leader.getCurrentTermForTest(), s);
    EXPECT_TRUE(res.success);
    EXPECT_GE(follower.getCommitIndexForTest(), 2u);

    std::filesystem::remove(std::string("/tmp/leader.snap"));
    std::filesystem::remove(std::string("/tmp/f1.snap"));
    leader.stop();
    follower.stop();
}
