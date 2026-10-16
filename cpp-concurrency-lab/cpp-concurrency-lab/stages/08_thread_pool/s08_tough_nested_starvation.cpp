// Stage 8 / TOUGH - A job that waits for a job it submitted to the SAME pool: pool starvation deadlock.
#include <atomic>
#include <chrono>
#include <latch>
#include <lab/thread_pool.hpp>
#include <lab/ui.hpp>
#include <thread>

using namespace std::chrono_literals;

static int run_scenario(std::size_t workers, int outer_jobs) {
    lab::ThreadPool pool(workers);
    std::atomic<int> starved{0};
    std::latch all_running(outer_jobs);   // makes the demo deterministic: every outer job occupies a worker first
    std::vector<std::future<void>> outer;
    for (int i = 0; i < outer_jobs; ++i)
        outer.push_back(pool.submit([&] {
            all_running.arrive_and_wait();
            auto inner = pool.submit([] { return 1; });            // child job goes to the back of the SAME queue
            // naive code: inner.get();   -> blocks this worker. We use a timeout so the demo can't hang:
            if (inner.wait_for(500ms) == std::future_status::timeout) {
                ++starved;   // a real program would be stuck forever here.
                // Keep the worker occupied a moment longer so the OTHER blocked worker also times out
                // before we free ours (otherwise the first to give up rescues its sibling).
                std::this_thread::sleep_for(150ms);
                // Returning now frees this worker, so the queued child can finally run.
            }
        }));
    for (auto& f : outer) f.get();
    return starved.load();
}

int main() {
    using namespace lab;
    ui::banner(8, "TOUGH", "Pool starvation deadlock", "a worker blocks waiting for work only another worker could run");

    ui::say("Each OUTER job submits an INNER job to the same pool and waits for its result.\n"
            "      pool of 2 workers, 2 outer jobs:\n"
            "        worker 1 runs outer#1 -> waits for inner#1 (queued)\n"
            "        worker 2 runs outer#2 -> waits for inner#2 (queued)\n"
            "        nobody is free to run the inner jobs -> deadlock\n"
            "No mutex is involved, so no lock-order tool will warn you. It is a RESOURCE deadlock: the "
            "resource is 'a free worker'.");

    ui::section("2 workers, 2 outer jobs (every worker blocked)");
    const int s1 = run_scenario(2, 2);
    ui::kv("outer jobs that starved", std::to_string(s1) + " of 2");
    ui::expect(s1 == 2, "both outer jobs starved: deadlock reproduced (rescued only by our timeout)");

    ui::section("4 workers, 2 outer jobs (a spare worker exists)");
    const int s2 = run_scenario(4, 2);
    ui::kv("outer jobs that starved", std::to_string(s2) + " of 2");
    ui::expect(s2 == 0, "with spare workers the inner jobs run immediately");

    ui::observe("'Make the pool bigger' hides the bug: with enough simultaneous outer jobs it comes back. "
                "The pool size must exceed the maximum number of jobs that can be blocked at once - which is "
                "unbounded for recursive algorithms.");
    ui::why(
        "Real fixes: (1) never block a worker on another job - use continuations (when the inner job finishes, "
        "THEN submit the next step); (2) 'help while waiting': the waiting worker pops and runs queued jobs "
        "itself; (3) give blocking jobs their own pool; (4) use a work-stealing scheduler. "
        "Your merge sort in stage 7 used std::async precisely to avoid this problem.");
    return ui::finish();
}
