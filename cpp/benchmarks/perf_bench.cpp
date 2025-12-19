#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cstdlib>
#include "cache/cache.h"

int main(int argc, char* argv[]) {
    int numOperations = 100000;
    int threads = 4;
    EvictionPolicyType policy = EvictionPolicyType::LRU;

    // simple CLI parsing
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--ops" || a == "-n") && i + 1 < argc) numOperations = std::stoi(argv[++i]);
        if ((a == "--threads" || a == "-t") && i + 1 < argc) threads = std::stoi(argv[++i]);
        if ((a == "--policy" || a == "-p") && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "lfu") policy = EvictionPolicyType::LFU;
        }
    }
    const char* envp = std::getenv("EVICTION_POLICY");
    if (envp && std::string(envp) == "lfu") policy = EvictionPolicyType::LFU;

    Cache cache(1024, policy);

    std::atomic<int> counter{0};
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> ths;
    for (int t = 0; t < threads; ++t) {
        ths.emplace_back([&]{
            while (true) {
                int i = counter.fetch_add(1);
                if (i >= numOperations) break;
                std::string key = "key" + std::to_string(i);
                cache.put(key, "value" + std::to_string(i));
                cache.get(key);
            }
        });
    }
    for (auto &t : ths) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Completed " << numOperations << " ops (" << threads << " threads) in " << duration.count() << "s" << std::endl;
    std::cout << "Cache size: " << cache.size() << ", hits=" << cache.hits() << ", misses=" << cache.misses() << std::endl;

    return 0;
}