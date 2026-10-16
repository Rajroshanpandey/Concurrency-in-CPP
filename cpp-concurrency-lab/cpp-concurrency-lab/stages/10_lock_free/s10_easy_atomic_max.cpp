// Stage 10 / EASY - A lock-free "atomic max" with a compare_exchange loop.
#include <algorithm>
#include <atomic>
#include <lab/ui.hpp>
#include <random>
#include <thread>
#include <vector>

// Raise `m` to at least `v`, never lower it, never lose an update.
static void update_max(std::atomic<int>& m, int v) {
    int cur = m.load();
    // compare_exchange_weak: "if m == cur then m = v (return true)
    //                         else cur = (m's CURRENT value) (return false)".
    // That second line is why `cur` is refreshed for us and the loop needs no extra load().
    while (v > cur && !m.compare_exchange_weak(cur, v)) { /* someone changed m; cur is fresh; re-test */ }
}

int main() {
    using namespace lab;
    ui::banner(10, "EASY", "CAS loop: atomic max", "compare_exchange_weak, the building block of lock-free code");

    ui::say("Compare-and-swap (CAS) is a CPU instruction: 'atomically: if the memory still holds the value "
            "I EXPECT, replace it with my NEW value; otherwise tell me what is there now'. A CAS LOOP "
            "turns that into arbitrary read-modify-write operations:\n"
            "      1. read the current value\n"
            "      2. compute the new value (pure function of the old)\n"
            "      3. CAS; if it failed, someone interfered -> go to 2 with the fresh value");

    constexpr int T = 4, N = 200000;
    std::atomic<int> maximum{0};
    std::vector<int> local_max(T, 0);
    std::vector<std::thread> ts;
    for (int t = 0; t < T; ++t)
        ts.emplace_back([&, t] {
            std::mt19937 rng(1234 + t);
            for (int i = 0; i < N; ++i) {
                const int v = static_cast<int>(rng() % 1'000'000);
                local_max[t] = std::max(local_max[t], v);
                update_max(maximum, v);
            }
        });
    for (auto& t : ts) t.join();
    const int expected = *std::max_element(local_max.begin(), local_max.end());
    ui::kv("shared max / true max", std::to_string(maximum.load()) + " / " + std::to_string(expected));
    ui::expect(maximum == expected, "no update lost: the result is the true maximum");

    ui::why("weak vs strong: compare_exchange_weak may fail SPURIOUSLY (return false even though the value "
            "matched) - on LL/SC CPUs (ARM, RISC-V) that lets the compiler emit a cheaper instruction "
            "sequence. In a retry LOOP a spurious failure just costs one more iteration, so use weak. "
            "Use strong when you do NOT loop and need a reliable answer.");
    ui::observe("The loop has a property no mutex has: if one thread is slow or pre-empted, the others "
                "still make progress (a CAS failure means SOMEONE ELSE succeeded). That is the definition "
                "of lock-free - 'system-wide progress' - not 'faster'.");
    return ui::finish();
}
