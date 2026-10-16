// Stage 8 / TOUGH - 100,000 tiny jobs: one-thread-per-job vs a thread pool.
#include <atomic>
#include <lab/thread_pool.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    using namespace lab;
    const int jobs = argc > 1 ? std::stoi(argv[1]) : 100000;
    ui::banner(8, "TOUGH", "Pool vs thread-per-job", "what does creating a thread really cost?");
    ui::say("Each job just does ++counter (a few nanoseconds of work). So the benchmark measures the "
            "OVERHEAD of getting a job executed: " + std::to_string(jobs) + " jobs.");

    std::atomic<long long> counter{0};

    Stopwatch sw;
    {
        std::vector<std::thread> batch;
        for (int i = 0; i < jobs; ++i) {
            batch.emplace_back([&] { ++counter; });
            if (batch.size() == 64) { for (auto& t : batch) t.join(); batch.clear(); }   // bound live threads
        }
        for (auto& t : batch) t.join();
    }
    const double t_threads = sw.ms();
    ui::expect(counter == jobs, "thread-per-job executed every job");

    counter = 0;
    sw.reset();
    {
        ThreadPool pool(std::max(2u, std::thread::hardware_concurrency()));
        for (int i = 0; i < jobs; ++i) pool.submit([&] { ++counter; });
    }
    const double t_pool = sw.ms();
    ui::expect(counter == jobs, "pool executed every job");

    ui::kv("thread per job", ui::num(t_threads) + " ms   (" + ui::num(t_threads * 1000.0 / jobs, 1) + " us per job)");
    ui::kv("thread pool", ui::num(t_pool) + " ms   (" + ui::num(t_pool * 1000.0 / jobs, 1) + " us per job)");
    ui::kv("pool is faster by", "x" + ui::num(t_threads / t_pool, 1));

    ui::observe("Creating + joining a thread costs tens of microseconds (a syscall, a stack mapping, a "
                "scheduler entry). A pool pays that once, then a job costs one queue push/pop. "
                "Note the pool's per-job cost is still dominated by the mutex + condition variable: "
                "that is why high-throughput schedulers use lock-free queues (stage 10).");
    ui::why("On a microcontroller the argument is even stronger: thread stacks are statically sized RAM. "
            "A fixed set of tasks created at start-up is the norm in embedded design, not an optimisation.");
    return ui::finish();
}
