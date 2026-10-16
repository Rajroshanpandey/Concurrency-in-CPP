#pragma once
// The ECU's tasks. Each is a std::jthread that checks its stop_token (Stage 9).
#include <chrono>
#include <cmath>
#include <future>
#include <lab/blocking_queue.hpp>
#include <lab/rt.hpp>
#include <lab/spsc_queue.hpp>
#include <lab/timing.hpp>
#include <thread>

#include "types.hpp"

namespace ecu {
using namespace std::chrono_literals;

using CanQueue = lab::SpscQueue<CanFrame, 256>;   // Stage 6: ISR-like producer -> one worker
using LogQueue = lab::BlockingQueue<LogEntry>;    // Stage 4: bounded blocking queue for the slow logger

// ---------------------------------------------------------------- sensor task (10 ms period)
inline std::jthread start_sensor(SharedState& st, std::promise<void> ready, double fault_after_s) {
    return std::jthread([&st, ready = std::move(ready), fault_after_s](std::stop_token stop) mutable {
        using clk = std::chrono::steady_clock;
        const auto t0 = clk::now();
        auto next = t0;
        std::uint64_t seq = 0;
        bool signalled = false;
        while (!stop.stop_requested()) {
            const double t = std::chrono::duration<double>(clk::now() - t0).count();
            SensorSnapshot s;
            s.temperature_c = 24.0 + 3.0 * std::sin(t * 2.0);
            if (fault_after_s > 0 && t > fault_after_s) s.temperature_c = 95.0;   // injected over-temperature
            s.pressure_kpa = 100.0 + s.temperature_c * 0.1;
            s.sequence = ++seq;
            st.sensor.store(s);
            if (!signalled) { ready.set_value(); signalled = true; }   // Stage 7: promise as a start-up handshake
            next += 10ms;
            std::this_thread::sleep_until(next);                       // Stage 11: absolute time
        }
    });
}

// ---------------------------------------------------------------- "CAN interrupt" (produces frames)
inline std::jthread start_can_isr(SharedState& st, CanQueue& q) {
    return std::jthread([&st, &q](std::stop_token stop) {
        auto next = std::chrono::steady_clock::now();
        int n = 0;
        while (!stop.stop_requested()) {
            CanFrame f{0x100, 25.0 + (n++ % 5)};            // setpoint request
            if (!q.push(f)) ++st.can_frames_dropped;        // an ISR must NEVER block: drop and count
            next += 2ms;
            std::this_thread::sleep_until(next);
        }
    });
}

// ---------------------------------------------------------------- CAN worker (heavier processing)
inline std::jthread start_can_worker(SharedState& st, CanQueue& q) {
    return std::jthread([&st, &q](std::stop_token stop) {
        CanFrame f;
        while (!stop.stop_requested()) {
            if (q.pop(f)) {
                ++st.can_frames_rx;
                if (f.id == 0x100) st.set_setpoint(f.value);
            } else {
                std::this_thread::sleep_for(200us);   // poll; an RTOS would block on a semaphore from the ISR
            }
        }
    });
}

// ---------------------------------------------------------------- logger (slow: 50 ms per write)
struct LoggerStats {
    std::atomic<std::uint64_t> written{0};
    std::atomic<std::uint64_t> dropped{0};
};

inline std::jthread start_logger(LogQueue& q, LoggerStats& stats) {
    return std::jthread([&q, &stats](std::stop_token stop) {
        for (;;) {
            auto e = q.pop_for(50ms);                 // never wait forever
            if (e) {
                std::this_thread::sleep_for(50ms);    // simulated slow flash / UART write
                ++stats.written;
            } else if (stop.stop_requested() && q.size() == 0) {
                break;                                // drained: now it is safe to exit
            }
        }
    });
}

// ---------------------------------------------------------------- control loop (1 ms, the thing we protect)
struct ControlReport {
    lab::Stats lateness_us;
    std::uint64_t cycles = 0;
    std::uint64_t fault_cycles = 0;
    std::uint64_t overruns = 0;   // lateness above 1 period
};

inline std::jthread start_control(SharedState& st, LogQueue& logq, LoggerStats& ls, ControlReport& rep, bool coupled,
                                  int rt_priority) {
    return std::jthread([&, coupled, rt_priority](std::stop_token stop) {
        if (rt_priority > 0) lab::rt::set_fifo_priority(rt_priority);   // best effort
        using clk = std::chrono::steady_clock;
        const auto start = clk::now();
        auto next = start;
        std::uint64_t k = 0;
        while (!stop.stop_requested()) {
            ++k;
            next += 1ms;
            std::this_thread::sleep_until(next);
            const double late = std::chrono::duration<double, std::micro>(clk::now() - next).count();
            rep.lateness_us.add(late);
            if (late > 1000.0) ++rep.overruns;

            // --- the actual control law ---
            const SensorSnapshot s = st.sensor.load();   // lock-free consistent snapshot
            const Config c = st.config_copy();           // short critical section
            double cmd = c.kp * (c.setpoint_c - s.temperature_c);
            if (s.temperature_c > c.max_temp_c) st.fault.store(true);
            const bool fault = st.fault.load();
            if (fault) { cmd = 0; ++rep.fault_cycles; }

            if (k % 20 == 0) {                           // 50 log entries per second
                LogEntry e{k, s.temperature_c, cmd, fault};
                if (coupled) {
                    logq.push(e);                        // BAD DESIGN: blocks when the logger is slow
                } else if (!logq.try_push(e)) {
                    ++ls.dropped;                        // GOOD DESIGN: never wait, count the loss
                }
            }
        }
        rep.cycles = k;
    });
}

// ---------------------------------------------------------------- start-up self test (async + future)
struct SelfTestResult { bool seqlock_ok = false, spsc_ok = false, queue_ok = false; };

inline std::future<SelfTestResult> run_self_test() {
    return std::async(std::launch::async, [] {
        SelfTestResult r;
        lab::SeqLock<SensorSnapshot> sl;
        sl.store({21.5, 101.0, 7});
        auto s = sl.load();
        r.seqlock_ok = s.sequence == 7 && s.temperature_c == 21.5;
        CanQueue cq;
        CanFrame in{0x1, 2.5}, out;
        r.spsc_ok = cq.push(in) && cq.pop(out) && out.value == 2.5;
        lab::BlockingQueue<int> bq(2);
        bq.push(1);
        auto v = bq.pop_for(10ms);
        r.queue_ok = v && *v == 1;
        return r;
    });
}

}  // namespace ecu
