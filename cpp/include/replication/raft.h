#ifndef RAFT_H
#define RAFT_H

#include <string>
#include <vector>
#include <mutex>

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

private:
    mutable std::mutex mutex_;
    bool running_;
    bool leader_;
    std::string id_;
    std::vector<std::string> peers_;
};

} // namespace replication

#endif // RAFT_H
