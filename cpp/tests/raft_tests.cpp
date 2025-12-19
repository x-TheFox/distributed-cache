#include "replication/raft.h"
#include "replication/wal.h"
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
    // Hook up append entries RPC so leader's append is applied on the follower
    a.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry>& entries){
        if (peer_id == "b") return b.handleAppendEntries(term, leader_id, prev_index, prev_term, entries);
        return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
    });
    b.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry>& entries){
        if (peer_id == "a") return a.handleAppendEntries(term, leader_id, prev_index, prev_term, entries);
        return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
    });

    // ---------- New tests for conflict/backtracking ----------
    // First, unit test that follower returns conflict hint when prev_log_term mismatches
    {
        // Prepare WAL for follower with a conflicting term at index 1
        std::string path_b = "/tmp/raft_test_conflict_b.wal";
        // remove potential leftover file
        std::remove(path_b.c_str());
        WAL wb(path_b);
        wb.append(1, "x");
        wb.append(3, "b_conflict");

        RaftNode follower("fb", {});
        follower.setWalPath(path_b);
        follower.start();

        // Create an AppendEntries call with prev_index=1 and prev_term=2 (mismatch)
        RaftNode::Entry e{2, "leader_entry"};
        auto res = follower.handleAppendEntries(2, "leader", 1, 2, std::vector<RaftNode::Entry>{e});
        EXPECT_FALSE(res.success);
        EXPECT_EQ(res.conflict_term, 3u);
        EXPECT_EQ(res.conflict_index, 1u);
        follower.stop();
        std::remove(path_b.c_str());
    }

    // Integration-style test: leader should use conflict hint to backtrack and eventually replicate
    {
        std::string path_a = "/tmp/raft_test_conflict_a.wal";
        std::string path_b = "/tmp/raft_test_conflict_b2.wal";
        std::remove(path_a.c_str()); std::remove(path_b.c_str());
        WAL wa(path_a); WAL wb(path_b);
        wa.append(1, "x"); wa.append(2, "a2");
        wb.append(1, "x"); wb.append(3, "b2");

        RaftNode leader("la", {"lb"});
        RaftNode follower("lb", {"la"});

        leader.setWalPath(path_a);
        follower.setWalPath(path_b);

        // Hook RPCs
        leader.setPeerRequestVoteFn([&](const std::string& peer_id, uint64_t term, const std::string& candidate){ if (peer_id=="lb") return follower.handleRequestVote(term, candidate); return false; });
        follower.setPeerRequestVoteFn([&](const std::string& peer_id, uint64_t term, const std::string& candidate){ if (peer_id=="la") return leader.handleRequestVote(term, candidate); return false; });
        leader.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry>& entries){ if (peer_id=="lb") return follower.handleAppendEntries(term, leader_id, prev_index, prev_term, entries); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });
        follower.setPeerAppendEntriesFn([&](const std::string& peer_id, uint64_t term, const std::string& leader_id, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry>& entries){ if (peer_id=="la") return leader.handleAppendEntries(term, leader_id, prev_index, prev_term, entries); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });

        leader.setElectionTimeoutMs(30);
        follower.setElectionTimeoutMs(150);
        leader.setReplicationIntervalMsPublic(10);

        leader.start(); follower.start();

        // Wait for leader election
        bool leader_is_la = false;
        for (int i=0;i<40;++i) { if (leader.isLeader()) { leader_is_la=true; break; } std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
        ASSERT_TRUE(leader_is_la);

        // Append a new entry and ensure it eventually commits and followers catch up
        bool ok = leader.appendEntry("leader_new");
        EXPECT_TRUE(ok);

        // Ensure logs converge
        EXPECT_EQ(leader.logSize(), follower.logSize());
        for (size_t i=0;i<leader.logSize();++i) EXPECT_EQ(leader.getLogEntry(i), follower.getLogEntry(i));

        leader.stop(); follower.stop();
        std::remove(path_a.c_str()); std::remove(path_b.c_str());
    }

    // Make A have a shorter timeout so it should win deterministically
    a.setElectionTimeoutMs(30);
    b.setElectionTimeoutMs(150);

    // Make replication quick
    a.setReplicationIntervalMs(10);

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

    // Leader appends an entry and appendEntry blocks until commit
    bool ok = a.appendEntry("value1");
    EXPECT_TRUE(ok);

    // After commit, follower should have the entry
    EXPECT_EQ(b.logSize(), 1u);
    EXPECT_EQ(b.getLogEntry(0), "value1");

    // Ensure leader's commit index advanced
    EXPECT_GT(a.getCommitIndexForTest(), 0u);

    a.stop();
    b.stop();
}

// Expanded deterministic conflict/backtracking tests (multiple scenarios)
TEST(Raft, ConflictBacktrackingMultipleCases) {
    // Scenario A: follower missing entries (prev_index too large)
    {
        std::string pa = "/tmp/raft_conflict_A_a.wal";
        std::string pb = "/tmp/raft_conflict_A_b.wal";
        std::remove(pa.c_str()); std::remove(pb.c_str());
        WAL wa(pa); WAL wb(pb);
        wa.append(1, "x"); wa.append(2, "a2");
        wb.append(1, "x"); // follower shorter

        RaftNode leader("la1", {"lb1"});
        RaftNode follower("lb1", {"la1"});
        leader.setWalPath(pa); follower.setWalPath(pb);
        leader.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="lb1") return follower.handleRequestVote(term, c); return false; });
        follower.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="la1") return leader.handleRequestVote(term, c); return false; });
        leader.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="lb1") return follower.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });
        follower.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="la1") return leader.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });

        leader.setElectionTimeoutMs(20); follower.setElectionTimeoutMs(150); leader.setReplicationIntervalMs(10);
        leader.start(); follower.start();
        // wait leader
        bool elected=false; for (int i=0;i<40;++i){ if (leader.isLeader()){ elected=true; break; } std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
        ASSERT_TRUE(elected);
        ASSERT_TRUE(leader.appendEntry("newA"));
        bool matchedA = false;
        for (int w=0; w<200; ++w) {
            size_t lsz = leader.logSize();
            if (lsz > 0 && follower.logSize() >= lsz) {
                if (leader.getLogEntry(lsz-1) == follower.getLogEntry(lsz-1) && leader.getLogEntry(lsz-1) == "newA") { matchedA = true; break; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!matchedA) {
            std::cerr<<"Final logs after timeout (scenario A): leader="<<leader.logSize()<<" follower="<<follower.logSize()<<"\n";
            for (size_t j=0; j< std::max(leader.logSize(), follower.logSize()); ++j) {
                std::string l = (j<leader.logSize()? leader.getLogEntry(j) : std::string("<none>"));
                std::string f = (j<follower.logSize()? follower.getLogEntry(j) : std::string("<none>"));
                std::cerr<<"SCENARIO A idx="<<j<<" leader="<<l<<" follower="<<f<<"\n";
            }
        }
        EXPECT_TRUE(matchedA);
        leader.stop(); follower.stop();
        std::remove(pa.c_str()); std::remove(pb.c_str());
    }

    // Scenario B: follower term mismatch and leader lacks that term
    {
        std::string pa = "/tmp/raft_conflict_B_a.wal";
        std::string pb = "/tmp/raft_conflict_B_b.wal";
        std::remove(pa.c_str()); std::remove(pb.c_str());
        WAL wa(pa); WAL wb(pb);
        wa.append(1, "x"); wa.append(2, "a2"); wa.append(4, "a3");
        wb.append(1, "x"); wb.append(3, "b2"); // conflicting term 3 not present on leader

        RaftNode leader("la2", {"lb2"});
        RaftNode follower("lb2", {"la2"});
        leader.setWalPath(pa); follower.setWalPath(pb);
        leader.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="lb2") return follower.handleRequestVote(term, c); return false; });
        follower.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="la2") return leader.handleRequestVote(term, c); return false; });
        leader.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="lb2") return follower.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });
        follower.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="la2") return leader.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });

        leader.setElectionTimeoutMs(20); follower.setElectionTimeoutMs(150); leader.setReplicationIntervalMs(10);
        leader.start(); follower.start();
        bool elected=false; for (int i=0;i<40;++i){ if (leader.isLeader()){ elected=true; break; } std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
        ASSERT_TRUE(elected);
        ASSERT_TRUE(leader.appendEntry("newB"));
        // Wait for follower to catch up on the committed entry (last entry should match)
        bool matched_last = false;
        for (int w=0; w<200; ++w) {
            size_t lsz = leader.logSize();
            if (lsz > 0 && follower.logSize() >= lsz) {
                if (leader.getLogEntry(lsz-1) == follower.getLogEntry(lsz-1) && leader.getLogEntry(lsz-1) == "newB") { matched_last = true; break; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!matched_last) {
            std::cerr<<"Final logs after timeout (scenario B): leader="<<leader.logSize()<<" follower="<<follower.logSize()<<"\n";
            for (size_t j=0; j< std::max(leader.logSize(), follower.logSize()); ++j) {
                std::string l = (j<leader.logSize()? leader.getLogEntry(j) : std::string("<none>"));
                std::string f = (j<follower.logSize()? follower.getLogEntry(j) : std::string("<none>"));
                std::cerr<<"SCENARIO B idx="<<j<<" leader="<<l<<" follower="<<f<<"\n";
            }
        }
        EXPECT_TRUE(matched_last);
        leader.stop(); follower.stop();
        std::remove(pa.c_str()); std::remove(pb.c_str());
    }

    // Scenario C: follower conflict term exists on leader; leader should backtrack to last index of that term
    {
        std::string pa = "/tmp/raft_conflict_C_a.wal";
        std::string pb = "/tmp/raft_conflict_C_b.wal";
        std::remove(pa.c_str()); std::remove(pb.c_str());
        WAL wa(pa); WAL wb(pb);
        wa.append(1, "x"); wa.append(3, "a2"); wa.append(3, "a3"); wa.append(4, "a4");
        wb.append(1, "x"); wb.append(3, "b2"); // conflict term 3 present on leader (indices 1 and 2)

        RaftNode leader("la3", {"lb3"});
        RaftNode follower("lb3", {"la3"});
        leader.setWalPath(pa); follower.setWalPath(pb);
        leader.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="lb3") return follower.handleRequestVote(term, c); return false; });
        follower.setPeerRequestVoteFn([&](const std::string& peer, uint64_t term, const std::string& c){ if (peer=="la3") return leader.handleRequestVote(term, c); return false; });
        leader.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="lb3") return follower.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });
        follower.setPeerAppendEntriesFn([&](const std::string& peer, uint64_t term, const std::string& lid, size_t pi, uint64_t pt, const std::vector<RaftNode::Entry>& e){ if (peer=="la3") return leader.handleAppendEntries(term, lid, pi, pt, e); return RaftNode::AppendEntriesResult{false,0,0,0,0}; });

        leader.setElectionTimeoutMs(20); follower.setElectionTimeoutMs(150); leader.setReplicationIntervalMs(10);
        leader.start(); follower.start();
        bool elected=false; for (int i=0;i<40;++i){ if (leader.isLeader()){ elected=true; break; } std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
        ASSERT_TRUE(elected);
        ASSERT_TRUE(leader.appendEntry("newC"));
        // Wait up to 2s for logs to converge to avoid transient asynchrony
        // Wait for follower to catch up on the committed entry (last entry should match)
        bool matched_last = false;
        for (int w=0; w<200; ++w) {
            size_t lsz = leader.logSize();
            if (lsz > 0 && follower.logSize() >= lsz) {
                if (leader.getLogEntry(lsz-1) == follower.getLogEntry(lsz-1) && leader.getLogEntry(lsz-1) == "newC") { matched_last = true; break; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!matched_last) {
            std::cerr << "Final logs after timeout (scenario C): leader="<<leader.logSize()<<" follower="<<follower.logSize()<<"\n";
            for (size_t j=0; j< std::max(leader.logSize(), follower.logSize()); ++j) {
                std::string l = (j<leader.logSize()? leader.getLogEntry(j) : std::string("<none>"));
                std::string f = (j<follower.logSize()? follower.getLogEntry(j) : std::string("<none>"));
                std::cerr<<"SCENARIO C idx="<<j<<" leader="<<l<<" follower="<<f<<"\n";
            }
        }
        EXPECT_TRUE(matched_last);
        leader.stop(); follower.stop();
        std::remove(pa.c_str()); std::remove(pb.c_str());
    }
}

