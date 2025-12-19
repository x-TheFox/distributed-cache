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

TEST(CacheLFU, EvictionBehavior) {
    Cache cache(3, EvictionPolicyType::LFU);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");

    // Access patterns: a:2, b:1, c:1
    auto va = cache.get("a");
    ASSERT_TRUE(va.has_value());
    auto vb = cache.get("b");
    ASSERT_TRUE(vb.has_value());
    auto va2 = cache.get("a");
    ASSERT_TRUE(va2.has_value());

    // Insert d, should evict one of the least frequently used (b or c)
    cache.put("d", "4");
    auto vb2 = cache.get("b");
    auto vc = cache.get("c");
    auto vd = cache.get("d");
    ASSERT_TRUE(vd.has_value());
    // Exactly one of b or c must be evicted
    EXPECT_TRUE(!(vb2.has_value() && vc.has_value()));
}

TEST(CachePolicy, LRUvsLFU) {
    Cache lruCache(2, EvictionPolicyType::LRU);
    lruCache.put("x", "1");
    lruCache.put("y", "2");
    lruCache.get("x"); // x becomes most recent
    lruCache.put("z", "3");
    EXPECT_FALSE(lruCache.get("y").has_value()); // y should be evicted (LRU)

    Cache lfuCache(2, EvictionPolicyType::LFU);
    lfuCache.put("x", "1");
    lfuCache.put("y", "2");
    lfuCache.get("x"); // x freq 1
    lfuCache.put("z", "3");
    // y (freq 1, but least recently used among freq 1) may be evicted; x should survive due to more freq
    EXPECT_TRUE(lfuCache.get("x").has_value());
}