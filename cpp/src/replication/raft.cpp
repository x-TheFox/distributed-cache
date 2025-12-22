#include "replication/raft.h"
#include "replication/log.h"
#include <thread>
#include <random>
#include <iostream>
#include <future>
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

        // Avoid starting an election immediately after receiving AppendEntries (heartbeat)
        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lg(mutex_);
            auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_append_time_).count();
            if (since < timeout) continue;
        }

            // If there are no peers, become leader immediately
        if (peers_.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Leader;
            current_term_++;
            // Load WAL if present
            if (wal_) {
                auto entries = wal_->replay();
                for (const auto &e : entries) log_.push_back(Entry{e.first, e.second});
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
            LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " starting election term=" << current_term_);
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
            LOG(LogLevel::INFO, "[raft] node=" << id_ << " elected leader term=" << current_term_ << " votes=" << votes);
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
    LOG(LogLevel::DEBUG, "[raft] start node=" << id_);
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) { LOG(LogLevel::WARN, "[raft] start called while already running"); return; }
    // Load snapshot (if exists) and WAL entries on start; replay WAL tail after snapshot
    Snapshot snap;
    bool has_snap = false;
    if (!snapshot_path_.empty()) {
        if (Snapshot::load(snapshot_path_ + ".snap", snap)) {
            std::lock_guard<std::mutex> lock(mutex_);
            last_included_index_ = snap.last_included_index;
            last_included_term_ = snap.last_included_term;
            commit_index_ = std::max(commit_index_, last_included_index_);
            LOG(LogLevel::INFO, "[raft] node=" << id_ << " loaded snapshot index=" << last_included_index_ << " term=" << last_included_term_);
            has_snap = true;
        }
    }

    if (wal_) {
        auto entries = wal_->replay();
        std::cerr << "[wal-debug] replay returned entries=" << entries.size() << std::endl;
        LOG(LogLevel::DEBUG, "[wal] replay returned entries=" << entries.size());
        std::lock_guard<std::mutex> lock(mutex_);
        // The WAL file is already truncated by compaction; replay() returns the WAL tail (entries after snapshot).
        // Append all returned entries to the in-memory log.
        for (size_t i = 0; i < entries.size(); ++i) {
            log_.push_back(Entry{entries[i].first, entries[i].second});
        }
        LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " after WAL replay logSize=" << log_.size());
        for (size_t i=0;i<log_.size();++i) LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " log["<<i<<"]=" << log_[i].data << " term="<<log_[i].term);
    }

    // initialize last append time
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_append_time_ = std::chrono::steady_clock::now();
    }
    // If no peers, become leader immediately
    if (peers_.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = State::Leader;
        current_term_++;
        LOG(LogLevel::INFO, "[raft] node=" << id_ << " becoming leader (no peers) term=" << current_term_);
        // initialize replication bookkeeping (empty peers)
    }

    // Start election loop
    election_thread_ = std::thread(&RaftNode::electionLoop, this);

    // Start replication thread
    replication_running_.store(true);
    replication_thread_ = std::thread([this]{
        LOG(LogLevel::DEBUG, "[raft] replication thread started for node=" << id_);
        while (replication_running_.load()) {
            std::unique_lock<std::mutex> lock(replication_mutex_);
            replication_cv_.wait_for(lock, std::chrono::milliseconds(replication_interval_ms_));
            if (!replication_running_.load()) break;
            // replication logic: for the leader, try to push unreplicated entries
            if (isLeader()) {
                if (peer_append_entries_fn_) {
                    int successes = 1; // leader counts as replicated
                    for (const auto &p : peers_) {
                        size_t start_index = 0;
                        {
                            std::lock_guard<std::mutex> lg(mutex_);
                            start_index = next_index_[p];
                        }

                        // If follower is so far behind (next_index <= last_included_index_), send InstallSnapshot
                        if (peer_install_snapshot_fn_) {
                            bool need_snapshot = false;
                            Snapshot snap;
                            {
                                std::lock_guard<std::mutex> lg(mutex_);
                                if (start_index <= last_included_index_) {
                                    need_snapshot = true;
                                    snap.last_included_index = last_included_index_;
                                    snap.last_included_term = last_included_term_;
                                    if (!snapshot_path_.empty()) Snapshot::load(snapshot_path_ + ".snap", snap);
                                }
                            }
                            if (need_snapshot) {
                                RaftNode::InstallSnapshotResult sres{0, false};
                                try {
                                    sres = peer_install_snapshot_fn_(p, current_term_, snap);
                                } catch (...) { sres.success = false; sres.term = 0; }
                                if (sres.term > current_term_) {
                                    std::lock_guard<std::mutex> lg(mutex_);
                                    std::cerr << "[raft] leader=" << id_ << " observed higher term " << sres.term << " from " << p << ", stepping down\n";
                                    current_term_ = sres.term;
                                    state_ = State::Follower;
                                    break;
                                }
                                if (sres.success) {
                                    std::lock_guard<std::mutex> lg(mutex_);
                                    // follower accepted snapshot; advance indices
                                    next_index_[p] = last_included_index_ + 1;
                                    match_index_[p] = last_included_index_;
                                }
                                continue; // done with this peer for this round
                            }
                        }

                        std::vector<Entry> entries;
                        size_t prev_index = (start_index == 0) ? static_cast<size_t>(-1) : start_index - 1;
                        uint64_t prev_term = 0;
                        {
                            std::lock_guard<std::mutex> lg(mutex_);
                            if (start_index < log_.size()) {
                                for (size_t i = start_index; i < log_.size(); ++i) entries.push_back(log_[i]);
                            }
                            if (prev_index != static_cast<size_t>(-1) && prev_index < log_.size()) prev_term = log_[prev_index].term;
                        }

                        RaftNode::AppendEntriesResult res{false, 0, 0, 0, 0};
                        try {
                            res = peer_append_entries_fn_(p, current_term_, id_, prev_index, prev_term, entries);
                        } catch (...) { res.success = false; res.term = 0; res.match_index = 0; }

                        LOG(LogLevel::DEBUG, "[raft] leader=" << id_ << " send to " << p << " ok=" << res.success << " entries=" << entries.size() << " term=" << res.term << " match_index=" << res.match_index);

                        if (res.term > current_term_) {
                            // follower has higher term -> step down
                            std::lock_guard<std::mutex> lg(mutex_);
                            std::cerr << "[raft] leader=" << id_ << " observed higher term " << res.term << " from " << p << ", stepping down\n";
                            current_term_ = res.term;
                            state_ = State::Follower;
                            // stop trying to replicate for now
                            break;
                        }

                        if (res.success) {
                            // update match and next indices; next_index should be the index of the next entry to send
                            std::lock_guard<std::mutex> lg(mutex_);
                            if (!entries.empty()) {
                                match_index_[p] = res.match_index;
                                // next index is match_index + 1, but do not exceed leader's log size
                                next_index_[p] = std::min(match_index_[p] + 1, log_.size());
                            } else {
                                // heartbeat: update known match index and set next_index accordingly
                                match_index_[p] = std::max(match_index_[p], res.match_index);
                                next_index_[p] = std::min(match_index_[p] + 1, log_.size());
                            }
                            successes++;
                        } else {
                            // improved backtrack using follower's conflict hint
                            std::lock_guard<std::mutex> lg(mutex_);
                            if (res.conflict_term != 0) {
                                // follower indicates a conflicting term; find last index in leader's log with that term
                                ssize_t last_idx = -1;
                                for (ssize_t i = static_cast<ssize_t>(log_.size()) - 1; i >= 0; --i) {
                                    if (log_[i].term == res.conflict_term) { last_idx = i; break; }
                                }
                                if (last_idx >= 0) {
                                    // leader has entries with that term; set next index to last_idx + 1
                                    next_index_[p] = static_cast<size_t>(last_idx) + 1;
                                } else {
                                    // leader doesn't have that term; jump to follower's first index of the conflicting term
                                    next_index_[p] = res.conflict_index;
                                }
                            } else {
                                // no conflict term hint; follower indicated missing entries larger than its log
                                next_index_[p] = res.conflict_index;
                            }
                            // ensure we don't advance next_index beyond current log size
                            if (next_index_[p] > log_.size()) next_index_[p] = log_.size();
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
                        // reset failure counter
                        consecutive_failed_replication_rounds_ = 0;
                    } else {
                        // failed to reach majority this round
                        consecutive_failed_replication_rounds_++;
                        if (consecutive_failed_replication_rounds_ >= max_consecutive_failed_rounds_before_stepdown_) {
                            std::lock_guard<std::mutex> lg(mutex_);
                            std::cerr << "[raft] leader=" << id_ << " unable to reach majority for " << consecutive_failed_replication_rounds_ << " rounds; stepping down\n";
                            state_ = State::Follower;
                            // reset counter
                            consecutive_failed_replication_rounds_ = 0;
                        }
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


RaftNode::AppendEntriesResult RaftNode::handleAppendEntries(uint64_t term, const std::string& leader_id, size_t prev_log_index, uint64_t prev_log_term, const std::vector<Entry>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    // debug print to trace AppendEntries handling
    LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " recv AppendEntries term=" << term << " my_term=" << current_term_ << " leader=" << leader_id << " prev_log_index=" << prev_log_index << " prev_log_term=" << prev_log_term << " entries=" << entries.size());
    AppendEntriesResult res{false, current_term_, log_.size(), 0, 0};
    if (term < current_term_) {
        std::cerr << "[raft] node=" << id_ << " rejecting AppendEntries due to stale term\n";
        res.success = false;
        res.term = current_term_;
        res.match_index = log_.size();
        res.conflict_term = 0;
        res.conflict_index = log_.size();
        return res;
    }
    if (term > current_term_) {
        std::cerr << "[raft] node=" << id_ << " updating term from " << current_term_ << " to " << term << " and becoming follower\n";
        current_term_ = term;
        state_ = State::Follower;
    }

    // Validate prev_log_index/term
    if (prev_log_index != static_cast<size_t>(-1)) {
        if (prev_log_index >= log_.size()) {
            // Missing prior entry
            res.success = false;
            res.term = current_term_;
            // indicate missing entries: conflict_term == 0, conflict_index = current log size
            res.conflict_term = 0;
            res.conflict_index = log_.size();
            std::cerr << "[raft] node=" << id_ << " rejecting AppendEntries: prev_log_index too large (" << prev_log_index << " >= " << log_.size() << ")\n";
            return res;
        }
        if (log_[prev_log_index].term != prev_log_term) {
            // Term mismatch -> conflict; provide conflict term and first index of that term
            uint64_t bad_term = log_[prev_log_index].term;
            size_t first_index = prev_log_index;
            // Scan backwards to find first index of the conflicting term
            while (first_index > 0 && log_[first_index - 1].term == bad_term) --first_index;
            res.success = false;
            res.term = current_term_;
            res.conflict_term = bad_term;
            res.conflict_index = first_index;
            std::cerr << "[raft] node=" << id_ << " rejecting AppendEntries: term mismatch at prev_log_index=" << prev_log_index << " conflict_term=" << bad_term << " conflict_index=" << first_index << "\n";
            return res;
        }
    }

    // Accept: truncate conflicting entries and append new ones
    if (!entries.empty()) {
        size_t start_pos = (prev_log_index == static_cast<size_t>(-1)) ? 0 : prev_log_index + 1;
        if (start_pos < log_.size()) {
            log_.resize(start_pos);
        }
        for (const auto &e : entries) {
            if (wal_) wal_->append(e.term, e.data);
            log_.push_back(e);
        }
    }

    // update last append time (heartbeat)
    last_append_time_ = std::chrono::steady_clock::now();
    res.success = true;
    res.term = current_term_;
    res.match_index = log_.size();
    res.conflict_term = 0;
    res.conflict_index = 0;
    return res;
}

size_t RaftNode::logSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return log_.size();
}

bool RaftNode::compactLogPrefix(size_t count) {
    if (!wal_) return false;
    if (count == 0) return true;
    // capture term and validate under lock
    uint64_t last_term = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count > log_.size()) return false;
        last_term = log_[count - 1].term;
    }

    LOG(LogLevel::DEBUG, "[raft] compactLogPrefix start node=" << id_ << " drop=" << count);

    // Call WAL truncate without holding Raft mutex to avoid potential deadlocks
    bool ok = wal_->truncateHead(count);
    if (!ok) { LOG(LogLevel::ERROR, "[raft] compactLogPrefix wal truncate failed"); return false; }

    // Now update in-memory log and metadata under lock
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // safety check
        if (count > log_.size()) {
            // nothing to do
            last_included_index_ += count;
            last_included_term_ = last_term;
            return true;
        }
        log_.erase(log_.begin(), log_.begin() + static_cast<ssize_t>(count));
        last_included_index_ += count;
        last_included_term_ = last_term;
    }

    LOG(LogLevel::DEBUG, "[raft] compactLogPrefix done node=" << id_ << " new_last_included_index=" << last_included_index_);
    return true;
}

std::string RaftNode::getLogEntry(size_t idx) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx >= log_.size()) return "";
    return log_[idx].data;
}

bool RaftNode::appendEntry(const std::string& data) {
    // Leader appends entry and waits for commit (simplified blocking semantics)
    LOG(LogLevel::DEBUG, "[raft] appendEntry called node=" << id_ << " isLeader=" << isLeader());
    if (!running_ || !isLeader()) return false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (wal_) wal_->append(current_term_, data);
        log_.push_back(Entry{current_term_, data});
        LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " appended entry term=" << current_term_ << " logSize=" << log_.size());
        // If single-node cluster, commit immediately
        if (peers_.empty()) {
            commit_index_ = log_.size();
            return true;
        }
    }

    // Wake replication thread to attempt to replicate asap
    replication_cv_.notify_all();

    // Wait for commit
    const int max_wait_ms = 15000;
    int waited = 0;
    while (waited < max_wait_ms) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (commit_index_ >= log_.size()) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waited += 10;
    }
    LOG(LogLevel::WARN, "[raft] appendEntry timeout node=" << id_);
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

void RaftNode::setSnapshotPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_path_ = path;
}

void RaftNode::setSnapshotThreshold(size_t entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_threshold_entries_ = entries;
}

bool RaftNode::createSnapshot(const std::string& data) {
    LOG(LogLevel::DEBUG, "[raft] createSnapshot node=" << id_ << " data_len=" << data.size());
    // Gather snapshot metadata under lock, then release before IO and compaction to avoid deadlocks
    Snapshot s;
    size_t to_drop = 0;
    std::cerr << "[snapshot-debug] before lock" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << "[snapshot-debug] after lock" << std::endl;
        if (state_ != State::Leader) { LOG(LogLevel::WARN, "[raft] createSnapshot node=" << id_ << " not leader"); return false; }
        if (snapshot_path_.empty()) { LOG(LogLevel::WARN, "[raft] createSnapshot no snapshot_path set"); return false; }
        s.last_included_index = last_included_index_ + log_.size();
        s.last_included_term = log_.empty() ? last_included_term_ : log_.back().term;
        to_drop = log_.size();
    }

    s.data = data;
    std::string path = snapshot_path_ + ".snap";
    std::cerr << "[snapshot-debug] before async save" << std::endl;
    LOG(LogLevel::DEBUG, "[snapshot] save path=" << path << " index=" << s.last_included_index << " term=" << s.last_included_term << " len=" << s.data.size());
    // Run save in async with a short timeout to avoid blocking the test indefinitely
    auto fut = std::async(std::launch::async, [&s, &path]{ std::cerr << "[snapshot-debug] in async save start" << std::endl; bool r = s.save(path); std::cerr << "[snapshot-debug] in async save done r=" << r << std::endl; return r; });
    std::cerr << "[snapshot-debug] after launching async" << std::endl;
    if (fut.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready) {
        std::cerr << "[snapshot-debug] async save timed out" << std::endl;
        LOG(LogLevel::ERROR, "[raft] createSnapshot save timed out");
        return false;
    }
    std::cerr << "[snapshot-debug] async ready" << std::endl;
    if (!fut.get()) { std::cerr << "[snapshot-debug] async returned false" << std::endl; LOG(LogLevel::ERROR, "[raft] createSnapshot save failed"); return false; }
    std::cerr << "[snapshot-debug] save succeeded" << std::endl;

    // compact entire log (may call WAL and acquire raft mutex internally as needed)
    if (to_drop > 0) {
        LOG(LogLevel::DEBUG, "[raft] createSnapshot compacting drop=" << to_drop);
        if (!compactLogPrefix(to_drop)) { LOG(LogLevel::ERROR, "[raft] createSnapshot compactLogPrefix failed"); return false; }
        LOG(LogLevel::DEBUG, "[raft] createSnapshot compacted");
    }
    return true;
}

void RaftNode::setPeerInstallSnapshotFn(PeerInstallSnapshotFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_install_snapshot_fn_ = std::move(fn);
}

RaftNode::InstallSnapshotResult RaftNode::handleInstallSnapshot(uint64_t term, const Snapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    InstallSnapshotResult res{current_term_, false};
    LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " recv InstallSnapshot term=" << term << " snap_index=" << snapshot.last_included_index << " snap_term=" << snapshot.last_included_term);
    if (term < current_term_) {
        res.term = current_term_;
        res.success = false;
        return res;
    }
    if (term > current_term_) {
        current_term_ = term;
        state_ = State::Follower;
    }

    // persist snapshot to disk if path configured
    if (!snapshot_path_.empty()) {
        Snapshot s = snapshot;
        std::string path = snapshot_path_ + ".snap";
        if (!s.save(path)) {
            LOG(LogLevel::ERROR, "[raft] node=" << id_ << " failed to save snapshot to " << path);
            res.success = false;
            res.term = current_term_;
            return res;
        }
    }

    // Figure out how many local log entries to drop
    size_t prev_last_included = last_included_index_;
    uint64_t prev_term = last_included_term_;
    if (snapshot.last_included_index > last_included_index_) {
        size_t drop = snapshot.last_included_index - last_included_index_;
        if (drop <= log_.size()) {
            // We are currently holding the Raft mutex; avoid calling compactLogPrefix() which would re-lock.
            // Capture the last term for the dropped range while locked, then release lock to truncate WAL, then re-acquire to update in-memory log.
            uint64_t drop_last_term = log_[drop - 1].term;
            // Truncate WAL outside lock
            if (wal_) {
                bool ok = wal_->truncateHead(drop);
                if (!ok) {
                    LOG(LogLevel::ERROR, "[raft] node=" << id_ << " wal truncateHead failed during install snapshot");
                    res.success = false;
                    res.term = current_term_;
                    return res;
                }
            }
            // Now update in-memory log and metadata under lock
            {
                // we already have the lock here; perform erase
                if (drop <= log_.size()) {
                    log_.erase(log_.begin(), log_.begin() + static_cast<ssize_t>(drop));
                } else {
                    log_.clear();
                }
                last_included_index_ += drop;
                last_included_term_ = drop_last_term;
            }
        } else {
            // follower doesn't have enough local entries: clear all
            size_t local = log_.size();
            // release in-memory entries under lock first
            log_.clear();
            // Truncate entire WAL without holding lock
            if (wal_) {
                if (!wal_->truncateHead(local)) {
                    LOG(LogLevel::ERROR, "[raft] node=" << id_ << " wal truncateHead failed during full install snapshot");
                    res.success = false;
                    res.term = current_term_;
                    return res;
                }
            }
            // update metadata
            last_included_index_ = snapshot.last_included_index;
            last_included_term_ = snapshot.last_included_term;
        }
    }

    // update snapshot metadata
    last_included_index_ = snapshot.last_included_index;
    last_included_term_ = snapshot.last_included_term;

    // reset commit index if necessary
    if (commit_index_ < last_included_index_) commit_index_ = last_included_index_;

    res.term = current_term_;
    res.success = true;
    return res;
}

bool RaftNode::handleRequestVote(uint64_t term, const std::string& candidate_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " handleRequestVote term=" << term << " my_term=" << current_term_ << " candidate=" << candidate_id);
    if (term < current_term_) {
        LOG(LogLevel::DEBUG, "[raft] node=" << id_ << " rejecting RequestVote due to stale term");
        return false;
    }
    if (term > current_term_) {
        LOG(LogLevel::INFO, "[raft] node=" << id_ << " updating term from " << current_term_ << " to " << term << " and clearing votes");
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
