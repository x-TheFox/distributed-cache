#include "replication/wal.h"
#include "replication/log.h"
#include <fstream>
#include <cstdint>

namespace replication {

WAL::WAL(const std::string& path) : path_(path) {}

WAL::~WAL() {}

bool WAL::append(uint64_t term, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(LogLevel::DEBUG, "[wal] append path=" << path_ << " term=" << term << " len=" << data.size());
    std::ofstream ofs(path_, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) { LOG(LogLevel::ERROR, "[wal] append failed open"); return false; }
    ofs.write(reinterpret_cast<const char*>(&term), sizeof(term));
    uint32_t len = static_cast<uint32_t>(data.size());
    ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
    ofs.write(data.data(), len);
    ofs.flush();
    LOG(LogLevel::DEBUG, "[wal] append done");
    return ofs.good();
}

std::vector<std::pair<uint64_t, std::string>> WAL::replay() const {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(LogLevel::DEBUG, "[wal] replay start path=" << path_);
    std::vector<std::pair<uint64_t, std::string>> out;
    std::ifstream ifs(path_, std::ios::binary);
    if (!ifs.is_open()) {
        LOG(LogLevel::WARN, "[wal] replay open failed");
        return out;
    }
    while (true) {
        uint64_t term = 0;
        ifs.read(reinterpret_cast<char*>(&term), sizeof(term));
        if (!ifs) break;
        uint32_t len = 0;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!ifs) break;
        std::string buf;
        buf.resize(len);
        ifs.read(&buf[0], len);
        if (!ifs) break;
        out.emplace_back(term, std::move(buf));
    }
    LOG(LogLevel::DEBUG, "[wal] replay end entries=" << out.size());
    return out;
}

bool WAL::truncateHead(size_t count) {
    // Do not hold the mutex while calling replay() which itself locks the mutex (avoid deadlock).
    auto all = replay();
    if (count >= all.size()) {
        // truncate to empty file
        std::ofstream ofs(path_, std::ios::trunc | std::ios::binary);
        return ofs.good();
    }
    std::string tmp = path_ + ".tmp";
    {
        std::ofstream ofs(tmp, std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) return false;
        for (size_t i = count; i < all.size(); ++i) {
            uint64_t term = all[i].first;
            uint32_t len = static_cast<uint32_t>(all[i].second.size());
            ofs.write(reinterpret_cast<const char*>(&term), sizeof(term));
            ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
            ofs.write(all[i].second.data(), len);
            if (!ofs) {
                ofs.close();
                std::remove(tmp.c_str());
                return false;
            }
        }
        ofs.flush();
    }
    // atomically replace the original WAL with the temporary
    if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace replication
