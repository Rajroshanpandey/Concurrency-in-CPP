// Stage 6 / TOUGH - Lock-free SPSC ring buffer: stress test + comparison with a mutex queue.
#include <lab/blocking_queue.hpp>
#include <lab/spsc_queue.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    const long long N = argc > 1 ? std::stoll(argv[1]) : 10'000'000;
    ui::banner(6, "TOUGH", "Lock-free SPSC ring buffer", "the ISR -> task queue of embedded systems");

    ui::say(
        "Open common/include/lab/spsc_queue.hpp. It has two indices: `tail` (written ONLY by the "
        "producer) and `head` (written ONLY by the consumer). Because each index has exactly one "
        "writer, no CAS and no lock are needed - only the right memory orders.");
    ui::say(
        "The four atomic operations and why each order is what it is:\n"
        "  (1) producer  tail.load(relaxed)  - reading my OWN variable; nobody else writes it.\n"
        "  (2) producer  head.load(acquire)  - pairs with (4): once I see the consumer freed a slot,\n"
        "                                      the consumer's READ of that slot is finished, so I may\n"
        "                                      overwrite it. Weaker => I could overwrite data still being read.\n"
        "  (3) producer  tail.store(release) - publishes buf[t] to the consumer. Weaker => consumer could\n"
        "                                      see the new tail but a STALE buffer slot.\n"
        "  (4) consumer  head.store(release) - releases the slot back to the producer (pairs with (2)).\n"
        "  (and the consumer's tail.load(acquire) pairs with (3).)");

    // ---- SPSC ----
    SpscQueue<std::int64_t, 1024> q;
    std::int64_t bad = 0, last = -1, received = 0;
    Stopwatch sw;
    std::thread consumer([&] {
        std::int64_t v;
        while (received < N) {
            if (q.pop(v)) {
                if (v != last + 1) ++bad;       // must arrive in order, no gaps, no duplicates
                last = v; ++received;
            } else std::this_thread::yield();
        }
    });
    std::thread producer([&] {
        for (std::int64_t i = 0; i < N; ++i)
            while (!q.push(i)) std::this_thread::yield();
    });
    producer.join();
    consumer.join();
    const double spsc_ms = sw.ms();

    // ---- mutex queue (for comparison, 1/10 of the items to keep it quick) ----
    const long long M = std::max<long long>(N / 10, 1);
    BlockingQueue<std::int64_t> bq(1024);
    sw.reset();
    std::thread bc([&] { while (bq.pop()) {} });
    std::thread bp([&] { for (std::int64_t i = 0; i < M; ++i) bq.push(i); bq.close(); });
    bp.join();
    bc.join();
    const double bq_ms = sw.ms();

    ui::section("Results");
    ui::kv("SPSC items / out-of-order", std::to_string(received) + " / " + std::to_string(bad));
    ui::kv("SPSC throughput", ui::num(static_cast<double>(N) / spsc_ms / 1000.0) + " M items/s");
    ui::kv("mutex+CV queue throughput", ui::num(static_cast<double>(M) / bq_ms / 1000.0) + " M items/s");
    ui::expect(received == N && bad == 0, "all items arrived in order");

    ui::observe(
        "Zero out-of-order items, and the lock-free queue is typically 5-50x faster than the "
        "mutex+condition_variable one - no kernel call, no blocking. Now run s06_tough_spsc_broken "
        "(every order replaced by relaxed) under TSan and see what gets flagged.");
    ui::why(
        "alignas(64) on head_ and tail_ puts them on different cache lines. Otherwise producer and "
        "consumer, writing different variables, would still fight over ONE cache line and bounce it "
        "between cores: FALSE SHARING. Try removing alignas and benchmark on a multi-core machine.");
    ui::warn("SPSC means exactly ONE producer thread and ONE consumer thread. Two producers = broken. "
             "For multiple, use the MPMC queue in stage 10.");
    return ui::finish();
}
