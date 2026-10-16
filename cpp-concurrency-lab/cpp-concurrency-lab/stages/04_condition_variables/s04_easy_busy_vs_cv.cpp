// Stage 4 / EASY - Waiting by polling (burns CPU) vs. waiting on a condition variable (sleeps).
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;
constexpr auto kWork = 1000ms;  // how long the "worker" takes to produce its result

int main() {
    using namespace lab;
    ui::banner(4, "EASY", "Busy-wait vs condition_variable", "same job, wildly different CPU bill");

    ui::say(
        "A worker needs ~1 second to get ready. The main thread must wait for it. We measure the "
        "CPU time the WHOLE PROCESS burns while waiting (std::clock = CPU seconds, not wall seconds).");

    // ---------- A) busy loop ----------
    ui::section("A) Busy waiting:   while (!ready) {}");
    std::atomic<bool> ready_a{false};
    double cpu0 = process_cpu_ms();
    Stopwatch sw;
    std::thread wa([&] { std::this_thread::sleep_for(kWork); ready_a = true; });
    while (!ready_a.load()) {}            // spin: asks "are we there yet?" millions of times a second
    wa.join();
    const double cpu_busy = process_cpu_ms() - cpu0;
    ui::kv("wall time", ui::num(sw.ms(), 0) + " ms");
    ui::kv("CPU time burned", ui::num(cpu_busy, 0) + " ms");

    // ---------- B) condition variable ----------
    ui::section("B) condition_variable:   cv.wait(lock, pred)");
    bool ready_b = false;
    std::mutex m;
    std::condition_variable cv;
    cpu0 = process_cpu_ms();
    sw.reset();
    std::thread wb([&] {
        std::this_thread::sleep_for(kWork);
        { std::lock_guard lk(m); ready_b = true; }   // change the state UNDER the mutex ...
        cv.notify_one();                             // ... then wake the sleeper
    });
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return ready_b; });        // sleeps in the kernel; zero CPU
    }
    wb.join();
    const double cpu_cv = process_cpu_ms() - cpu0;
    ui::kv("wall time", ui::num(sw.ms(), 0) + " ms");
    ui::kv("CPU time burned", ui::num(cpu_cv, 0) + " ms");

    ui::observe(
        "Same wall time (~1000 ms), but the spin loop burned roughly a full core (~1000 ms of CPU) "
        "while the condition variable burned ~0. On a battery-powered or thermally limited device, "
        "that difference is the whole battery life. In a real-time system it also steals CPU from "
        "lower-priority tasks.");
    ui::why(
        "wait() atomically (1) releases the mutex and (2) puts the thread to sleep, so no wake-up "
        "can slip between 'I checked' and 'I went to sleep'. notify_one() wakes it; it re-takes the "
        "mutex and re-checks the predicate.");
    ui::expect(cpu_cv < cpu_busy / 4, "condition_variable used far less CPU than the spin loop");
    return ui::finish();
}
