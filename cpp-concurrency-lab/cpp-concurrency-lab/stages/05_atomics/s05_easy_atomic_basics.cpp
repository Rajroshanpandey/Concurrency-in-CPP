// Stage 5 / EASY - std::atomic counter and a std::atomic<bool> stop flag.
#include <atomic>
#include <chrono>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(5, "EASY", "Atomics: counter and stop flag", "indivisible operations, no mutex required");

    ui::section("1) Atomic counter");
    ui::say("The racy counter from stage 2, fixed WITHOUT a mutex. fetch_add is one indivisible "
            "read-modify-write: no other thread can ever see the 'half-way' state.");
    std::atomic<int> counter{0};
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < 4; ++i) ts.emplace_back([&] { for (int k = 0; k < 500000; ++k) ++counter; });
        for (auto& t : ts) t.join();
    }
    ui::kv("final counter", std::to_string(counter.load()));
    ui::expect(counter == 2'000'000, "4 x 500000 increments, none lost");

    ui::section("2) Atomic stop flag");
    ui::say("A worker loops until another thread says stop. A plain `bool running` is a data race "
            "(and the compiler may even hoist the read out of the loop, creating an infinite loop!). "
            "std::atomic<bool> is the correct tool.");
    std::atomic<bool> running{true};
    std::atomic<long> iterations{0};
    std::thread worker([&] { while (running.load()) { ++iterations; std::this_thread::yield(); } });
    std::this_thread::sleep_for(100ms);
    running = false;     // seq_cst store: visible to the worker "promptly"
    worker.join();
    ui::kv("worker iterations before stop", std::to_string(iterations.load()));
    ui::expect(iterations > 0, "worker ran, then observed the stop flag and exited");

    ui::section("3) What the hardware says");
    ui::kv("sizeof(std::atomic<int>)", std::to_string(sizeof(std::atomic<int>)));
    ui::kv("atomic<int>::is_always_lock_free", std::atomic<int>::is_always_lock_free ? "true" : "false");
    ui::kv("atomic<long long> lock-free?", std::atomic<long long>::is_always_lock_free ? "true" : "false");
    struct Big { long a, b, c, d; };
    ui::kv("atomic<Big{4 longs}> lock-free?", std::atomic<Big>::is_always_lock_free ? "true" : "false (would need a hidden lock / libatomic!)");

    ui::observe(
        "std::atomic<T> is only truly lock-free for small T that the CPU can operate on directly. "
        "A large struct silently falls back to an internal lock, which destroys the real-time / "
        "ISR-safety you wanted. ALWAYS check is_lock_free() / is_always_lock_free on your target.");
    ui::why(
        "Rule of thumb: atomics for ONE small piece of state (flag, counter, index). A mutex for "
        "state made of SEVERAL related fields that must change together (next demo-but-one).");
    return ui::finish();
}
