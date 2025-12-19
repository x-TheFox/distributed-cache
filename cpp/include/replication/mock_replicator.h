#ifndef MOCK_REPLICATOR_H
#define MOCK_REPLICATOR_H

#include "replication/replicator.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class MockReplicator : public Replicator, public std::enable_shared_from_this<MockReplicator> {
public:
    MockReplicator() = default;
    explicit MockReplicator(const std::vector<std::shared_ptr<MockReplicator>>& peers);

    bool replicate(const std::string& key, const std::string& value) override;
    std::vector<std::string> peers() const override;

    // For tests: read local store
    bool get_local(const std::string& key, std::string& value) const;

    // Add peer after construction
    void add_peer(const std::shared_ptr<MockReplicator>& peer);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> store_;
    std::vector<std::weak_ptr<MockReplicator>> peers_;
};

#endif // MOCK_REPLICATOR_H
