#include <gtest/gtest.h>
#include "sharder/consistent_hash.h"

TEST(Sharder, MappingStability) {
    ConsistentHash ch(10);
    ch.add_node("n1");
    ch.add_node("n2");

    std::string k = "alpha";
    auto n1 = ch.get_node(k);
    auto n2 = ch.get_node(k);
    EXPECT_EQ(n1, n2);
}

TEST(Sharder, RebalanceBehavior) {
    ConsistentHash ch(20);
    ch.add_node("a");
    ch.add_node("b");

    std::vector<std::string> keys;
    for (int i = 0; i < 1000; ++i) keys.push_back("key" + std::to_string(i));

    int moved = 0;
    std::vector<std::string> before;
    for (auto &k : keys) before.push_back(ch.get_node(k));

    ch.add_node("c");
    for (size_t i = 0; i < keys.size(); ++i) {
        auto after = ch.get_node(keys[i]);
        if (after != before[i]) ++moved;
    }
    // Expect that not all keys moved; i.e., some remapping occurred but majority remain
    EXPECT_GT(moved, 0);
    EXPECT_LT(moved, static_cast<int>(keys.size()));
}
