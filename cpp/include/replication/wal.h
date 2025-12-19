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

    // Append an entry to the WAL (durable)
    bool append(const std::string& data);

    // Replay the WAL into a list of entries
    std::vector<std::string> replay() const;

private:
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace replication

#endif // WAL_H
