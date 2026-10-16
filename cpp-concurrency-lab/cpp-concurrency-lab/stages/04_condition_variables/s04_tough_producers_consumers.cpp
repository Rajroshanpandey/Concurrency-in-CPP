// Stage 4 / TOUGH - 3 producers, 3 consumers: no loss, no duplicates, clean shutdown, no hang.
#include <atomic>
#include <lab/blocking_queue.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    using namespace lab;
    const int total = argc > 1 ? std::stoi(argv[1]) : 100000;
    constexpr int P = 3, C = 3;
    ui::banner(4, "TOUGH", "3 producers x 3 consumers", "checksums prove nothing was lost or duplicated");

    ui::say(
        "Producers push the unique numbers 1.." + std::to_string(total) + " (each producer takes a "
        "third). Three consumers pop until the queue is closed AND empty. Afterwards we check three "
        "independent invariants: count, sum, and 'every number seen exactly once'.");
    ui::code(
        "producers --push--> [ bounded queue, cap 64 ] --pop--> consumers\n"
        "shutdown order:  join producers  ->  q.close()  ->  join consumers");

    BlockingQueue<int> q(64);
    std::vector<std::atomic<std::uint8_t>> seen(static_cast<std::size_t>(total) + 1);
    for (auto& s : seen) s = 0;
    std::atomic<long long> consumed_sum{0}, consumed_count{0};

    Stopwatch sw;
    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < P; ++p)
        producers.emplace_back([&, p] {
            for (int v = p + 1; v <= total; v += P) q.push(v);
        });
    for (int c = 0; c < C; ++c)
        consumers.emplace_back([&] {
            long long local_sum = 0, local_cnt = 0;
            while (auto v = q.pop()) {
                seen[static_cast<std::size_t>(*v)].fetch_add(1);
                local_sum += *v; ++local_cnt;
            }
            consumed_sum += local_sum;
            consumed_count += local_cnt;
        });

    for (auto& t : producers) t.join();   // 1) all data is in the queue
    q.close();                            // 2) tell consumers "no more data"
    for (auto& t : consumers) t.join();   // 3) they drain, see nullopt, exit
    const double ms = sw.ms();

    const long long expected_sum = static_cast<long long>(total) * (total + 1) / 2;
    int missing = 0, dup = 0;
    for (int v = 1; v <= total; ++v) {
        const int n = seen[static_cast<std::size_t>(v)].load();
        if (n == 0) ++missing;
        if (n > 1) ++dup;
    }
    ui::kv("items", std::to_string(total));
    ui::kv("consumed count / sum", std::to_string(consumed_count.load()) + " / " + std::to_string(consumed_sum.load()));
    ui::kv("expected sum", std::to_string(expected_sum));
    ui::kv("missing / duplicated", std::to_string(missing) + " / " + std::to_string(dup));
    ui::kv("elapsed", ui::num(ms) + " ms   (" + ui::num(total / ms / 1000.0 * 1000.0, 0) + " k items/s)");

    ui::expect(consumed_count == total, "count matches");
    ui::expect(consumed_sum == expected_sum, "sum matches");
    ui::expect(missing == 0 && dup == 0, "every number seen exactly once");
    ui::observe(
        "If you swap steps 1 and 2 (close before the producers finish) push() starts returning "
        "false and data is lost - try it. If you forget step 2 entirely, the consumers wait forever: "
        "the program HANGS. 'Who tells the consumers to stop?' is the first question to ask of any "
        "producer/consumer design.");
    return ui::finish();
}
