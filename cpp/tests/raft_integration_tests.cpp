#include "replication/raft.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <map>
#include <set>

using namespace replication;

// Simple in-memory network simulator to allow/deny RPCs between nodes
struct NetSim {
    std::map<std::string, std::set<std::string>> links;

    void allow(const std::string &a, const std::string &b) {
        links[a].insert(b);
    }
    void block(const std::string &a, const std::string &b) {
        links[a].erase(b);
    }
    bool ok(const std::string &a, const std::string &b) const {
        auto it = links.find(a);
        if (it == links.end()) return false;
        return it->second.count(b) > 0;
    }
};

TEST(RaftIntegration, ThreeNodeFullReplication) {
    RaftNode a("a", {"b","c"});
    RaftNode b("b", {"a","c"});
    RaftNode c("c", {"a","b"});

    NetSim net;
    // fully connected
    net.allow("a","b"); net.allow("a","c");
    net.allow("b","a"); net.allow("b","c");
    net.allow("c","a"); net.allow("c","b");

    // hook RPCs through NetSim
    a.setPeerRequestVoteFn([&](const std::string &peer, uint64_t term, const std::string &cand){
        if (!net.ok("a", peer)) return false;
        if (peer=="b") return b.handleRequestVote(term, cand);
        if (peer=="c") return c.handleRequestVote(term, cand);
        return false;
    });
    b.setPeerRequestVoteFn([&](const std::string &peer, uint64_t term, const std::string &cand){
        if (!net.ok("b", peer)) return false;
        if (peer=="a") return a.handleRequestVote(term, cand);
        if (peer=="c") return c.handleRequestVote(term, cand);
        return false;
    });
    c.setPeerRequestVoteFn([&](const std::string &peer, uint64_t term, const std::string &cand){
        if (!net.ok("c", peer)) return false;
        if (peer=="a") return a.handleRequestVote(term, cand);
        if (peer=="b") return b.handleRequestVote(term, cand);
        return false;
    });

    a.setPeerAppendEntriesFn([&](const std::string &peer, uint64_t term, const std::string &leader, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry> &entries){
        if (!net.ok("a", peer)) return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
        if (peer=="b") return b.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        if (peer=="c") return c.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
    });
    b.setPeerAppendEntriesFn([&](const std::string &peer, uint64_t term, const std::string &leader, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry> &entries){
        if (!net.ok("b", peer)) return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
        if (peer=="a") return a.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        if (peer=="c") return c.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
    });
    c.setPeerAppendEntriesFn([&](const std::string &peer, uint64_t term, const std::string &leader, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry> &entries){
        if (!net.ok("c", peer)) return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
        if (peer=="a") return a.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        if (peer=="b") return b.handleAppendEntries(term, leader, prev_index, prev_term, entries);
        return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
    });

    a.setReplicationIntervalMs(10);
    b.setReplicationIntervalMs(10);
    c.setReplicationIntervalMs(10);

    a.setElectionTimeoutMs(30);
    b.setElectionTimeoutMs(80);
    c.setElectionTimeoutMs(80);

    a.start(); b.start(); c.start();

    // wait for some node to become leader
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    bool leader_found = a.isLeader() || b.isLeader() || c.isLeader();
    ASSERT_TRUE(leader_found);

    RaftNode *leader = nullptr;
    if (a.isLeader()) leader = &a;
    if (b.isLeader()) leader = &b;
    if (c.isLeader()) leader = &c;

    ASSERT_NE(leader, nullptr);

    // Append an entry and ensure eventual replication to all nodes
    ASSERT_TRUE(leader->appendEntry("x1"));

    // wait for replication
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(a.logSize(), 1u);
    EXPECT_EQ(b.logSize(), 1u);
    EXPECT_EQ(c.logSize(), 1u);

    a.stop(); b.stop(); c.stop();
}

TEST(RaftIntegration, LeaderFailoverAndRecovery) {
    RaftNode a("a", {"b","c"});
    RaftNode b("b", {"a","c"});
    RaftNode c("c", {"a","b"});

    NetSim net;
    // fully connected initially
    net.allow("a","b"); net.allow("a","c");
    net.allow("b","a"); net.allow("b","c");
    net.allow("c","a"); net.allow("c","b");

    auto attach = [&](RaftNode &self, const std::string &self_id){
        self.setPeerRequestVoteFn([&](const std::string &peer, uint64_t term, const std::string &cand){
            if (!net.ok(self_id, peer)) return false;
            if (peer=="a") return a.handleRequestVote(term, cand);
            if (peer=="b") return b.handleRequestVote(term, cand);
            if (peer=="c") return c.handleRequestVote(term, cand);
            return false;
        });
        self.setPeerAppendEntriesFn([&](const std::string &peer, uint64_t term, const std::string &leader, size_t prev_index, uint64_t prev_term, const std::vector<RaftNode::Entry> &entries){
            if (!net.ok(self_id, peer)) return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
            if (peer=="a") return a.handleAppendEntries(term, leader, prev_index, prev_term, entries);
            if (peer=="b") return b.handleAppendEntries(term, leader, prev_index, prev_term, entries);
            if (peer=="c") return c.handleAppendEntries(term, leader, prev_index, prev_term, entries);
            return RaftNode::AppendEntriesResult{false, 0, 0, 0, 0};
        });
    };

    attach(a, "a"); attach(b, "b"); attach(c, "c");

    a.setReplicationIntervalMs(10);
    b.setReplicationIntervalMs(10);
    c.setReplicationIntervalMs(10);

    a.setElectionTimeoutMs(30);
    b.setElectionTimeoutMs(80);
    c.setElectionTimeoutMs(80);

    a.start(); b.start(); c.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // identify leader
    RaftNode *leader = nullptr;
    if (a.isLeader()) leader = &a;
    if (b.isLeader()) leader = &b;
    if (c.isLeader()) leader = &c;
    ASSERT_NE(leader, nullptr);

    // leader appends an entry
    EXPECT_TRUE(leader->appendEntry("before_down"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // simulate crash by partitioning the leader away (don't call stop, keep state intact but unreachable)
    std::string orig_leader_id = leader==&a?"a": leader==&b?"b":"c";
    if (leader==&a) { net.block("b","a"); net.block("c","a"); net.block("a","b"); net.block("a","c"); }
    if (leader==&b) { net.block("a","b"); net.block("c","b"); net.block("b","a"); net.block("b","c"); }
    if (leader==&c) { net.block("a","c"); net.block("b","c"); net.block("c","a"); net.block("c","b"); }

    // allow others to elect
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // check new leader among remaining two
    int leaders = (a.isLeader()?1:0) + (b.isLeader()?1:0) + (c.isLeader()?1:0);
    EXPECT_EQ(leaders, 1);

    // append entry on new leader (allow eventual replication; appendEntry may fail briefly during elections)
    RaftNode *new_leader = nullptr;
    if (a.isLeader()) new_leader = &a;
    if (b.isLeader()) new_leader = &b;
    if (c.isLeader()) new_leader = &c;
    ASSERT_NE(new_leader, nullptr);
    bool ok = new_leader->appendEntry("after_fail");
    if (!ok) {
        // wait up to 5s for replication to majority
        int waited = 0;
        const int wait_step = 50;
        while (waited < 5000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_step));
            waited += wait_step;
            size_t s1 = a.logSize();
            size_t s2 = b.logSize();
            size_t s3 = c.logSize();
            size_t majority = (s1 + s2 + s3) / 3; // rough check
            if (s1 >= 2 || s2 >= 2 || s3 >= 2) break;
        }
    }

    // Confirm other nodes in the majority partition have replicated the entry
    if (orig_leader_id == "a") {
        EXPECT_EQ(b.logSize(), 2u);
        EXPECT_EQ(c.logSize(), 2u);
    } else if (orig_leader_id == "b") {
        EXPECT_EQ(a.logSize(), 2u);
        EXPECT_EQ(c.logSize(), 2u);
    } else {
        EXPECT_EQ(a.logSize(), 2u);
        EXPECT_EQ(b.logSize(), 2u);
    }

    // re-allow links and let the old leader catch up
    if (orig_leader_id == "a") { net.allow("a","b"); net.allow("a","c"); net.allow("b","a"); net.allow("c","a"); }
    if (orig_leader_id == "b") { net.allow("b","a"); net.allow("b","c"); net.allow("a","b"); net.allow("c","b"); }
    if (orig_leader_id == "c") { net.allow("c","a"); net.allow("c","b"); net.allow("a","c"); net.allow("b","c"); }

    // ensure RPC hooks are still attached
    if (orig_leader_id == "a") attach(a, "a");
    if (orig_leader_id == "b") attach(b, "b");
    if (orig_leader_id == "c") attach(c, "c");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // all nodes should eventually have at least both entries
    EXPECT_GE(a.logSize(), 2u);
    EXPECT_GE(b.logSize(), 2u);
    EXPECT_GE(c.logSize(), 2u);

    a.stop(); b.stop(); c.stop();
}

TEST(RaftIntegration, PartitionPreventsMinorityElection) {
    RaftNode a("a", {"b","c"});
    RaftNode b("b", {"a","c"});
    RaftNode c("c", {"a","b"});

    NetSim net;
    // Partition: {a} vs {b,c}
    net.allow("a","a"); net.allow("b","b"); net.allow("c","c");
    net.allow("b","c"); net.allow("c","b");

    auto attach = [&](RaftNode &self, const std::string &self_id){
        self.setPeerRequestVoteFn([&](const std::string &peer, uint64_t term, const std::string &cand){
            if (!net.ok(self_id, peer)) return false;
            if (peer=="a") return a.handleRequestVote(term, cand);
            if (peer=="b") return b.handleRequestVote(term, cand);
            if (peer=="c") return c.handleRequestVote(term, cand);
            return false;
        });
    };

    attach(a, "a"); attach(b, "b"); attach(c, "c");

    a.setElectionTimeoutMs(30);
    b.setElectionTimeoutMs(80);
    c.setElectionTimeoutMs(80);

    a.start(); b.start(); c.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // In this partition, {b,c} may elect a leader but {a} alone shouldn't
    bool a_leader = a.isLeader();
    bool b_or_c_leader = b.isLeader() || c.isLeader();

    EXPECT_FALSE(a_leader);
    EXPECT_TRUE(b_or_c_leader);

    a.stop(); b.stop(); c.stop();
}
