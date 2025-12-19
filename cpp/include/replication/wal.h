#ifndef WAL_H
#define WAL_H

#include <string>
#include <vector>
#include <mutex>

namespace replication {

class WAL {
public:
    explicit WAL(const std::string& path);
    ~WAL();

    // Append an entry to the WAL (durable) with term+data
    bool append(uint64_t term, const std::string& data);

    // Replay the WAL into a list of (term,data) pairs
    std::vector<std::pair<uint64_t, std::string>> replay() const;

    // Truncate the WAL head by dropping the first `count` entries and compacting the file
    bool truncateHead(size_t count);

private:
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace replication

#endif // WAL_H
