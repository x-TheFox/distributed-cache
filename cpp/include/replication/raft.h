#ifndef RAFT_H
#define RAFT_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace replication {

class RaftNode {
public:
    explicit RaftNode(const std::string& id, const std::vector<std::string>& peers);
    ~RaftNode();

    // Lifecycle
    void start();
    void stop();

    // Query
    bool isLeader() const;

    // Testing hooks
    void setElectionTimeoutMs(int ms);

    // RPC glue for tests: set a callable to invoke RequestVote on a peer
    using PeerRequestVoteFn = std::function<bool(const std::string& peer_id, uint64_t term, const std::string& candidate_id)>;
    void setPeerRequestVoteFn(PeerRequestVoteFn fn);

    // Direct RPC handler used by tests (simulates receiving RequestVote)
    bool handleRequestVote(uint64_t term, const std::string& candidate_id);

    // Log entry type with per-entry term
    struct Entry { uint64_t term; std::string data; };

    // AppendEntries RPC: include prev log index/term and entries; result includes success, current term, and match_index
    struct AppendEntriesResult { bool success; uint64_t term; size_t match_index; uint64_t conflict_term; size_t conflict_index; };
    using PeerAppendEntriesFn = std::function<AppendEntriesResult(const std::string& peer_id, uint64_t term, const std::string& leader_id, size_t prev_log_index, uint64_t prev_log_term, const std::vector<Entry>& entries)>;
    void setPeerAppendEntriesFn(PeerAppendEntriesFn fn);
    AppendEntriesResult handleAppendEntries(uint64_t term, const std::string& leader_id, size_t prev_log_index, uint64_t prev_log_term, const std::vector<Entry>& entries);

    // Test helpers
    uint64_t getCurrentTermForTest() const { std::lock_guard<std::mutex> lock(mutex_); return current_term_; }
    void updateLastAppendTimeForTest() { std::lock_guard<std::mutex> lock(mutex_); last_append_time_ = std::chrono::steady_clock::now(); }

    // Append locally (used by leader)
    bool appendEntry(const std::string& data);

    // For tests: inspect log
    size_t logSize() const;
    std::string getLogEntry(size_t idx) const;

    // WAL support
    void setWalPath(const std::string& path);

    // Log compaction (testing & maintenance helper): drop `count` entries from log head and compact WAL
    // Note: this is a simple compaction helper for tests; full snapshotting/sync will be more comprehensive.
    bool compactLogPrefix(size_t count);

    // Async replication control
    void setReplicationIntervalMs(int ms);

    // Helper for tests
    size_t getCommitIndexForTest() const { std::lock_guard<std::mutex> lock(mutex_); return commit_index_; }

private:
    // In-memory log entries (term + data)
    std::vector<Entry> log_;

    // WAL (optional)
    std::unique_ptr<class WAL> wal_;

    // RPC callable
    PeerAppendEntriesFn peer_append_entries_fn_;

    // Replication state
    std::unordered_map<std::string, size_t> match_index_;
    std::unordered_map<std::string, size_t> next_index_;
    size_t commit_index_;
    std::thread replication_thread_;
    std::atomic<bool> replication_running_;
    std::condition_variable replication_cv_;
    std::mutex replication_mutex_;
    int replication_interval_ms_;
    // heuristic to detect leader isolation
    int consecutive_failed_replication_rounds_ = 0;
    int max_consecutive_failed_rounds_before_stepdown_ = 3;

private:
    enum class State {Follower, Candidate, Leader};

    void electionLoop();
    int randomizedElectionTimeoutMs() const;

    mutable std::mutex mutex_;
    std::atomic<bool> running_;
    State state_;
    uint64_t current_term_;
    std::string id_;
    std::vector<std::string> peers_;

    // Election timing
    int election_timeout_min_ms_;
    int election_timeout_max_ms_;

    // Track last AppendEntries received (heartbeats)
    std::chrono::steady_clock::time_point last_append_time_;

    // Snapshot/compaction metadata (last included prefix index & term)
    size_t last_included_index_ = 0;
    uint64_t last_included_term_ = 0;

    // Worker
    std::thread election_thread_;

    // Voting state
    std::string voted_for_;
    uint64_t voted_term_;

    // RPC callable
    PeerRequestVoteFn peer_request_vote_fn_;
};

} // namespace replication

#endif // RAFT_H
