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

    // AppendEntries RPC
    using PeerAppendEntriesFn = std::function<bool(const std::string& peer_id, uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries)>;
    void setPeerAppendEntriesFn(PeerAppendEntriesFn fn);
    bool handleAppendEntries(uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries);

    // Append locally (used by leader)
    bool appendEntry(const std::string& data);

    // For tests: inspect log
    size_t logSize() const;
    std::string getLogEntry(size_t idx) const;

private:
    // In-memory log entries
    std::vector<std::string> log_;

    // RPC callable
    PeerAppendEntriesFn peer_append_entries_fn_;

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
