#include <gtest/gtest.h>
#include "replication/raft.h"
#include "replication/snapshot.h"
#include "replication/wal.h"
#include <filesystem>

using namespace replication;

TEST(RaftSnapshotRestart, RecoverFromSnapshotAndWalTail) {
    std::filesystem::remove(std::string("/tmp/R.wal"));
    std::filesystem::remove(std::string("/tmp/R.snap"));

    // leader writes entries and creates snapshot (drops all), then appends new entries
    RaftNode leader("R", {});
    leader.setWalPath("/tmp/R.wal");
    leader.setSnapshotPath("/tmp/R");
    leader.setPeerRequestVoteFn([&](const std::string&, uint64_t, const std::string&){ return true; });
    leader.setElectionTimeoutMs(10);
    leader.start();
    for (int i=1;i<=3;i++) ASSERT_TRUE(leader.appendEntry("v" + std::to_string(i)));
    ASSERT_TRUE(leader.createSnapshot("snapdata"));
    leader.stop();

    // Verify snapshot file exists and is loadable
    Snapshot s2;
    ASSERT_TRUE(Snapshot::load(std::string("/tmp/R.snap"), s2));
    std::cerr << "[test] loaded snap last_index=" << s2.last_included_index << " term=" << s2.last_included_term << std::endl;

    // Stop leader first to ensure WAL file is stable, then append tail entries
    leader.stop();
    replication::WAL w2(std::string("/tmp/R.wal"));
    ASSERT_TRUE(w2.append(1, "v4"));
    ASSERT_TRUE(w2.append(1, "v5"));
    auto ents2 = w2.replay();
    std::cerr << "[test] wal replay before restart entries=" << ents2.size() << std::endl;
    for (size_t i=0;i<ents2.size();++i) std::cerr << "[test] wal["<<i<<"] data="<<ents2[i].second<<" term="<<ents2[i].first<<std::endl;

    // new node should load snapshot and WAL tail (v4,v5)
    RaftNode restarted("R", {});
    restarted.setWalPath("/tmp/R.wal");
    restarted.setSnapshotPath("/tmp/R");
    restarted.start();

    EXPECT_EQ(restarted.getCommitIndexForTest(), 3u);
    EXPECT_EQ(restarted.logSize(), 2u);
    EXPECT_EQ(restarted.getLogEntry(0), "v4");
    EXPECT_EQ(restarted.getLogEntry(1), "v5");
    restarted.stop();
}

TEST(RaftSnapshotRestart, FollowerPersistsInstalledSnapshot) {
    std::filesystem::remove(std::string("/tmp/F.wal"));
    std::filesystem::remove(std::string("/tmp/F.snap"));

    RaftNode follower("F", {});
    follower.setWalPath("/tmp/F.wal");
    follower.setSnapshotPath("/tmp/F");
    follower.start();

    Snapshot s; s.last_included_index = 10; s.last_included_term = 2; s.data = "state";
    ASSERT_TRUE(follower.handleInstallSnapshot(2, s).success);
    follower.stop();

    // restart and ensure snapshot metadata loaded
    RaftNode restarted("F", {});
    restarted.setWalPath("/tmp/F.wal");
    restarted.setSnapshotPath("/tmp/F");
    restarted.start();

    EXPECT_GE(restarted.getCommitIndexForTest(), 10u);
    restarted.stop();
}
