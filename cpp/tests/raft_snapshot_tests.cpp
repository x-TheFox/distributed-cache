#include <gtest/gtest.h>
#include "replication/snapshot.h"
#include <filesystem>
#include <fstream>

using namespace replication;

TEST(RaftSnapshot, SaveAndLoad) {
    Snapshot s;
    s.last_included_index = 42;
    s.last_included_term = 7;
    s.data = "hello-snapshot";

    std::string tmp = "/tmp/raft_snapshot_test.bin";
    std::filesystem::remove(tmp);
    ASSERT_TRUE(s.save(tmp));

    Snapshot s2;
    ASSERT_TRUE(Snapshot::load(tmp, s2));
    EXPECT_EQ(s.last_included_index, s2.last_included_index);
    EXPECT_EQ(s.last_included_term, s2.last_included_term);
    EXPECT_EQ(s.data, s2.data);

    std::filesystem::remove(tmp);
}

TEST(RaftSnapshot, LoadCorruptedFile) {
    std::string tmp = "/tmp/raft_snapshot_corrupt.bin";
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    ofs << "garbage";
    ofs.close();

    Snapshot s;
    EXPECT_FALSE(Snapshot::load(tmp, s));
    std::filesystem::remove(tmp);
}
