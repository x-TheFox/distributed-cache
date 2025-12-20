#ifndef REPLICATION_SNAPSHOT_H
#define REPLICATION_SNAPSHOT_H

#include <string>
#include <cstdint>

namespace replication {

// Simple on-disk snapshot format:
// [magic: 4 bytes "SNAP"] [version: uint32_t] [last_included_index: uint64_t]
// [last_included_term: uint64_t] [payload_len: uint32_t] [payload bytes]

struct Snapshot {
    uint64_t last_included_index = 0;
    uint64_t last_included_term = 0;
    std::string data; // serialized state machine snapshot

    // Save snapshot to `path`. Returns true on success.
    bool save(const std::string& path) const;

    // Load snapshot from `path`. Returns true on success and fills the object.
    static bool load(const std::string& path, Snapshot& out);
};

} // namespace replication

#endif // REPLICATION_SNAPSHOT_H
