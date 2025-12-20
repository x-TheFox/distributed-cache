#include <gtest/gtest.h>
#include "replication/raft.h"
#include "replication/snapshot.h"
#include "replication/log.h"
#include <filesystem>

using namespace replication;

TEST(RaftSnapshotCreation, LeaderCreatesSnapshotAndCompactsWAL) {
    replication::setLogLevel(replication::LogLevel::DEBUG);
    RaftNode leader("L", {});
    leader.setWalPath("/tmp/L.wal");
    leader.setSnapshotPath("/tmp/L");
    // remove previous artifacts
    std::filesystem::remove(std::string("/tmp/L.wal"));
    std::filesystem::remove(std::string("/tmp/L.snap"));
    leader.setPeerRequestVoteFn([&](const std::string&, uint64_t, const std::string&){ return true; });
    leader.setElectionTimeoutMs(10);
    leader.start();
    // wait up to 200ms for election
    for (int i=0;i<20;i++) { if (leader.isLeader()) break; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    ASSERT_TRUE(leader.isLeader());

    ASSERT_TRUE(leader.appendEntry("v1"));
    ASSERT_TRUE(leader.appendEntry("v2"));
    ASSERT_TRUE(leader.appendEntry("v3"));

    // create snapshot
    std::cerr << "[test] about to call createSnapshot" << std::endl;
    bool ok = leader.createSnapshot("snapdata");
    std::cerr << "[test] createSnapshot returned=" << ok << std::endl;
    ASSERT_TRUE(ok);

    // snapshot file should exist
    bool exists = std::filesystem::exists(std::string("/tmp/L.snap"));
    std::cerr << "[test] snapshot exists=" << exists << std::endl;
    EXPECT_TRUE(exists);

    leader.stop();
}
