// Stage 6 / EASY (broken twin) - the same program with memory_order_relaxed. INTENTIONAL DATA RACE.
// Run under TSan:  ./build/tsan/bin/s06_easy_relaxed_broken   ->  "data race on `data`"
#include <atomic>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    const int rounds = argc > 1 ? std::stoi(argv[1]) : 200000;
    ui::banner(6, "EASY", "relaxed: the BROKEN twin", "same code, one word different, undefined behaviour");

    ui::say(
        "relaxed gives you atomicity of the flag itself and NOTHING else: no ordering of surrounding "
        "reads and writes. The consumer may see the flag raised and still read a stale `data`.");
    ui::warn("This program has a data race on purpose. On x86 it will almost always still print 0 wrong "
             "values (x86 hardware is strongly ordered) - and that is exactly the trap: it breaks on ARM "
             "(your embedded target!), or when the compiler reorders. ThreadSanitizer finds it on ANY CPU.");

    int data = 0;
    std::atomic<int> seq{0}, ack{0};
    long long wrong = 0;
    std::thread consumer([&] {
        for (int r = 1; r <= rounds; ++r) {
            while (seq.load(std::memory_order_relaxed) != r) std::this_thread::yield();   // <-- relaxed
            if (data != r) ++wrong;
            ack.store(r, std::memory_order_relaxed);
        }
    });
    std::thread producer([&] {
        for (int r = 1; r <= rounds; ++r) {
            data = r;
            seq.store(r, std::memory_order_relaxed);                                       // <-- relaxed
            while (ack.load(std::memory_order_relaxed) != r) std::this_thread::yield();
        }
    });
    producer.join();
    consumer.join();

    ui::kv("wrong values seen on THIS machine", std::to_string(wrong));
    ui::observe("'It printed 0 so it works' is the most dangerous sentence in concurrency. "
                "Correctness is defined by the C++ memory model, not by what your laptop does today.");
    return ui::finish();
}
