#include "replication/wal.h"
#include <fstream>
#include <cstdint>

namespace replication {

WAL::WAL(const std::string& path) : path_(path) {}

WAL::~WAL() {}

bool WAL::append(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream ofs(path_, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) return false;
    uint32_t len = static_cast<uint32_t>(data.size());
    ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
    ofs.write(data.data(), len);
    ofs.flush();
    return ofs.good();
}

std::vector<std::string> WAL::replay() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    std::ifstream ifs(path_, std::ios::binary);
    if (!ifs.is_open()) return out;
    while (true) {
        uint32_t len = 0;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!ifs) break;
        std::string buf;
        buf.resize(len);
        ifs.read(&buf[0], len);
        if (!ifs) break;
        out.push_back(std::move(buf));
    }
    return out;
}

} // namespace replication
