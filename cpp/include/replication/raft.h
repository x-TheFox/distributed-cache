#ifndef RAFT_H
#define RAFT_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace replication {

class RaftNode {
public:
    explicit RaftNode(const std::string& id, const std::vector<std::string>& peers);
    ~RaftNode();

    // Lifecycle
    void start();
    void stop();

    // Append an entry to the log (returns true if accepted by leader)
    bool appendEntry(const std::string& data);

    // Query
    bool isLeader() const;

    // Testing hooks
    void setElectionTimeoutMs(int ms);

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
};

} // namespace replication

#endif // RAFT_H
