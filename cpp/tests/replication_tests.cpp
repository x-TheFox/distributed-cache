#include <gtest/gtest.h>
#include "replication/replicator.h"

TEST(ReplicationStub, CanConstruct) {
    Replicator r;
    EXPECT_TRUE(r.peers().empty());
    EXPECT_FALSE(r.replicate("k","v"));
}
