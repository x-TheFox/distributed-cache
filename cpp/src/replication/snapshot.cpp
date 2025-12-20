#include "replication/snapshot.h"
#include "replication/log.h"
#include <fstream>
#include <cstdint>
#include <array>

namespace replication {

static constexpr uint32_t kSnapshotMagic = 0x50414E53; // 'SNAP' little-endian
static constexpr uint32_t kSnapshotVersion = 1;

bool Snapshot::save(const std::string& path) const {
    LOG(LogLevel::DEBUG, "[snapshot] save path=" << path << " index=" << last_included_index << " term=" << last_included_term << " len=" << data.size());
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) { LOG(LogLevel::ERROR, "[snapshot] save open failed"); return false; }
    uint32_t magic = kSnapshotMagic;
    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    uint32_t version = kSnapshotVersion;
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    ofs.write(reinterpret_cast<const char*>(&last_included_index), sizeof(last_included_index));
    ofs.write(reinterpret_cast<const char*>(&last_included_term), sizeof(last_included_term));
    uint32_t len = static_cast<uint32_t>(data.size());
    ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len) ofs.write(data.data(), len);
    ofs.flush();
    return ofs.good();
}

bool Snapshot::load(const std::string& path, Snapshot& out) {
    std::cerr << "[snapshot-debug] load path=" << path << " exists=" << std::filesystem::exists(path) << std::endl;
    LOG(LogLevel::DEBUG, "[snapshot] load path=" << path);
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) { LOG(LogLevel::WARN, "[snapshot] load open failed"); return false; }
    uint32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic)); if (!ifs) return false;
    if (magic != kSnapshotMagic) { LOG(LogLevel::ERROR, "[snapshot] bad magic"); return false; }
    uint32_t version = 0;
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version)); if (!ifs) return false;
    if (version != kSnapshotVersion) { LOG(LogLevel::ERROR, "[snapshot] unsupported version=" << version); return false; }
    Snapshot s;
    ifs.read(reinterpret_cast<char*>(&s.last_included_index), sizeof(s.last_included_index)); if (!ifs) return false;
    ifs.read(reinterpret_cast<char*>(&s.last_included_term), sizeof(s.last_included_term)); if (!ifs) return false;
    uint32_t len = 0;
    ifs.read(reinterpret_cast<char*>(&len), sizeof(len)); if (!ifs) return false;
    s.data.resize(len);
    if (len) {
        ifs.read(&s.data[0], len);
        if (!ifs) return false;
    }
    out = std::move(s);
    LOG(LogLevel::DEBUG, "[snapshot] load done index=" << out.last_included_index << " term=" << out.last_included_term << " len=" << out.data.size());
    return true;
}

} // namespace replication
