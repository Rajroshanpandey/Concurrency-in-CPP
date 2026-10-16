// Stage 8 / EASY - One worker thread consuming jobs from a BlockingQueue: the seed of every thread pool.
#include <functional>
#include <lab/blocking_queue.hpp>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

int main() {
    using namespace lab;
    ui::banner(8, "EASY", "A worker thread and a job queue", "std::function<void()> is a 'job'");

    ui::say("A job is just a callable with no arguments: std::function<void()>. The producer (main) "
            "puts jobs in the queue, ONE worker takes them out and runs them in order. Jobs run on the "
            "worker's thread, not on the thread that submitted them.");
    ui::code(
        "worker:  while (auto job = queue.pop()) (*job)();     // pop() == nullopt when closed+drained\n"
        "main:    queue.push([]{ ... });  ...  queue.close();  worker.join();");

    BlockingQueue<std::function<void()>> jobs(16);
    std::vector<std::string> log;
    std::thread::id worker_id;
    std::thread worker([&] {
        worker_id = std::this_thread::get_id();
        while (auto job = jobs.pop()) (*job)();
    });
    for (int i = 1; i <= 5; ++i)
        jobs.push([i, &log, &worker_id] {
            log.push_back("job " + std::to_string(i) + (std::this_thread::get_id() == worker_id ? " ran on the worker" : " ran elsewhere?!"));
        });
    jobs.close();
    worker.join();

    for (const auto& l : log) ui::println("      " + l);
    ui::expect(log.size() == 5, "all five jobs executed");
    ui::expect(log.front().find("worker") != std::string::npos, "jobs ran on the worker thread");
    ui::observe("`log` is touched only by the worker thread while it runs, then read by main AFTER join() - "
                "join() is the synchronisation that makes that safe. One worker means jobs never run in "
                "parallel with each other: this gives you ordering AND mutual exclusion for free "
                "(the 'actor' / 'active object' pattern).");
    return ui::finish();
}
