#pragma once
// Shared data types and the SharedState that every ECU component talks through.
#include <atomic>
#include <cstdint>
#include <lab/seqlock.hpp>
#include <mutex>

namespace ecu {

// Trivially copyable => can be published through a SeqLock (writer never blocks).
struct SensorSnapshot {
    double temperature_c = 20.0;
    double pressure_kpa = 100.0;
    std::uint64_t sequence = 0;
};

// Several related fields that must change together => protected by a mutex.
struct Config {
    double setpoint_c = 25.0;
    double kp = 0.8;
    double max_temp_c = 90.0;
};

struct CanFrame {
    std::uint32_t id = 0;
    double value = 0;
};

struct LogEntry {
    std::uint64_t cycle = 0;
    double temperature_c = 0;
    double motor_cmd = 0;
    bool fault = false;
};

struct SharedState {
    lab::SeqLock<SensorSnapshot> sensor;     // Stage 5: seqlock for a consistent multi-field snapshot
    std::mutex cfg_m;                        // Stage 3: mutex + lock_guard for related fields
    Config cfg;
    std::atomic<bool> fault{false};          // Stage 5: atomic flag
    std::atomic<std::uint64_t> can_frames_rx{0};
    std::atomic<std::uint64_t> can_frames_dropped{0};

    Config config_copy() {
        std::lock_guard lk(cfg_m);
        return cfg;
    }
    void set_setpoint(double v) {
        std::lock_guard lk(cfg_m);
        cfg.setpoint_c = v;
    }
};

}  // namespace ecu
