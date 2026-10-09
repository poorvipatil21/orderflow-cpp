#pragma once
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <string>

namespace orderflow {

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::nanoseconds;

struct BenchmarkResult {
    uint64_t    total_messages   = 0;
    uint64_t    elapsed_ns       = 0;
    double      throughput_mps   = 0.0;  // million messages/sec
    double      latency_p50_ns   = 0.0;
    double      latency_p99_ns   = 0.0;
    double      latency_p999_ns  = 0.0;
    uint64_t    peak_memory_kb   = 0;
    std::string dataset;
};

class Timer {
public:
    void start()  { t0_ = Clock::now(); }
    uint64_t elapsed_ns() const {
        return std::chrono::duration_cast<ns>(Clock::now() - t0_).count();
    }
private:
    Clock::time_point t0_;
};

// Record per-event latency samples (call tick() per message)
class LatencyRecorder {
public:
    void tick() {
        auto now = Clock::now();
        if (last_.time_since_epoch().count() != 0)
            samples_.push_back(
                std::chrono::duration_cast<ns>(now - last_).count());
        last_ = now;
    }
    double percentile(double p) {
        if (samples_.empty()) return 0.0;
        std::sort(samples_.begin(), samples_.end());
        size_t idx = static_cast<size_t>(p / 100.0 * samples_.size());
        if (idx >= samples_.size()) idx = samples_.size() - 1;
        return static_cast<double>(samples_[idx]);
    }
private:
    std::vector<uint64_t> samples_;
    Clock::time_point     last_{};
};

// Read current process RSS (Linux /proc/self/status)
uint64_t current_rss_kb();

} // namespace orderflow
