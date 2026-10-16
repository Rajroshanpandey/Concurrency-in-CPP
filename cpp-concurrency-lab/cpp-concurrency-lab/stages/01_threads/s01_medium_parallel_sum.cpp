// Stage 1 / MEDIUM - Parallel sum: each thread writes ONLY to its own slot (no sharing, no locks).
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <numeric>
#include <thread>
#include <vector>

// Worker: sums v[begin,end) and writes the result to ITS OWN output variable.
// Note the reference parameters: std::thread copies its arguments, so we must wrap
// them in std::cref / std::ref to say "I really mean a reference".
static void sum_range(const std::vector<int>& v, std::size_t begin, std::size_t end, long long& out) {
    long long local = 0;  // accumulate in a register-friendly local, write ONCE at the end
    for (std::size_t i = begin; i < end; ++i) local += v[i];
    out = local;
}

static long long parallel_sum(const std::vector<int>& v, unsigned n_threads) {
    std::vector<long long> partial(n_threads, 0);
    std::vector<std::thread> ts;
    const std::size_t chunk = v.size() / n_threads;
    for (unsigned i = 0; i < n_threads; ++i) {
        const std::size_t b = i * chunk;
        const std::size_t e = (i + 1 == n_threads) ? v.size() : b + chunk;
        ts.emplace_back(sum_range, std::cref(v), b, e, std::ref(partial[i]));
    }
    for (auto& t : ts) t.join();  // join BEFORE reading partial[] - join is also a synchronisation point
    return std::accumulate(partial.begin(), partial.end(), 0LL);
}

int main(int argc, char** argv) {
    using namespace lab;
    const std::size_t n = argc > 1 ? std::stoull(argv[1]) : 40'000'000;
    ui::banner(1, "MEDIUM", "Parallel sum", "std::ref, partitioning work, and why more threads != faster");

    ui::say(
        "We sum " + std::to_string(n) + " integers with 1, 2, 4 ... threads. The data is split into "
        "equal chunks. Thread i writes its result to partial[i]: nobody writes to the same memory, "
        "so no mutex is needed. That is the cheapest form of thread safety - sharing nothing.");

    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 1);
    const long long expected = static_cast<long long>(n) * (static_cast<long long>(n) + 1) / 2;
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    ui::kv("hardware threads reported", std::to_string(hw));

    bool ok = true;
    double t1 = 0;
    for (unsigned threads : {1u, 2u, 4u, 8u}) {
        Stopwatch sw;
        const long long s = parallel_sum(v, threads);
        const double ms = sw.ms();
        if (threads == 1) t1 = ms;
        ok = ok && (s == expected);
        ui::kv(std::to_string(threads) + " thread(s)", ui::num(ms) + " ms   speed-up x" + ui::num(t1 / ms) +
                                                           (s == expected ? "" : "   WRONG SUM"));
    }

    ui::observe(
        "Speed-up is bounded by the number of real cores (" + std::to_string(hw) + " here) AND by "
        "memory bandwidth - summing is so cheap that RAM, not the CPU, is the bottleneck. Past the "
        "core count you only add scheduling overhead. If the work per element were heavy, you would "
        "see near-linear scaling.");
    ui::why(
        "Slots partial[0], partial[1]... sit next to each other in memory. If every iteration wrote "
        "directly to them, the cores would fight over one cache line ('false sharing', stage 6). "
        "That is why each worker accumulates in a local variable and writes once.");
    ui::expect(ok, "every thread count produced the exact expected sum");
    return ui::finish();
}
