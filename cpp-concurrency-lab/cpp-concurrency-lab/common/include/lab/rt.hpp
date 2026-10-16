#pragma once
// lab/rt.hpp - real-time helpers for stage 11 and the capstone (Linux-specific parts are guarded).
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

#include "timing.hpp"

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#endif

namespace lab::rt {

// Returns 0 on success, else an errno-style code (EPERM when not privileged).
inline int set_fifo_priority(int prio) {
#ifdef __linux__
    sched_param sp{};
    sp.sched_priority = prio;
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
#else
    (void)prio; return -1;
#endif
}

inline int pin_to_cpu(int cpu) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)cpu; return -1;
#endif
}

inline int lock_memory() {
#ifdef __linux__
    return mlockall(MCL_CURRENT | MCL_FUTURE) == 0 ? 0 : -1;
#else
    return -1;
#endif
}

// CPU hogs: they steal cores to make scheduling jitter visible.
class Hogs {
    std::vector<std::jthread> t_;
public:
    explicit Hogs(int n) {
        for (int i = 0; i < n; ++i)
            t_.emplace_back([](std::stop_token st) {
                volatile unsigned long x = 0;
                while (!st.stop_requested())
                    for (int k = 0; k < 20000; ++k) x = x + 1;
            });
    }
};

inline void busy_for(std::chrono::microseconds d) {
    const auto end = std::chrono::steady_clock::now() + d;
    while (std::chrono::steady_clock::now() < end) {}
}

// Runs a periodic loop and records LATENESS (microseconds) = actual wake time - ideal wake time,
// where the ideal grid is start + k*period.
//   absolute=true : sleep_until(next)  -> errors do not accumulate
//   absolute=false: sleep_for(period)  -> every iteration's overhead adds up (DRIFT)
inline Stats measure_lateness(std::chrono::microseconds period, int iterations, bool absolute,
                              const std::function<void()>& work = {}) {
    using clk = std::chrono::steady_clock;
    Stats s;
    const auto start = clk::now();
    auto next = start;
    for (int k = 1; k <= iterations; ++k) {
        if (work) work();
        const auto ideal = start + period * k;
        if (absolute) {
            next += period;
            std::this_thread::sleep_until(next);
        } else {
            std::this_thread::sleep_for(period);
        }
        s.add(std::chrono::duration<double, std::micro>(clk::now() - ideal).count());
    }
    return s;
}

}  // namespace lab::rt
