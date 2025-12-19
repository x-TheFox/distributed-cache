#include "replication/raft.h"
#include <thread>

namespace replication {

RaftNode::RaftNode(const std::string& id, const std::vector<std::string>& peers)
    : running_(false), leader_(false), id_(id), peers_(peers) {}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    // TODO: spawn election + replication worker threads
}

void RaftNode::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    // TODO: join worker threads
}

bool RaftNode::appendEntry(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || !leader_) return false;
    // TODO: persist entry to WAL and replicate to followers
    (void)data;
    return true;
}

bool RaftNode::isLeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return leader_;
}

} // namespace replication
