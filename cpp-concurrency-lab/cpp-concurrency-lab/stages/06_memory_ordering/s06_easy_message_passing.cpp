// Stage 6 / EASY - Message passing with release/acquire: the handshake that makes data visible.
#include <atomic>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    const int rounds = argc > 1 ? std::stoi(argv[1]) : 200000;
    ui::banner(6, "EASY", "release / acquire handshake", "atomicity is not the same as VISIBILITY");

    ui::say(
        "The producer writes a plain int `data`, then raises an atomic flag. The consumer waits for "
        "the flag, then reads `data`. `data` is NOT atomic - it is protected purely by the flag's "
        "ordering. We repeat this " + std::to_string(rounds) + " times and check every message.");
    ui::code(
        "producer:  data = round;                        // (A) ordinary write\n"
        "           seq.store(round, memory_order_release);  // (B) 'everything before me is done'\n"
        "consumer:  while (seq.load(memory_order_acquire) != round) {}   // (C) 'I saw (B)'\n"
        "           assert(data == round);               // (D) guaranteed: A happens-before D");

    int data = 0;                       // plain, non-atomic
    std::atomic<int> seq{0};            // producer publishes round numbers
    std::atomic<int> ack{0};            // consumer acknowledges, so the producer does not overwrite early
    long long wrong = 0;

    std::thread consumer([&] {
        for (int r = 1; r <= rounds; ++r) {
            while (seq.load(std::memory_order_acquire) != r) std::this_thread::yield();
            if (data != r) ++wrong;     // would be a data race WITHOUT the acquire/release pairing
            ack.store(r, std::memory_order_release);
        }
    });
    std::thread producer([&] {
        for (int r = 1; r <= rounds; ++r) {
            data = r;
            seq.store(r, std::memory_order_release);
            while (ack.load(std::memory_order_acquire) != r) std::this_thread::yield();
        }
    });
    producer.join();
    consumer.join();

    ui::kv("messages checked", std::to_string(rounds));
    ui::kv("wrong values seen", std::to_string(wrong));
    ui::expect(wrong == 0, "every message arrived intact");

    ui::observe(
        "Zero wrong values - and, importantly, a clean ThreadSanitizer run (try the tsan preset). "
        "Now run s06_easy_relaxed_broken: it differs by ONE word (relaxed) and TSan reports a data "
        "race on `data`.");
    ui::why(
        "release/acquire is a HANDSHAKE between exactly two operations on the same atomic: when an "
        "acquire load reads the value written by a release store, every write that happened before "
        "the release is guaranteed visible after the acquire ('synchronizes-with'). It orders "
        "the OTHER variables around the atomic - that is the whole point.");
    return ui::finish();
}
