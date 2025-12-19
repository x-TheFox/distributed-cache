#include <gtest/gtest.h>
#include "cache/cache.h"
#include "cache/lru.h"

class CacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = new Cache(10); // Initialize cache with a capacity of 10
    }

    void TearDown() override {
        delete cache;
    }

    Cache* cache;
};

TEST_F(CacheTest, BasicPutAndGet) {
    cache->put("key1", "value1");
    EXPECT_EQ(cache->get("key1"), "value1");
}

TEST_F(CacheTest, EvictionPolicy) {
    for (int i = 0; i < 15; ++i) {
        cache->put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    EXPECT_EQ(cache->get("key0"), nullptr); // key0 should be evicted
    EXPECT_EQ(cache->get("key14"), "value14"); // key14 should still be present
}

TEST_F(CacheTest, UpdateValue) {
    cache->put("key1", "value1");
    cache->put("key1", "value2");
    EXPECT_EQ(cache->get("key1"), "value2");
}

TEST_F(CacheTest, ConcurrentAccess) {
    // This test would require a more complex setup with threads to test concurrency
    // For simplicity, we will just check that the cache can handle multiple puts
    std::thread t1([this]() { cache->put("key1", "value1"); });
    std::thread t2([this]() { cache->put("key2", "value2"); });
    t1.join();
    t2.join();
    EXPECT_EQ(cache->get("key1"), "value1");
    EXPECT_EQ(cache->get("key2"), "value2");
}