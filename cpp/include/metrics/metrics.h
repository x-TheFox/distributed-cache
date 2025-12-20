#ifndef METRICS_H
#define METRICS_H

#include <vector>
#include <mutex>
#include <cstdint>

class Metrics {
public:
    static Metrics& instance();
    void record_latency_us(uint64_t us);
    // compute p99 and other percentiles
    double percentile(double p);

private:
    std::vector<uint64_t> latencies_us_;
    std::mutex mutex_;
};

#endif // METRICS_H
