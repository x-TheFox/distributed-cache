#include "replication/raft.h"
#include <thread>
#include <random>
#include <iostream>

namespace replication {

RaftNode::RaftNode(const std::string& id, const std::vector<std::string>& peers)
    : running_(false), state_(State::Follower), current_term_(0), id_(id), peers_(peers),
      election_timeout_min_ms_(150), election_timeout_max_ms_(300) {}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    // Start election loop
    election_thread_ = std::thread(&RaftNode::electionLoop, this);
}

void RaftNode::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    if (election_thread_.joinable()) election_thread_.join();
}

void RaftNode::electionLoop() {
    // Simple election logic: single-node becomes leader after timeout
    std::mt19937 rng(std::random_device{}());
    while (running_) {
        int timeout = randomizedElectionTimeoutMs();
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        if (!running_) break;

        // If there are no peers, become leader immediately
        if (peers_.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Leader;
            current_term_++;
            return; // leader elected; election loop can stop for now
        }

        // TODO: implement RequestVote RPC and majority voting across peers
    }
}

int RaftNode::randomizedElectionTimeoutMs() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(election_timeout_min_ms_, election_timeout_max_ms_);
    return dist(gen);
}

bool RaftNode::appendEntry(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || state_ != State::Leader) return false;
    // TODO: persist entry to WAL and replicate to followers
    (void)data;
    return true;
}

bool RaftNode::isLeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == State::Leader;
}

void RaftNode::setElectionTimeoutMs(int ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    election_timeout_min_ms_ = ms;
    election_timeout_max_ms_ = ms * 2;
}

} // namespace replication
