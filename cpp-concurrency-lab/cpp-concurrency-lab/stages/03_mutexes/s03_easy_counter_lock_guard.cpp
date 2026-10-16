// Stage 3 / EASY - Fix the counter with std::lock_guard, then measure what safety costs.
// (Contains one deliberately racy timing run, so ctest skips it under TSan.)
#include <atomic>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>
#include <vector>

template <class Body>
static double run_threads(int threads, Body body) {
    lab::Stopwatch sw;
    std::vector<std::thread> ts;
    for (int i = 0; i < threads; ++i) ts.emplace_back(body);
    for (auto& t : ts) t.join();
    return sw.ms();
}

int main(int argc, char** argv) {
    using namespace lab;
    const int per_thread = argc > 1 ? std::stoi(argv[1]) : 2'000'000;
    const int threads = 4;
    ui::banner(3, "EASY", "lock_guard: the safe counter", "RAII locking - the unlock you cannot forget");

    ui::say(
        "Four threads each add 1 to a shared counter " + std::to_string(per_thread) + " times. "
        "We run it three ways and compare correctness and speed.");
    ui::code(
        "void increment() {\n"
        "    std::lock_guard<std::mutex> lock(m);   // constructor locks ...\n"
        "    ++counter;\n"
        "}                                          // ... destructor unlocks, even on exception/return");

    // 1. mutex
    std::mutex m;
    long long locked = 0;
    const double t_mutex = run_threads(threads, [&] {
        for (int i = 0; i < per_thread; ++i) { std::lock_guard lk(m); ++locked; }
    });
    // 2. atomic
    std::atomic<long long> atom{0};
    const double t_atomic = run_threads(threads, [&] {
        for (int i = 0; i < per_thread; ++i) atom.fetch_add(1);
    });
    // 3. racy (UB, timing only!)
    static long long racy = 0;
    const double t_racy = run_threads(threads, [&] {
        for (int i = 0; i < per_thread; ++i) { ++racy; asm volatile("" ::: "memory"); }
    });

    const long long expected = static_cast<long long>(threads) * per_thread;
    ui::section("Results (expected " + std::to_string(expected) + ")");
    ui::kv("lock_guard + int", std::to_string(locked) + "   " + ui::num(t_mutex) + " ms");
    ui::kv("std::atomic<long long>", std::to_string(atom.load()) + "   " + ui::num(t_atomic) + " ms");
    ui::kv("unsynchronised (UB!)", std::to_string(racy) + "   " + ui::num(t_racy) + " ms");

    ui::observe(
        "The mutex version is always exact. The racy version is the fastest AND wrong - speed that "
        "gives wrong answers is worthless. The atomic is exact and usually faster than the mutex "
        "because it needs no kernel/futex round-trip when uncontended. Contention (4 threads "
        "hammering one cache line) is what makes both slower than the racy run.");
    ui::why(
        "lock_guard is RAII: lock in the constructor, unlock in the destructor. If the code between "
        "throws or returns early, the destructor still runs. Manual lock()/unlock() pairs leak the "
        "lock on every early return - the #1 cause of 'my program hangs'.");
    ui::expect(locked == expected, "mutex counter is exact");
    ui::expect(atom.load() == expected, "atomic counter is exact");
    return ui::finish();
}
