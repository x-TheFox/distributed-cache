#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <random>
#include <algorithm>
#include "cache/cache.h"

// Simple Zipf generator using precomputed CDF and binary search
class ZipfGenerator {
public:
    ZipfGenerator(int n = 1000, double s = 1.0) : n_(n), s_(s), dist_(0.0, 1.0) {
        cdf_.resize(n_);
        double H = 0.0;
        for (int i = 1; i <= n_; ++i) H += 1.0 / std::pow(i, s_);
        double cumulative = 0.0;
        for (int i = 1; i <= n_; ++i) {
            cumulative += (1.0 / std::pow(i, s_)) / H;
            cdf_[i - 1] = cumulative;
        }
        rng_.seed(std::random_device{}());
    }

    int next() {
        double u = dist_(rng_);
        auto it = std::lower_bound(cdf_.begin(), cdf_.end(), u);
        int idx = std::min<int>(std::distance(cdf_.begin(), it), n_ - 1);
        return idx; // 0-based id
    }

private:
    int n_;
    double s_;
    std::vector<double> cdf_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;
};

int main(int argc, char* argv[]) {
    int numOperations = 100000;
    int threads = 4;
    EvictionPolicyType policy = EvictionPolicyType::LRU;
    std::string dist_type = "uniform";
    int zipf_n = 1000;
    double zipf_s = 1.0;

    // CLI parsing
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--ops" || a == "-n") && i + 1 < argc) numOperations = std::stoi(argv[++i]);
        if ((a == "--threads" || a == "-t") && i + 1 < argc) threads = std::stoi(argv[++i]);
        if ((a == "--policy" || a == "-p") && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "lfu") policy = EvictionPolicyType::LFU;
        }
        if ((a == "--dist" || a == "-d") && i + 1 < argc) dist_type = argv[++i];
        if ((a == "--zipf-n") && i + 1 < argc) zipf_n = std::stoi(argv[++i]);
        if ((a == "--zipf-s") && i + 1 < argc) zipf_s = std::stod(argv[++i]);
    }
    const char* envp = std::getenv("EVICTION_POLICY");
    if (envp && std::string(envp) == "lfu") policy = EvictionPolicyType::LFU;
    const char* envd = std::getenv("KEY_DISTRIBUTION");
    if (envd) dist_type = envd;

    Cache cache(1024, policy);
    ZipfGenerator zipf(zipf_n, zipf_s);

    std::atomic<int> counter{0};
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> ths;
    for (int t = 0; t < threads; ++t) {
        ths.emplace_back([&]{
            while (true) {
                int i = counter.fetch_add(1);
                if (i >= numOperations) break;
                std::string key;
                if (dist_type == "zipf") {
                    int id = zipf.next();
                    key = "key" + std::to_string(id);
                } else {
                    key = "key" + std::to_string(i);
                }
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