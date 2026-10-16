// Stage 9 / MEDIUM - Shutting down a thread that is BLOCKED: condition_variable_any::wait(lock, stop_token, pred).
#include <chrono>
#include <condition_variable>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(9, "MEDIUM", "Waking a blocked thread with a stop_token", "condition_variable_any + stop_token");

    ui::say("The classic problem: a worker sleeps in cv.wait() waiting for work. Nobody ever sends work. "
            "How do you shut it down? With a bool flag you must remember to notify; forget it, or race "
            "between 'check flag' and 'go to sleep', and the join() hangs forever.");
    ui::say("condition_variable_any::wait(lock, stop_token, pred) registers a callback with the token: "
            "request_stop() itself wakes the waiter. No flag, no notify, no race.");
    ui::code(
        "cv.wait(lock, st, [&]{ return !queue.empty(); });   // returns the predicate's value\n"
        "// false => woken by the stop request, queue still empty -> leave the loop");

    std::mutex m;
    std::condition_variable_any cv;
    std::queue<int> q;
    std::atomic<int> processed{0};
    std::atomic<bool> exited_cleanly{false};
    std::atomic<bool> is_waiting{false};

    std::jthread worker([&](std::stop_token st) {
        for (;;) {
            std::unique_lock lk(m);
            is_waiting = true;
            if (!cv.wait(lk, st, [&] { return !q.empty(); })) break;   // stop requested while idle
            q.pop();
            ++processed;
        }
        exited_cleanly = true;
    });

    for (int i = 0; i < 3; ++i) { { std::lock_guard lk(m); q.push(i); } cv.notify_one(); }
    std::this_thread::sleep_for(100ms);    // let it process and go idle: now it is BLOCKED in wait()
    ui::kv("jobs processed so far", std::to_string(processed.load()));

    Stopwatch sw;
    worker.request_stop();                 // note: no notify!
    worker.join();
    ui::kv("time from request_stop() to join() done", ui::num(sw.ms(), 2) + " ms");
    ui::expect(processed == 3, "all queued items processed before the stop");
    ui::expect(exited_cleanly.load(), "blocked worker woke up by itself and exited");
    ui::expect(sw.ms() < 50, "shutdown latency is milliseconds, not 'never'");

    ui::observe("Compare with std::condition_variable: it has no stop_token overload, so you must keep a "
                "bool, set it under the mutex, and call notify_all() - and every new waiter you add is another "
                "place to forget. With condition_variable_any the token carries the wake-up for you.");
    ui::note("condition_variable_any is slightly heavier than condition_variable (it works with any lockable "
             "type). For hot paths measure; for shutdown logic the convenience wins.");
    return ui::finish();
}
