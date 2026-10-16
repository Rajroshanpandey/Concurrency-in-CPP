// Stage 9 / TOUGH - The thread pool rebuilt on jthread + stop_token; cancel a long job with stop_callback.
#include <atomic>
#include <chrono>
#include <lab/jthread_pool.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <stop_token>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(9, "TOUGH", "jthread thread pool + stop_callback", "shutdown with fewer ways to go wrong");

    ui::say("Compare the two shutdown paths line by line:\n"
            "  stage 8 ThreadPool                      stage 9 JThreadPool\n"
            "  -------------------------------------   ---------------------------------\n"
            "  bool stopping_;                         (nothing)\n"
            "  ~Pool() { lock; stopping_=true;         ~Pool() = default;\n"
            "            unlock; notify_all();         // jthread: request_stop() + join()\n"
            "            for (t : workers) t.join(); }\n"
            "  predicate: stopping_ || !jobs_.empty()  cv.wait(lk, st, !jobs_.empty())\n"
            "Four things to remember in the old version, zero in the new one. Member order matters though: "
            "`workers_` is declared LAST so it is destroyed FIRST (joined while the queue still exists).");

    ui::section("1) Normal use + drain on destruction");
    std::atomic<int> done{0};
    {
        JThreadPool pool(3);
        std::vector<std::future<int>> fs;
        for (int i = 0; i < 20; ++i) fs.push_back(pool.submit([i, &done] { ++done; return i * i; }));
        long long sum = 0;
        for (auto& f : fs) sum += f.get();
        ui::kv("sum of squares 0..19", std::to_string(sum));
        ui::expect(sum == 2470, "results arrive through futures");
        for (int i = 0; i < 30; ++i) pool.submit([&done] { std::this_thread::sleep_for(1ms); ++done; });
    }   // destructor: queued jobs finish, workers stop, no explicit join anywhere
    ui::expect(done == 50, "all 50 jobs completed by the time the pool was destroyed");

    ui::section("2) Cancelling a long-running job mid-way");
    ui::say("A job that takes a std::stop_token can be interrupted by pool shutdown. We also register a "
            "std::stop_callback: code that runs IMMEDIATELY, on the requesting thread, when stop is "
            "requested - ideal for poking a blocked hardware call or closing a socket.");
    std::atomic<int> steps{0};
    std::atomic<bool> callback_ran{false};
    Stopwatch sw;
    {
        JThreadPool pool(1);
        pool.submit([&](std::stop_token st) {
            std::stop_callback cb(st, [&] { callback_ran = true; });   // "abort the hardware transfer"
            for (int i = 0; i < 100000 && !st.stop_requested(); ++i) {   // would take ~100 s
                std::this_thread::sleep_for(1ms);
                ++steps;
            }
        });
        std::this_thread::sleep_for(50ms);
    }   // destructor requests stop -> job notices -> returns -> join
    ui::kv("steps completed before cancellation", std::to_string(steps.load()) + " of 100000");
    ui::kv("whole block took", ui::num(sw.ms(), 0) + " ms (not 100 s)");
    ui::expect(steps.load() < 100000 && sw.ms() < 2000, "long job was cancelled cooperatively");
    ui::expect(callback_ran.load(), "stop_callback fired on request_stop()");

    ui::observe("Reflect: which shutdown code has fewer ways to go wrong? Old: forgetting notify_all, "
                "forgetting join, joining under the lock, reading stopping_ without the mutex. New: "
                "declaring members in the wrong order. Fewer, and that one is checkable by a review rule.");
    ui::warn("A stop request is a REQUEST. A job that never checks its token cannot be cancelled and will "
             "block the destructor. Long loops and blocking calls must be written to cooperate.");
    return ui::finish();
}
