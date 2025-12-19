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

TEST(Raft, TwoNodeDeterministicElection) {
    RaftNode a("a", {"b"});
    RaftNode b("b", {"a"});

    // Hook up peer RPC callables to directly call handleRequestVote on the peer
    a.setPeerRequestVoteFn([&](const std::string& peer_id, uint64_t term, const std::string& candidate){
        if (peer_id == "b") return b.handleRequestVote(term, candidate);
        return false;
    });

    b.setPeerRequestVoteFn([&](const std::string& peer_id, uint64_t term, const std::string& candidate){
        if (peer_id == "a") return a.handleRequestVote(term, candidate);
        return false;
    });

    // Hook up append entries RPC so leader's append is applied on the follower
    a.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries){
        if (peer_id == "b") return b.handleAppendEntries(term, leader_id, entries);
        return false;
    });
    b.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries){
        if (peer_id == "a") return a.handleAppendEntries(term, leader_id, entries);
        return false;
    });

    // Make A have a shorter timeout so it should win deterministically
    a.setElectionTimeoutMs(30);
    b.setElectionTimeoutMs(150);

    a.start();
    b.start();

    // Wait up to ~1s for election outcome
    bool a_leader = false;
    for (int i = 0; i < 40; ++i) {
        if (a.isLeader()) { a_leader = true; break; }
        if (b.isLeader()) break; // if b became leader it's ok
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    ASSERT_TRUE(a_leader);

    // Leader appends an entry and replication should succeed
    bool ok = a.appendEntry("value1");
    EXPECT_TRUE(ok);

    // Wait briefly for replication
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(b.logSize(), 1u);
    EXPECT_EQ(b.getLogEntry(0), "value1");

    a.stop();
    b.stop();
}

