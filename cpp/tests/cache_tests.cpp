#include <gtest/gtest.h>
#include "cache/cache.h"
#include <thread>

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
    auto val = cache->get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "value1");
}

TEST_F(CacheTest, EvictionPolicy) {
    for (int i = 0; i < 15; ++i) {
        cache->put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    auto v0 = cache->get("key0");
    EXPECT_FALSE(v0.has_value()); // key0 should be evicted
    auto v14 = cache->get("key14");
    ASSERT_TRUE(v14.has_value());
    EXPECT_EQ(v14.value(), "value14"); // key14 should still be present
}

TEST_F(CacheTest, UpdateValue) {
    cache->put("key1", "value1");
    cache->put("key1", "value2");
    auto v = cache->get("key1");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), "value2");
}

TEST_F(CacheTest, ConcurrentAccess) {
    // Spawn multiple threads doing puts and gets
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() { cache->put("key" + std::to_string(i), "value" + std::to_string(i)); });
    }
    for (auto &t : threads) t.join();

    for (int i = 0; i < 5; ++i) {
        auto v = cache->get("key" + std::to_string(i));
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), "value" + std::to_string(i));
    }
}

TEST_F(CacheTest, TTLExpiration) {
    cache->put("temp", "x", 10); // 10 ms TTL
    auto v1 = cache->get("temp");
    ASSERT_TRUE(v1.has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto v2 = cache->get("temp");
    EXPECT_FALSE(v2.has_value());
}