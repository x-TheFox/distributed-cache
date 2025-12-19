import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class CacheTest {
    private Cache<String, String> cache;

    @BeforeEach
    void setUp() {
        cache = new Cache<>(100); // Initialize cache with a maximum size of 100
    }

    @Test
    void testPutAndGet() {
        cache.put("key1", "value1");
        assertEquals("value1", cache.get("key1"));
    }

    @Test
    void testEviction() {
        for (int i = 0; i < 105; i++) {
            cache.put("key" + i, "value" + i);
        }
        assertNull(cache.get("key0")); // key0 should be evicted
        assertEquals("value1", cache.get("key1")); // key1 should still be present
    }

    @Test
    void testConcurrentAccess() throws InterruptedException {
        Thread writer = new Thread(() -> {
            for (int i = 0; i < 50; i++) {
                cache.put("key" + i, "value" + i);
            }
        });

        Thread reader = new Thread(() -> {
            for (int i = 0; i < 50; i++) {
                assertNotNull(cache.get("key" + i));
            }
        });

        writer.start();
        reader.start();
        writer.join();
        reader.join();
    }

    @Test
    void testCacheSizeLimit() {
        for (int i = 0; i < 100; i++) {
            cache.put("key" + i, "value" + i);
        }
        assertEquals(100, cache.size());
        cache.put("key101", "value101");
        assertEquals(100, cache.size()); // Size should remain 100 after adding new item
    }
}