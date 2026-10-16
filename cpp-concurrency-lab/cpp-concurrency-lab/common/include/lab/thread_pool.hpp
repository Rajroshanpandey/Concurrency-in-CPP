#pragma once
// lab/thread_pool.hpp - classic fixed-size thread pool: mutex + condition_variable + queue,
// submit() returns a std::future, exceptions travel through the future (stage 8).
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace lab {

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stopping_ = false;

    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lk(m_);
                cv_.wait(lk, [&] { return stopping_ || !jobs_.empty(); });
                if (jobs_.empty()) return;  // stopping_ && drained
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job();  // packaged_task catches exceptions and stores them in the future
        }
    }

public:
    explicit ThreadPool(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) workers_.emplace_back([this] { worker_loop(); });
    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool() { shutdown(); }

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        // packaged_task is move-only, std::function needs copyable -> hold it in a shared_ptr.
        auto task = std::make_shared<std::packaged_task<R()>>(
            [fn = std::forward<F>(f), ... as = std::forward<Args>(args)]() mutable -> R {
                return std::invoke(std::move(fn), std::move(as)...);
            });
        auto fut = task->get_future();
        {
            std::lock_guard lk(m_);
            if (stopping_) throw std::runtime_error("ThreadPool: submit() after shutdown");
            jobs_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    // Finishes every queued job, then joins. Idempotent.
    void shutdown() {
        {
            std::lock_guard lk(m_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    std::size_t size() const { return workers_.size(); }
};

}  // namespace lab
