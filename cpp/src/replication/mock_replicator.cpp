#include "replication/mock_replicator.h"

MockReplicator::MockReplicator(const std::vector<std::shared_ptr<MockReplicator>>& peers) {
    for (const auto &p : peers) {
        peers_.push_back(p);
    }
}

bool MockReplicator::replicate(const std::string& key, const std::string& value) {
    // Apply locally first
    {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[key] = value;
    }

    // Synchronously push to peers (simulated)
    for (auto &w : peers_) {
        if (auto p = w.lock()) {
            std::lock_guard<std::mutex> lock(p->mutex_);
            p->store_[key] = value;
        }
    }
    return true;
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

void MockReplicator::add_peer(const std::shared_ptr<MockReplicator>& peer) {
    peers_.push_back(peer);
}
