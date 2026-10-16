// Stage 11 / EASY - A periodic task done right: sleep_until(absolute) vs sleep_for(relative) = drift.
#include <chrono>
#include <lab/rt.hpp>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    using namespace std::chrono_literals;
    const int seconds = argc > 1 ? std::stoi(argv[1]) : 10;
    const auto period = 1ms;
    const int iterations = seconds * 1000;
    ui::banner(11, "EASY", "Periodic task: drift", "sleep_until(next) vs sleep_for(period)");

    ui::say("A control loop must run every 1 ms. Each iteration does ~200 us of work. Two ways to wait:");
    ui::code(
        "WRONG:  do_work(); sleep_for(1ms);          // period = work + 1 ms + wake-up error, EVERY loop\n"
        "RIGHT:  next += 1ms; do_work(); sleep_until(next);   // period fixed by an absolute time grid");
    ui::say("We run each for " + std::to_string(seconds) + " s and measure LATENESS: how far the wake-up "
            "is from the ideal grid point start + k*1ms. (Run longer with an argument: ./s11_easy_periodic_drift 60)");

    auto work = [] { rt::busy_for(200us); };
    const auto rel = rt::measure_lateness(period, iterations, /*absolute=*/false, work);
    const auto abs = rt::measure_lateness(period, iterations, /*absolute=*/true, work);

    ui::section("sleep_for(period)  -  relative");
    ui::kv("lateness at the LAST iteration", ui::num(rel.max() / 1000.0, 1) + " ms behind schedule");
    ui::section("sleep_until(next)  -  absolute");
    ui::kv("worst lateness of any iteration", ui::num(abs.max(), 0) + " us");
    ui::kv("mean lateness", ui::num(abs.mean(), 0) + " us");

    ui::expect(rel.max() > abs.max() * 2, "relative sleeping accumulates error; absolute sleeping does not");
    ui::observe("The relative loop falls further behind with every iteration (200 us of work + the OS wake-up "
                "latency are added to each period), so after " + std::to_string(seconds) + " s it has run "
                "noticeably FEWER cycles than it should have. The absolute loop is late by at most one wake-up "
                "latency, forever, because each deadline is computed from the start, not from 'now'.");
    ui::why("steady_clock (never jumps) + sleep_until(absolute time) is THE pattern for periodic tasks. "
            "If an iteration overruns its slot, sleep_until returns immediately and the loop catches up - "
            "you must still detect overruns (stage 11 medium) because catching up is not meeting the deadline.");
    return ui::finish();
}
