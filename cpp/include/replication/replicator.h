#ifndef REPLICATOR_H
#define REPLICATOR_H

#include <string>
#include <vector>
#include <memory>

// Lightweight interface for a replication component. This is a stub to be
// expanded later with network transport, snapshot/WAL, and election.
class Replicator {
public:
    Replicator() = default;
    virtual ~Replicator() = default;

    // Submit a write to be replicated; returns true if local apply succeeds
    virtual bool replicate(const std::string& key, const std::string& value) {
        return false;
    }

    // Basic status
    virtual std::vector<std::string> peers() const { return {}; }
};

using ReplicatorPtr = std::shared_ptr<Replicator>;

#endif // REPLICATOR_H
