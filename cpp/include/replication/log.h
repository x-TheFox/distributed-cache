#ifndef REPLICATION_LOG_H
#define REPLICATION_LOG_H

#include <atomic>
#include <iostream>
#include <string>

namespace replication {

enum class LogLevel { ERROR=0, WARN=1, INFO=2, DEBUG=3 };

inline std::atomic<LogLevel>& getLogLevel() {
    static std::atomic<LogLevel> lvl{LogLevel::INFO};
    return lvl;
}

inline void setLogLevel(LogLevel l) { getLogLevel().store(l); }

inline bool logEnabled(LogLevel l) { return static_cast<int>(l) <= static_cast<int>(getLogLevel().load()); }

#define LOG(level, msg) do { if (replication::logEnabled(level)) { std::cerr << msg << std::endl; } } while(0)

} // namespace replication

#endif // REPLICATION_LOG_H
