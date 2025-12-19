#include "replication/mock_replicator.h"
#include <algorithm>

MockReplicator::MockReplicator(MockReplicatorOptions opts) : opts_(opts), worker_thread_([this]{ worker_loop(); }) {}

MockReplicator::MockReplicator(const std::vector<std::shared_ptr<MockReplicator>>& peers, MockReplicatorOptions opts) : opts_(opts), worker_thread_([this]{ worker_loop(); }) {
    for (const auto &p : peers) {
        peers_.push_back(p);
    }
}

MockReplicator::~MockReplicator() {
    stop();
}

void MockReplicator::stop() {
    {
        std::lock_guard<std::mutex> lock(q_mutex_);
        running_ = false;
    }
    q_cv_.notify_all();
    if (worker_thread_.joinable()) worker_thread_.join();
}

bool MockReplicator::replicate(const std::string& key, const std::string& value) {
    // Apply locally first
    {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[key] = value;
    }

    // Push to async queue for sending to peers
    {
        std::lock_guard<std::mutex> lock(q_mutex_);
        queue_.emplace(key, value);
    }
    q_cv_.notify_one();

    if (opts_.wait_for_acks) {
        // naive ack: poll peers until they report the key (with timeout)
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < deadline) {
            bool all_ok = true;
            for (auto &w : peers_) {
                if (auto p = w.lock()) {
                    std::string v;
                    if (!p->get_local(key, v) || v != value) { all_ok = false; break; }
                }
            }
            if (all_ok) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    return true;
}

void MockReplicator::worker_loop() {
    while (true) {
        std::pair<std::string, std::string> item;
        {
            std::unique_lock<std::mutex> lock(q_mutex_);
            q_cv_.wait(lock, [this]{ return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) return;
            item = queue_.front(); queue_.pop();
        }

        // simulate network send with configured latency
        if (opts_.peer_latency.count() > 0) std::this_thread::sleep_for(opts_.peer_latency);

        for (auto &w : peers_) {
            if (auto p = w.lock()) {
                std::lock_guard<std::mutex> lock(p->mutex_);
                p->store_[item.first] = item.second;
            }
        }
    }
}

std::vector<std::string> MockReplicator::peers() const {
    std::vector<std::string> out;
    for (auto &w : peers_) if (auto p = w.lock()) out.push_back("mock-peer");
    return out;
}

bool MockReplicator::get_local(const std::string& key, std::string& value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    value = it->second;
    return true;
}

std::vector<std::string> MockReplicator::list_keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(store_.size());
    for (const auto &kv : store_) keys.push_back(kv.first);
    return keys;
}

bool MockReplicator::remove_local(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    store_.erase(it);
    return true;
}

void MockReplicator::add_peer(const std::shared_ptr<MockReplicator>& peer) {
    peers_.push_back(peer);
}
