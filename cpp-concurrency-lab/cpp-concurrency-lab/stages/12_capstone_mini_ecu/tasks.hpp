#pragma once
// The ECU's tasks. Every task is a std::jthread that polls its stop_token (stage 9).
#include <chrono>
#include <cmath>
#include <future>
#include <lab/blocking_queue.hpp>
#include <lab/spsc_queue.hpp>
#include <lab/timing.hpp>
#include <random>
#include <stop_token>
#include <thread>

#include "shared_state.hpp"

namespace ecu {
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

using CanQueue = lab::SpscQueue<CanFrame, 256>;       // "ISR" -> worker: lock-free SPSC (stage 6)
using LogQueue = lab::BlockingQueue<LogEntry>;        // control -> logger: bounded blocking queue (stage 4)

// ---- SENSOR: 100 Hz, publishes into the seqlock. Injects a temperature spike at `fault_at`. ----
inline void sensor_task(std::stop_token st, SharedState& S, std::promise<void> ready, Clock::time_point fault_at) {
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 0.2);
    auto next = Clock::now();
    std::uint64_t n = 0;
    ready.set_value();                                   // future/promise handshake: "I am up" (stage 7)
    while (!st.stop_requested()) {
        next += 10ms;
        const bool spike = Clock::now() >= fault_at;
        const double t = (spike ? 120.0 : 58.0 + 4.0 * std::sin(0.05 * static_cast<double>(n))) + noise(rng);
        S.sensor.store({t, 101.3 + noise(rng), ++n});
        std::this_thread::sleep_until(next);             // absolute time: no drift (stage 11)
    }
}

// ---- CAN "ISR": produces frames every ~2 ms into the SPSC queue; NEVER blocks, drops if full. ----
inline void can_isr_task(std::stop_token st, SharedState& S, CanQueue& q) {
    auto next = Clock::now();
    std::uint32_t k = 0;
    while (!st.stop_requested()) {
        next += 2ms;
        const CanFrame f{(k % 50 == 0) ? 0x100u : 0x200u, 55.0 + static_cast<double>(k % 20)};
        if (!q.push(f)) S.can_dropped.fetch_add(1, std::memory_order_relaxed);
        ++k;
        std::this_thread::sleep_until(next);
    }
}

// ---- CAN worker: drains the queue, decodes, updates config under the mutex. ----
inline void can_worker_task(std::stop_token st, SharedState& S, CanQueue& q) {
    CanFrame f;
    while (!st.stop_requested()) {
        bool any = false;
        while (q.pop(f)) {
            any = true;
            S.can_frames.fetch_add(1, std::memory_order_relaxed);
            if (f.id == 0x100) S.write_setpoint(f.value);      // 0x100 = "new setpoint" message
        }
        if (!any) std::this_thread::sleep_for(500us);         // poll with sleep: simple, bounded
    }
}

// ---- LOGGER: deliberately SLOW (50 ms per entry, like a flash write). Must not disturb control. ----
inline void logger_task(std::stop_token st, SharedState& S, LogQueue& q) {
    for (;;) {
        auto e = q.pop_for(20ms);                             // never wait forever (stage 4)
        if (!e) {
            if (st.stop_requested() || q.closed()) break;
            continue;
        }
        std::this_thread::sleep_for(50ms);                    // the "slow write"
        S.log_written.fetch_add(1, std::memory_order_relaxed);
    }
}

// ---- CONTROL: 1 kHz loop. Records lateness (jitter) of every cycle. ----
struct ControlReport {
    lab::Stats lateness_us;
    std::uint64_t cycles = 0, overruns = 0;
    bool saw_fault = false;
};

// `coupled` = deliberately BAD design: the control loop does a BLOCKING push to the slow logger.
inline void control_task(std::stop_token st, SharedState& S, LogQueue& logq, ControlReport& rep, bool coupled) {
    const auto period = 1ms;
    auto next = Clock::now();
    double motor = 0;
    while (!st.stop_requested()) {
        next += period;
        std::this_thread::sleep_until(next);
        const auto wake = Clock::now();
        rep.lateness_us.add(std::chrono::duration<double, std::micro>(wake - next).count());
        ++rep.cycles;

        const SensorSnapshot snap = S.sensor.load();           // consistent pair, never blocks the writer
        const Config cfg = S.read_cfg();                       // short critical section
        if (snap.temperature_c > 100.0) S.fault.store(true);   // latch the fault
        const bool fault = S.fault.load();
        if (fault) { motor = 0; rep.saw_fault = true; }
        else motor = std::clamp(cfg.gain * (cfg.setpoint_c - snap.temperature_c), -1.0, 1.0);

        if (rep.cycles % 20 == 0) {                            // 50 Hz log entries (logger can only do 20/s)
            LogEntry e{rep.cycles, snap.temperature_c, motor, fault};
            if (coupled) logq.push(e);                         // BAD: blocks when the logger is behind
            else if (!logq.try_push(e)) S.log_dropped.fetch_add(1, std::memory_order_relaxed);   // GOOD: drop + count
        }
        if (Clock::now() > next + period) ++rep.overruns;      // finished after the next deadline
    }
}

// ---- SELF-TEST: runs asynchronously before start-up, returns a result through a future. ----
struct SelfTestResult {
    bool seqlock_consistent = false, spsc_roundtrip = false, queue_timeout_ok = false;
    bool ok() const { return seqlock_consistent && spsc_roundtrip && queue_timeout_ok; }
};

inline SelfTestResult self_test() {
    SelfTestResult r;
    SharedState s;
    s.sensor.store({42.0, 84.0, 1});
    auto snap = s.sensor.load();
    r.seqlock_consistent = snap.temperature_c == 42.0 && snap.pressure_kpa == 84.0;
    CanQueue q;
    CanFrame in{0x100, 1.5}, out{};
    r.spsc_roundtrip = q.push(in) && q.pop(out) && out.id == 0x100 && out.value == 1.5;
    LogQueue lq(2);
    r.queue_timeout_ok = !lq.pop_for(5ms).has_value();
    return r;
}

}  // namespace ecu
