#pragma once
// lab/timing.hpp - stopwatch, process CPU time and a small statistics collector.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <numeric>
#include <string>
#include <vector>

#include "ui.hpp"

namespace lab {

class Stopwatch {
    using clock = std::chrono::steady_clock;
    clock::time_point t0_ = clock::now();
public:
    void reset() { t0_ = clock::now(); }
    double ms() const { return std::chrono::duration<double, std::milli>(clock::now() - t0_).count(); }
    double us() const { return std::chrono::duration<double, std::micro>(clock::now() - t0_).count(); }
};

// CPU time consumed by the whole process (all threads). Great for "busy-wait vs sleep".
inline double process_cpu_ms() { return 1000.0 * static_cast<double>(std::clock()) / CLOCKS_PER_SEC; }

class Stats {
    std::vector<double> v_;
public:
    void add(double x) { v_.push_back(x); }
    void merge(const Stats& o) { v_.insert(v_.end(), o.v_.begin(), o.v_.end()); }
    std::size_t count() const { return v_.size(); }
    double min() const { return v_.empty() ? 0 : *std::min_element(v_.begin(), v_.end()); }
    double max() const { return v_.empty() ? 0 : *std::max_element(v_.begin(), v_.end()); }
    double mean() const { return v_.empty() ? 0 : std::accumulate(v_.begin(), v_.end(), 0.0) / static_cast<double>(v_.size()); }
    double percentile(double p) const {
        if (v_.empty()) return 0;
        auto s = v_;
        std::sort(s.begin(), s.end());
        auto idx = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(s.size()))) ;
        if (idx > 0) --idx;
        return s[std::min(idx, s.size() - 1)];
    }
    void print(const std::string& title, const std::string& unit) const {
        ui::say(title + "  (" + std::to_string(count()) + " samples)");
        ui::kv("min", ui::num(min()) + " " + unit);
        ui::kv("mean", ui::num(mean()) + " " + unit);
        ui::kv("median (p50)", ui::num(percentile(50)) + " " + unit);
        ui::kv("p99", ui::num(percentile(99)) + " " + unit);
        ui::kv("p99.9", ui::num(percentile(99.9)) + " " + unit);
        ui::kv("MAX  <-- the number real-time cares about", ui::num(max()) + " " + unit);
    }
};

}  // namespace lab
