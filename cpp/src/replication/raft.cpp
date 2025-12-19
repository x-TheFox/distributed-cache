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
    // Very small, simple election implementation for tests: candidate requests votes from peers via peer_request_vote_fn_
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

        // Start election
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Candidate;
            current_term_++;
            voted_for_ = id_;
            voted_term_ = current_term_;
        }

        int votes = 1; // self vote
        uint64_t term_snapshot = current_term_;

            // Request votes from peers via configured callable
        if (peer_request_vote_fn_) {
            for (const auto& p : peers_) {
                bool granted = false;
                try {
                    granted = peer_request_vote_fn_(p, term_snapshot, id_);
                } catch (...) {
                    granted = false;
                }
                if (granted) votes++;
            }
        }

        // Check majority
        size_t total_nodes = peers_.size() + 1;
        if (votes > static_cast<int>(total_nodes / 2)) {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Leader;
            // stop election loop for now; in a real implementation we'd continue sending heartbeats
            return;
        }

        // If not elected, try again next timeout
    }
}

void RaftNode::setPeerAppendEntriesFn(PeerAppendEntriesFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_append_entries_fn_ = std::move(fn);
}

bool RaftNode::handleAppendEntries(uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (term < current_term_) return false;
    if (term > current_term_) {
        current_term_ = term;
        state_ = State::Follower;
    }
    // Append entries to local log
    for (const auto& e : entries) log_.push_back(e);
    return true;
}

size_t RaftNode::logSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return log_.size();
}

std::string RaftNode::getLogEntry(size_t idx) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx >= log_.size()) return "";
    return log_[idx];
}

bool RaftNode::appendEntry(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || state_ != State::Leader) return false;
    // Append locally
    log_.push_back(data);
    uint64_t term_snapshot = current_term_;

    // Replicate synchronously using peer_append_entries_fn_
    int successes = 1;
    if (peer_append_entries_fn_) {
        for (const auto& p : peers_) {
            bool ok = false;
            try {
                ok = peer_append_entries_fn_(p, term_snapshot, id_, std::vector<std::string>{data});
            } catch (...) {
                ok = false;
            }
            if (ok) successes++;
        }
    }

    size_t total_nodes = peers_.size() + 1;
    return successes > static_cast<int>(total_nodes / 2);
}


int RaftNode::randomizedElectionTimeoutMs() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(election_timeout_min_ms_, election_timeout_max_ms_);
    return dist(gen);
}

void RaftNode::setPeerRequestVoteFn(PeerRequestVoteFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_request_vote_fn_ = std::move(fn);
}

bool RaftNode::handleRequestVote(uint64_t term, const std::string& candidate_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (term < current_term_) return false;
    if (term > current_term_) {
        current_term_ = term;
        voted_for_.clear();
        voted_term_ = 0;
        state_ = State::Follower;
    }
    if (voted_term_ != current_term_ || voted_for_.empty()) {
        voted_for_ = candidate_id;
        voted_term_ = current_term_;
        return true;
    }
    return false;
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
