// Stage 10 / TOUGH (final boss) - Vyukov's bounded MPMC queue vs. the mutex queue. Be honest about who wins.
#include <atomic>
#include <lab/blocking_queue.hpp>
#include <lab/mpmc_queue.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

struct Result { double ms; long long sum; long long count; };

template <class PushFn, class PopFn>
static Result run(int P, int C, int per_producer, PushFn push, PopFn pop_one) {
    std::atomic<long long> sum{0}, count{0};
    std::atomic<int> producers_done{0};
    lab::Stopwatch sw;
    std::vector<std::thread> ts;
    for (int p = 0; p < P; ++p)
        ts.emplace_back([&] {
            for (int i = 1; i <= per_producer; ++i) push(i);
            ++producers_done;
        });
    for (int c = 0; c < C; ++c)
        ts.emplace_back([&] {
            long long s = 0, n = 0;
            int v;
            for (;;) {
                const bool all_produced = producers_done == P;   // read BEFORE trying to pop ...
                if (pop_one(v)) { s += v; ++n; continue; }
                if (all_produced) break;                         // ... so 'empty' now really means 'finished'
                std::this_thread::yield();
            }
            sum += s; count += n;
        });
    for (auto& t : ts) t.join();
    return {sw.ms(), sum.load(), count.load()};
}

int main(int argc, char** argv) {
    using namespace lab;
    const int per = argc > 1 ? std::stoi(argv[1]) : 500000;
    constexpr int P = 2, C = 2;
    ui::banner(10, "TOUGH", "Bounded MPMC queue (Vyukov)", "lock-free vs mutex: measure, do not assume");

    ui::say("Read common/include/lab/mpmc_queue.hpp. Each cell has a sequence number that says whose turn "
            "it is: 'seq == pos' -> free for the producer holding ticket pos; 'seq == pos+1' -> full for the "
            "consumer holding ticket pos. The ONLY contended operation is a CAS on the enqueue/dequeue "
            "ticket counter; data is handed over through the cell's sequence number with release/acquire.");
    ui::kv("producers x consumers x items", std::to_string(P) + " x " + std::to_string(C) + " x " + std::to_string(per) + " each");

    const long long exp_cnt = static_cast<long long>(P) * per;
    const long long exp_sum = static_cast<long long>(P) * per * (per + 1LL) / 2;

    MpmcQueue<int> lf(1024);
    auto r1 = run(P, C, per, [&](int v) { while (!lf.push(v)) std::this_thread::yield(); }, [&](int& v) { return lf.pop(v); });

    BlockingQueue<int> bq(1024);
    // the blocking queue signals the end via close(); adapt it to the same harness using pop_for + try semantics
    std::atomic<int> pdone{0};
    Stopwatch sw;
    std::atomic<long long> s2{0}, c2{0};
    std::vector<std::thread> ts;
    for (int p = 0; p < P; ++p) ts.emplace_back([&] { for (int i = 1; i <= per; ++i) bq.push(i); if (++pdone == P) bq.close(); });
    for (int c = 0; c < C; ++c) ts.emplace_back([&] { long long s = 0, n = 0; while (auto v = bq.pop()) { s += *v; ++n; } s2 += s; c2 += n; });
    for (auto& t : ts) t.join();
    Result r2{sw.ms(), s2.load(), c2.load()};

    ui::section("Results");
    ui::kv("lock-free MPMC", ui::num(r1.ms) + " ms   " + ui::num(exp_cnt / r1.ms / 1000.0) + " M items/s");
    ui::kv("mutex + condition_variable", ui::num(r2.ms) + " ms   " + ui::num(exp_cnt / r2.ms / 1000.0) + " M items/s");
    ui::expect(r1.count == exp_cnt && r1.sum == exp_sum, "lock-free queue: nothing lost or duplicated");
    ui::expect(r2.count == exp_cnt && r2.sum == exp_sum, "mutex queue: nothing lost or duplicated");

    ui::observe("Write down honestly who won on YOUR machine. On many-core hardware the lock-free queue "
                "usually wins clearly; on a 1-core container it may be slower or similar, because yield-spinning "
                "wastes the only core while a blocked mutex waiter would sleep. Lock-free does not mean faster; "
                "it means 'no thread can block the others' - which matters most for LATENCY GUARANTEES "
                "(ISR-adjacent code, real-time threads), not necessarily raw throughput.");
    ui::note("Also notice what the lock-free version cannot do: it has no blocking pop and no close(). "
             "Building waiting/shutdown on top of it brings back sleeping, which is why many systems use "
             "lock-free queues PLUS an eventfd/semaphore for wake-ups.");
    return ui::finish();
}
