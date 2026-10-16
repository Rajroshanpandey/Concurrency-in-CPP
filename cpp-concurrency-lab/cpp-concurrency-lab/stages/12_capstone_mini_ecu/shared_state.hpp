#pragma once
// Shared state of the mini ECU. Each member uses the mechanism that fits its access pattern.
#include <atomic>
#include <lab/seqlock.hpp>
#include <mutex>

namespace ecu {

// 1) SENSOR SNAPSHOT: one writer (sensor), one hard-real-time reader (control). Several fields
//    must be consistent -> SeqLock (stage 5). The writer never waits, the reader never blocks it.
struct SensorSnapshot {
    double temperature_c = 0;
    double pressure_kpa = 0;
    std::uint64_t sample_no = 0;
};

// 2) CONFIG: low-rate, multi-field, written by the CAN worker, read by control -> mutex + lock_guard (stage 3).
struct Config {
    double setpoint_c = 60.0;
    double gain = 0.5;
};

struct SharedState {
    lab::SeqLock<SensorSnapshot> sensor;

    mutable std::mutex cfg_m;
    Config cfg;
    Config read_cfg() const { std::lock_guard lk(cfg_m); return cfg; }
    void write_setpoint(double sp) { std::lock_guard lk(cfg_m); cfg.setpoint_c = sp; }

    // 3) FAULT FLAG: a single bit of state -> std::atomic<bool> (stage 5).
    std::atomic<bool> fault{false};

    // 4) COUNTERS: statistics only, nobody orders anything by them -> relaxed (stage 6).
    std::atomic<std::uint64_t> can_frames{0}, can_dropped{0}, log_dropped{0}, log_written{0};
};

struct CanFrame {
    std::uint32_t id;
    double value;
};

struct LogEntry {
    std::uint64_t cycle;
    double temperature_c;
    double motor_cmd;
    bool fault;
};

}  // namespace ecu
