// Stage 8 / MEDIUM - N workers, submit(), and a destructor that finishes queued jobs before joining.
#include <atomic>
#include <chrono>
#include <lab/thread_pool.hpp>
#include <lab/ui.hpp>
#include <map>
#include <mutex>
#include <set>
#include <thread>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(8, "MEDIUM", "A fixed-size thread pool", "N workers share one queue");

    ui::say("Open common/include/lab/thread_pool.hpp. Workers all wait on ONE condition variable. "
            "submit() pushes a job and notifies ONE worker. The destructor sets `stopping_`, wakes "
            "everybody, and each worker exits only when stopping AND the queue is empty - so queued "
            "jobs are never dropped.");

    std::mutex m;
    std::map<std::thread::id, int> per_worker;
    std::atomic<int> done{0};
    {
        ThreadPool pool(4);
        for (int i = 0; i < 40; ++i)
            pool.submit([&] {
                std::this_thread::sleep_for(5ms);
                { std::lock_guard lk(m); ++per_worker[std::this_thread::get_id()]; }
                ++done;
            });
        ui::say("40 jobs submitted; leaving the scope now. The destructor must finish ALL of them.");
    }   // ~ThreadPool(): drain + join
    ui::kv("jobs completed after destructor", std::to_string(done.load()) + " / 40");
    ui::kv("distinct worker threads used", std::to_string(per_worker.size()));
    for (const auto& [id, n] : per_worker) { (void)id; ui::println("      worker handled " + std::to_string(n) + " jobs"); }
    ui::expect(done == 40, "destructor drained the queue before joining");
    ui::expect(per_worker.size() >= 1 && per_worker.size() <= 4, "no more than 4 threads ever existed");

    ui::observe("Forty jobs, four threads, spread roughly evenly - and 40 jobs never created 40 threads. "
                "That is the entire point of a pool: thread creation cost and thread COUNT are fixed.");
    ui::why("Why `if (jobs_.empty()) return;` AFTER the wait? The predicate wakes a worker when stopping "
            "OR a job exists. After waking, an empty queue means 'stopping and nothing left' - the only "
            "state in which leaving is correct. Swap the order and you either drop jobs or hang.");
    return ui::finish();
}
