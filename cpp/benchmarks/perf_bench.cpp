#include <iostream>
#include <chrono>
#include <thread>
#include "cache/cache.h"
#include "cache/lru.h"

void benchmarkCacheOperations(Cache& cache, int numOperations) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOperations; ++i) {
        cache.put(i, "value" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Put " << numOperations << " operations took " << duration.count() << " seconds.\n";

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOperations; ++i) {
        cache.get(i);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    std::cout << "Get " << numOperations << " operations took " << duration.count() << " seconds.\n";
}

int main() {
    const int numOperations = 100000;
    Cache cache;

    benchmarkCacheOperations(cache, numOperations);

    return 0;
}