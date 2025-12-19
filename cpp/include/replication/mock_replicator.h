#ifndef MOCK_REPLICATOR_H
#define MOCK_REPLICATOR_H

#include "replication/replicator.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <chrono>

// Simulation options for MockReplicator
struct MockReplicatorOptions {
    // artificial per-replica latency in milliseconds
    std::chrono::milliseconds peer_latency{0};
    // wait for replication acks
    bool wait_for_acks{false};
};

class MockReplicator : public Replicator, public std::enable_shared_from_this<MockReplicator> {
public:
    explicit MockReplicator(MockReplicatorOptions opts = {});
    explicit MockReplicator(const std::vector<std::shared_ptr<MockReplicator>>& peers, MockReplicatorOptions opts = {});
    ~MockReplicator();

    // replicate returns true if local apply succeeded; if wait_for_acks is true, waits for all peer acks
    bool replicate(const std::string& key, const std::string& value) override;
    std::vector<std::string> peers() const override;

    // For tests: read local store
    bool get_local(const std::string& key, std::string& value) const;
    // For tests: list local keys
    std::vector<std::string> list_keys() const;
    // For testing: remove local key
    bool remove_local(const std::string& key);
    // Add peer after construction
    void add_peer(const std::shared_ptr<MockReplicator>& peer);

    // For testing: stop internal worker thread
    void stop();

private:
    void worker_loop();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> store_;
    std::vector<std::weak_ptr<MockReplicator>> peers_;

    // async queue
    std::mutex q_mutex_;
    std::condition_variable q_cv_;
    std::queue<std::pair<std::string, std::string>> queue_;
    bool running_{true};
    std::thread worker_thread_;

    MockReplicatorOptions opts_;
};

#endif // MOCK_REPLICATOR_H
