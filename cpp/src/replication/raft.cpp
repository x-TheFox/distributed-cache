#include "replication/raft.h"
#include <thread>
#include <random>
#include <iostream>
#include "replication/wal.h"

namespace replication {

RaftNode::RaftNode(const std::string& id, const std::vector<std::string>& peers)
    : running_(false), state_(State::Follower), current_term_(0), id_(id), peers_(peers),
      election_timeout_min_ms_(150), election_timeout_max_ms_(300), commit_index_(0), replication_running_(false), replication_interval_ms_(100) {}

RaftNode::~RaftNode() {
    stop();
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
            // Load WAL if present
            if (wal_) {
                auto entries = wal_->replay();
                for (const auto &e : entries) log_.push_back(e);
            }
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
            // Initialize leader's replication bookkeeping
            for (const auto &p : peers_) {
                next_index_[p] = log_.size();
                match_index_[p] = 0;
            }
            // stop election loop for now; replication will continue in replication thread
            return;
        }

        // If not elected, try again next timeout
    }
}

void RaftNode::setPeerAppendEntriesFn(PeerAppendEntriesFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_append_entries_fn_ = std::move(fn);
}

void RaftNode::setWalPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    wal_ = std::make_unique<WAL>(path);
}

void RaftNode::setReplicationIntervalMs(int ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    replication_interval_ms_ = ms;
}

void RaftNode::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    // Load WAL entries on start if present (useful for followers)
    if (wal_) {
        auto entries = wal_->replay();
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_.empty()) {
            for (const auto &e : entries) log_.push_back(e);
        }
    }
    // Start election loop
    election_thread_ = std::thread(&RaftNode::electionLoop, this);

    // Start replication thread
    replication_running_.store(true);
    replication_thread_ = std::thread([this]{
        while (replication_running_.load()) {
            std::unique_lock<std::mutex> lock(replication_mutex_);
            replication_cv_.wait_for(lock, std::chrono::milliseconds(replication_interval_ms_));
            if (!replication_running_.load()) break;
            // replication logic: for the leader, try to push unreplicated entries
            if (isLeader()) {
                std::vector<std::string> to_replicate;
                {
                    std::lock_guard<std::mutex> lg(mutex_);
                    if (!log_.empty()) {
                        size_t start = commit_index_;
                        for (size_t i = start; i < log_.size(); ++i) to_replicate.push_back(log_[i]);
                    }
                }
                if (peer_append_entries_fn_) {
                    int successes = 1; // leader counts as replicated
                    for (const auto &p : peers_) {
                        size_t start_index = 0;
                        {
                            std::lock_guard<std::mutex> lg(mutex_);
                            start_index = next_index_[p];
                        }

                        if (start_index >= log_.size()) {
                            // nothing to send
                            if (match_index_.count(p)) {
                                // already up-to-date
                                successes += (match_index_[p] >= log_.size()) ? 1 : 0;
                            }
                            continue;
                        }

                        std::vector<std::string> entries;
                        {
                            std::lock_guard<std::mutex> lg(mutex_);
                            for (size_t i = start_index; i < log_.size(); ++i) entries.push_back(log_[i]);
                        }

                        bool ok = false;
                        try {
                            ok = peer_append_entries_fn_(p, current_term_, id_, entries);
                        } catch (...) { ok = false; }

                        if (ok) {
                            // update match and next indices
                            std::lock_guard<std::mutex> lg(mutex_);
                            match_index_[p] = start_index + entries.size();
                            next_index_[p] = match_index_[p];
                            successes++;
                        } else {
                            // back off next_index
                            std::lock_guard<std::mutex> lg(mutex_);
                            if (next_index_[p] > 0) next_index_[p]--;
                        }
                    }

                    size_t total = peers_.size() + 1;
                    if (successes > static_cast<int>(total/2)) {
                        // compute new commit index (majority of match indexes + leader)
                        std::vector<size_t> all;
                        {
                            std::lock_guard<std::mutex> lg(mutex_);
                            all.reserve(match_index_.size() + 1);
                            for (auto &kv : match_index_) all.push_back(kv.second);
                            // leader's index is log_.size()
                            all.push_back(log_.size());
                        }
                        std::sort(all.begin(), all.end(), std::greater<size_t>());
                        size_t majority_pos = (total - 1) / 2; // 0-based
                        size_t new_commit = all[majority_pos];
                        std::lock_guard<std::mutex> lg(mutex_);
                        if (new_commit > commit_index_) commit_index_ = new_commit;
                    }
                }
            }
        }
    });
}

void RaftNode::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    if (election_thread_.joinable()) election_thread_.join();
    replication_running_.store(false);
    replication_cv_.notify_all();
    if (replication_thread_.joinable()) replication_thread_.join();
}


bool RaftNode::handleAppendEntries(uint64_t term, const std::string& leader_id, const std::vector<std::string>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (term < current_term_) return false;
    if (term > current_term_) {
        current_term_ = term;
        state_ = State::Follower;
    }
    // Append entries to local log and WAL
    for (const auto& e : entries) {
        if (wal_) wal_->append(e);
        log_.push_back(e);
    }
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
    // Leader appends entry and waits for commit (simplified blocking semantics)
    if (!running_ || !isLeader()) return false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (wal_) wal_->append(data);
        log_.push_back(data);
        // If single-node cluster, commit immediately
        if (peers_.empty()) {
            commit_index_ = log_.size();
            return true;
        }
    }

    // Wake replication thread to attempt to replicate asap
    replication_cv_.notify_all();

    // Wait for commit
    const int max_wait_ms = 2000;
    int waited = 0;
    while (waited < max_wait_ms) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (commit_index_ >= log_.size()) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waited += 10;
    }
    return false;
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
