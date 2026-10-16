// Stage 5 / TOUGH - Atomics do not compose: torn pairs, fixed with a mutex and with a seqlock.
#include <atomic>
#include <lab/seqlock.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>

// Invariant the writer maintains in EVERY published state: pressure == temperature * 2.
struct Pair { std::int64_t temperature; std::int64_t pressure; };

struct TwoAtomics {
    std::atomic<std::int64_t> t{0}, p{0};
    void write(std::int64_t i) { t.store(i); p.store(i * 2); }               // two separate atomic ops
    Pair read() const { return {t.load(), p.load()}; }
};
struct MutexPair {
    mutable std::mutex m; Pair v{0, 0};
    void write(std::int64_t i) { std::lock_guard lk(m); v = {i, i * 2}; }
    Pair read() const { std::lock_guard lk(m); return v; }
};
struct SeqPair {
    lab::SeqLock<Pair> s;
    void write(std::int64_t i) { s.store({i, i * 2}); }
    Pair read() const { return s.load(); }
};

template <class Shared>
static long long count_torn(long long writes, double& writer_ms) {
    Shared sh;
    std::atomic<bool> done{false};
    long long torn = 0, reads = 0;
    std::thread reader([&] {
        while (!done.load(std::memory_order_acquire)) {
            Pair p = sh.read();
            ++reads;
            if (p.pressure != p.temperature * 2) ++torn;
        }
    });
    lab::Stopwatch sw;
    for (long long i = 1; i <= writes; ++i) {
        sh.write(i);
        if ((i & 1023) == 0) std::this_thread::yield();   // give the reader CPU on 1-core boxes
    }
    writer_ms = sw.ms();
    done.store(true, std::memory_order_release);
    reader.join();
    return torn;
}

int main(int argc, char** argv) {
    using namespace lab;
    const long long writes = argc > 1 ? std::stoll(argv[1]) : 1'000'000;
    ui::banner(5, "TOUGH", "Atomics don't compose: the torn pair", "mutex vs seqlock for multi-field state");

    ui::say(
        "The writer publishes (temperature = i, pressure = 2*i). A reader must NEVER see a pair that "
        "breaks pressure == 2*temperature. Each field is individually atomic - so is the pair safe?\n"
        "      writer: t=5 ......... t=6 ... p=12 ...... (p=10 still stored)\n"
        "      reader:      reads t=6, p=10   -> an inconsistent pair that never existed");

    double w1, w2, w3;
    const long long torn_atomic = count_torn<TwoAtomics>(writes, w1);
    const long long torn_mutex = count_torn<MutexPair>(writes, w2);
    const long long torn_seq = count_torn<SeqPair>(writes, w3);

    ui::section("Torn reads out of " + std::to_string(writes) + " writes");
    ui::kv("two std::atomic fields", std::to_string(torn_atomic) + "      (writer " + ui::num(w1, 0) + " ms)");
    ui::kv("mutex around the struct", std::to_string(torn_mutex) + "      (writer " + ui::num(w2, 0) + " ms)");
    ui::kv("seqlock", std::to_string(torn_seq) + "      (writer " + ui::num(w3, 0) + " ms)");

    ui::expect(torn_mutex == 0, "mutex: never torn");
    ui::expect(torn_seq == 0, "seqlock: never torn");
    ui::observe(
        "The two-atomics version tears (on a 1-core box you may need more writes or a re-run; on "
        "multi-core it tears constantly). The fix must make the PAIR the unit of atomicity.");
    ui::why(
        "Mutex: simple, but a reader can block the writer (priority inversion in an RTOS!). "
        "Seqlock: the writer NEVER waits. It bumps a version counter to odd, writes, bumps it to "
        "even. A reader copies the data and re-checks the version; if it changed (or was odd) it "
        "retries. Perfect for 'one high-priority writer, many readers, small struct' - e.g. a "
        "sensor snapshot consumed by a control loop. Cost: readers may spin under heavy writing, "
        "and T must be trivially copyable.");
    return ui::finish();
}
