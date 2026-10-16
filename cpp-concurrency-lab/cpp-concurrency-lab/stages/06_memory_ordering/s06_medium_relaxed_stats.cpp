// Stage 6 / MEDIUM - Using memory_order_relaxed CORRECTLY: a statistics counter.
#include <atomic>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

int main() {
    using namespace lab;
    ui::banner(6, "MEDIUM", "relaxed done right", "when you need atomicity but not ordering");

    constexpr int T = 4, N = 1'000'000;
    ui::say(
        "Four threads count events into one shared counter. Nobody reads the counter while they "
        "work, and nobody uses it to decide whether OTHER data is ready. The only requirement is "
        "'no increment lost'. That is atomicity without ordering, which is exactly relaxed.");
    ui::code(
        "events.fetch_add(1, std::memory_order_relaxed);   // in the workers\n"
        "t.join();                                         // join = the synchronisation point\n"
        "events.load(std::memory_order_relaxed);           // after join: safe, everything is visible");

    auto run = [&](std::memory_order mo) {
        std::atomic<long long> events{0};
        Stopwatch sw;
        std::vector<std::thread> ts;
        for (int t = 0; t < T; ++t) ts.emplace_back([&] { for (int i = 0; i < N; ++i) events.fetch_add(1, mo); });
        for (auto& t : ts) t.join();
        return std::pair{events.load(std::memory_order_relaxed), sw.ms()};
    };
    auto [c_relaxed, t_relaxed] = run(std::memory_order_relaxed);
    auto [c_seq, t_seq] = run(std::memory_order_seq_cst);

    ui::kv("relaxed  fetch_add", std::to_string(c_relaxed) + "   " + ui::num(t_relaxed) + " ms");
    ui::kv("seq_cst  fetch_add", std::to_string(c_seq) + "   " + ui::num(t_seq) + " ms");
    ui::expect(c_relaxed == static_cast<long long>(T) * N, "relaxed counter exact");
    ui::expect(c_seq == static_cast<long long>(T) * N, "seq_cst counter exact");

    ui::observe(
        "Both are exact. On x86 the timings are nearly identical (a LOCK-prefixed add is already "
        "sequentially consistent). On ARM / RISC-V the relaxed version can be noticeably cheaper "
        "because no barrier instruction is emitted.");
    ui::why(
        "ONE-SENTENCE ANSWER: relaxed is safe here because this counter is never used to publish or "
        "guard any other data - while the flag in s06_easy_* publishes `data`, so it needs release/"
        "acquire.");
    ui::note("Default to seq_cst. Reach for acquire/release when you understand the handshake. Use "
             "relaxed only for isolated counters/statistics, and write down WHY in a comment.");
    return ui::finish();
}
