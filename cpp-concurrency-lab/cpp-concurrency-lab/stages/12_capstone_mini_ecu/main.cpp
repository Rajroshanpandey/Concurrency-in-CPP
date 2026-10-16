// Stage 12 / CAPSTONE - Mini ECU: every concept from the course in one running system.
//
//   usage: mini_ecu [--seconds N] [--coupled] [--rt]
//     --coupled : BAD design - control loop calls a BLOCKING push to the slow logger (watch jitter explode)
//     --rt      : try SCHED_FIFO for the control loop (needs privileges)
#include <cstring>
#include <lab/ui.hpp>

#include "components.hpp"

int main(int argc, char** argv) {
    using namespace lab;
    using namespace ecu;
    double seconds = 5;
    bool coupled = false, rt = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = std::stod(argv[++i]);
        else if (!std::strcmp(argv[i], "--coupled")) coupled = true;
        else if (!std::strcmp(argv[i], "--rt")) rt = true;
    }

    ui::banner(12, "CAPSTONE", "Mini ECU", coupled ? "mode: COUPLED logger (the bad design)" : "mode: DECOUPLED logger (the good design)");
    ui::println(R"(
      sensor jthread --(seqlock)--------\
      CAN "ISR" --(SPSC lock-free)--> CAN worker --(mutex: setpoint)--> shared state --> CONTROL LOOP (1 ms)
      fault flag (atomic<bool>) ----------------------------------------------------/          |
                                                                                    try_push (never blocks)
                                                                                                v
                                                                         LOGGER (50 ms per write, BlockingQueue)
    )");

    ui::section("Start-up self test (std::async + future)");
    const auto r = run_self_test().get();
    ui::expect(r.seqlock_ok && r.spsc_ok && r.queue_ok, "seqlock, SPSC and blocking queue all passed their self-test");

    SharedState state;
    CanQueue can_q;
    LogQueue log_q(8);
    LoggerStats log_stats;
    ControlReport report;

    {   // Everything below is owned by jthreads: leaving this scope = request_stop + join, in reverse order.
        std::promise<void> sensor_ready;
        auto ready_f = sensor_ready.get_future();
        auto sensor = start_sensor(state, std::move(sensor_ready), seconds * 0.7);
        ready_f.wait();                                  // control must not start before the first sample exists
        ui::say("Sensor is up. Starting logger, CAN and the 1 ms control loop for " + ui::num(seconds, 1) + " s ...");
        auto logger = start_logger(log_q, log_stats);
        auto can_worker = start_can_worker(state, can_q);
        auto can_isr = start_can_isr(state, can_q);
        auto control = start_control(state, log_q, log_stats, report, coupled, rt ? 80 : 0);

        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
        ui::say("Time is up - shutting down (stop_token to every task)...");
    }   // jthreads destroyed: control, isr, worker, logger, sensor stop and join automatically

    ui::section("Report");
    report.lateness_us.print("control loop lateness", "us");
    ui::kv("control cycles / overruns (>1 ms late)", std::to_string(report.cycles) + " / " + std::to_string(report.overruns));
    ui::kv("fault cycles (over-temperature)", std::to_string(report.fault_cycles));
    ui::kv("CAN frames processed / dropped", std::to_string(state.can_frames_rx.load()) + " / " + std::to_string(state.can_frames_dropped.load()));
    ui::kv("log entries written / dropped", std::to_string(log_stats.written.load()) + " / " + std::to_string(log_stats.dropped.load()));
    ui::kv("final fault flag", state.fault.load() ? "SET" : "clear");

    ui::expect(report.cycles > 0, "control loop ran");
    ui::expect(state.can_frames_rx > 0, "CAN frames flowed ISR -> queue -> worker");
    ui::expect(state.fault.load(), "injected over-temperature raised the atomic fault flag");
    ui::expect(report.fault_cycles > 0, "control loop reacted to the fault (motor command forced to 0)");
    ui::expect(log_stats.written > 0, "logger wrote entries");

    ui::observe(coupled
        ? "COUPLED mode: the control loop waits whenever the 8-slot log queue is full, i.e. it runs at the "
          "logger's 50 ms pace -> the max lateness is tens of ms and overruns pile up. Re-run WITHOUT --coupled and compare."
        : "DECOUPLED mode: try_push never blocks. When the logger falls behind we DROP log lines and count them - "
          "the control loop's max lateness stays at OS-scheduling noise level. Re-run with --coupled to see what "
          "the architecture buys you.");
    ui::why("The whole architecture exists for one acceptance criterion: the logger's slowness must NOT show up "
            "in the control loop's jitter. Slow, unimportant work goes behind a bounded queue with a "
            "non-blocking producer; critical work never waits for it.");
    return ui::finish();
}
