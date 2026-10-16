// Stage 7 / TOUGH - Parallel merge sort with std::async: naive vs depth-limited, plus the blocking future destructor.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using It = std::vector<int>::iterator;
static std::atomic<int> g_tasks{0};

// depth_left <= 0 -> recurse serially. kLeaf stops recursion on small ranges.
static void merge_sort(It b, It e, int depth_left) {
    const auto n = e - b;
    if (n <= 64) { std::sort(b, e); return; }
    It mid = b + n / 2;
    if (depth_left > 0) {
        ++g_tasks;
        auto left = std::async(std::launch::async, merge_sort, b, mid, depth_left - 1);
        merge_sort(mid, e, depth_left - 1);   // do the other half ourselves
        left.get();                           // also rethrows any exception
    } else {
        merge_sort(b, mid, 0);
        merge_sort(mid, e, 0);
    }
    std::inplace_merge(b, mid, e);
}

int main(int argc, char** argv) {
    using namespace lab;
    const std::size_t n = argc > 1 ? std::stoull(argv[1]) : 400'000;
    ui::banner(7, "TOUGH", "Parallel merge sort with std::async", "thread explosion, depth limits, blocking destructors");

    std::vector<int> base(n);
    std::mt19937 rng(42);
    for (auto& x : base) x = static_cast<int>(rng());
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    const int good_depth = static_cast<int>(std::ceil(std::log2(static_cast<double>(cores))));
    ui::kv("elements / cores", std::to_string(n) + " / " + std::to_string(cores));

    auto run = [&](const char* name, int depth) {
        auto v = base;
        g_tasks = 0;
        Stopwatch sw;
        merge_sort(v.begin(), v.end(), depth);
        const double ms = sw.ms();
        const bool sorted = std::is_sorted(v.begin(), v.end());
        ui::kv(name, ui::num(ms, 1) + " ms   async tasks: " + std::to_string(g_tasks.load()));
        ui::expect(sorted, std::string(name) + ": output is sorted");
        return ms;
    };

    ui::section("Serial vs naive vs depth-limited");
    ui::say("depth 0 = serial.  'Naive' spawns a task at EVERY recursion level (hundreds of threads).  "
            "'Limited' spawns only while depth < log2(cores).");
    const double t_ser = run("serial (depth 0)", 0);
    const double t_naive = run("naive (spawn at every level)", 12);
    const double t_lim = run("limited (depth = log2(cores))", std::max(1, good_depth));

    ui::observe(
        "Naive creates thousands of OS threads: each costs ~10-100 us to start and ~8 MB of virtual "
        "stack, and they all fight for " + std::to_string(cores) + " core(s). It is usually SLOWER "
        "than serial (here " + ui::num(t_naive / t_ser, 1) + "x serial). The limited version spawns a handful of "
        "tasks and is the only one that can beat serial on a multi-core machine (here " +
        ui::num(t_lim / t_ser, 2) + "x serial's time).");
    ui::why("Parallelism needs GRAIN SIZE: tasks must be big enough to pay for their creation. "
            "This is exactly what thread pools and work-stealing schedulers (stage 8) manage for you.");

    ui::section("The blocking destructor of a future from std::async");
    ui::say("A future obtained from std::async(launch::async, ...) BLOCKS in its destructor until the task "
            "finishes. So ignoring the return value silently makes the call synchronous:");
    ui::code("std::async(std::launch::async, [] { sleep(300ms); });   // temporary future destroyed at ';'");
    Stopwatch sw;
    (void)std::async(std::launch::async, [] { std::this_thread::sleep_for(300ms); });   // discarded on purpose
    const double blocked = sw.ms();
    ui::kv("time spent on that one statement", ui::num(blocked, 0) + " ms");
    ui::expect(blocked >= 280, "the 'asynchronous' call blocked for the task's whole duration");

    sw.reset();
    { std::promise<int> p; auto f = p.get_future(); (void)f; }   // promise-based future: destructor does NOT block
    ui::kv("destroying a promise's future", ui::num(sw.ms(), 2) + " ms");
    ui::why("Only futures created by std::async carry this 'join on destruction' rule. Keep the future in a "
            "variable ('auto f = std::async(...)') or you have written sequential code with extra steps.");
    return ui::finish();
}
