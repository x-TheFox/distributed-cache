#include "metrics/metrics.h"
#include <algorithm>

Metrics& Metrics::instance() {
    static Metrics m;
    return m;
}

void Metrics::record_latency_us(uint64_t us) {
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_us_.push_back(us);
    if (latencies_us_.size() > 100000) latencies_us_.erase(latencies_us_.begin(), latencies_us_.begin()+latencies_us_.size()/2);
}

double Metrics::percentile(double p) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latencies_us_.empty()) return 0.0;
    std::vector<uint64_t> copy = latencies_us_;
    std::sort(copy.begin(), copy.end());
    size_t idx = (size_t)((p/100.0) * copy.size());
    if (idx >= copy.size()) idx = copy.size()-1;
    return (double)copy[idx];
}
