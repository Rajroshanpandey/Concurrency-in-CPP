// Stage 11 / MEDIUM - Measure jitter, then add CPU load: the average barely moves, the MAX explodes.
#include <chrono>
#include <lab/rt.hpp>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    using namespace std::chrono_literals;
    const int iters = argc > 1 ? std::stoi(argv[1]) : 20000;
    ui::banner(11, "MEDIUM", "Jitter: fast is not deterministic", "average vs worst case under load");

    ui::say("We run a 1 ms periodic loop for " + std::to_string(iters) + " iterations and record "
            "lateness = (actual wake) - (ideal wake) in microseconds. Then we repeat while background "
            "threads hog the CPU. Real-time engineers read the MAX and the 99.9th percentile, not the mean.");

    ui::section("1) Quiet system");
    const auto quiet = rt::measure_lateness(1ms, iters, true);
    quiet.print("lateness, no load", "us");

    const int hogs = static_cast<int>(std::max(2u, std::thread::hardware_concurrency() * 2));
    ui::section("2) With " + std::to_string(hogs) + " CPU-hogging threads");
    ui::say("The hogs are ordinary-priority threads: the scheduler must share the CPU between them and "
            "our loop, so our wake-ups get delayed by up to a scheduling time-slice.");
    Stats loaded;
    {
        rt::Hogs h(hogs);
        loaded = rt::measure_lateness(1ms, iters, true);
    }
    loaded.print("lateness, under load", "us");

    ui::section("Summary");
    ui::kv("mean  quiet -> loaded", ui::num(quiet.mean(), 0) + " -> " + ui::num(loaded.mean(), 0) + " us");
    ui::kv("MAX   quiet -> loaded", ui::num(quiet.max(), 0) + " -> " + ui::num(loaded.max(), 0) + " us");
    ui::observe("The mean may look acceptable while the max is milliseconds - a 1 ms control loop that is "
                "occasionally 5 ms late has missed deadlines. Averages hide exactly the events that crash "
                "drones and trip protections. Next demo: what you can do about it (real-time priority).");
    ui::why("Sources of jitter on Linux: timer interrupt granularity, scheduler time-slices, other tasks, "
            "page faults, malloc, cache misses, interrupts, CPU frequency changes, virtualisation (a "
            "container shares a host kernel). Stock Linux is 'soft real-time'; PREEMPT_RT patches and "
            "RTOSes shrink the tail.");
    ui::expect(quiet.count() == static_cast<std::size_t>(iters), "all iterations were measured");
    return ui::finish();
}
