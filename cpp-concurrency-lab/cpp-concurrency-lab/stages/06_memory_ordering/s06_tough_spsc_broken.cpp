// Stage 6 / TOUGH (broken twin) - SPSC queue with ALL orders relaxed. INTENTIONAL DATA RACE.
#include <lab/spsc_queue.hpp>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    const std::int64_t N = argc > 1 ? std::stoll(argv[1]) : 5'000'000;
    ui::banner(6, "TOUGH", "SPSC queue, all relaxed (BROKEN)", "let ThreadSanitizer show you the missing happens-before");

    ui::say("Same queue, template flag Broken=true: every acquire/release became relaxed. The "
            "consumer can now observe the new `tail` before the slot's contents - or the producer "
            "can overwrite a slot the consumer is still reading.");
    ui::warn("On x86 this will most likely 'work' (TSO hardware). Under -fsanitize=thread it is flagged "
             "immediately. On ARM it can really lose or corrupt data. Never trust a green run on x86.");

    SpscQueue<std::int64_t, 1024, /*Broken=*/true> q;
    std::int64_t bad = 0, last = -1, received = 0;
    std::thread consumer([&] {
        std::int64_t v;
        while (received < N) {
            if (q.pop(v)) { if (v != last + 1) ++bad; last = v; ++received; }
            else std::this_thread::yield();
        }
    });
    std::thread producer([&] { for (std::int64_t i = 0; i < N; ++i) while (!q.push(i)) std::this_thread::yield(); });
    producer.join();
    consumer.join();
    ui::kv("out-of-order items seen here", std::to_string(bad));
    ui::observe("0 on your machine proves nothing. Run:  cmake --preset tsan && cmake --build --preset tsan "
                "--target s06_tough_spsc_broken && ./build/tsan/bin/s06_tough_spsc_broken");
    return ui::finish();
}
