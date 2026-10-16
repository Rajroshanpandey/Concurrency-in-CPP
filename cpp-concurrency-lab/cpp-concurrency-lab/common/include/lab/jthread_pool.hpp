#pragma once
// lab/jthread_pool.hpp - the same pool rebuilt on std::jthread + stop_token + condition_variable_any.
// Note what disappeared: no `stopping_` flag, no explicit join, no manual notify in the destructor.
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace lab {

namespace detail {
template <class F, bool = std::is_invocable_v<F&, std::stop_token>>
struct job_result { using type = std::invoke_result_t<F&, std::stop_token>; };
template <class F>
struct job_result<F, false> { using type = std::invoke_result_t<F&>; };
}  // namespace detail

class JThreadPool {
    std::mutex m_;
    std::condition_variable_any cv_;
    std::deque<std::function<void(std::stop_token)>> jobs_;
    std::vector<std::jthread> workers_;  // declared LAST => destroyed FIRST (joins before the rest dies)

    void loop(std::stop_token st) {
        for (;;) {
            std::function<void(std::stop_token)> job;
            {
                std::unique_lock lk(m_);
                // Returns pred() on stop: queued jobs are still drained, then we leave.
                if (!cv_.wait(lk, st, [&] { return !jobs_.empty(); })) return;
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            job(st);
        }
    }

public:
    explicit JThreadPool(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) workers_.emplace_back([this](std::stop_token st) { loop(st); });
    }

    // f may take no arguments, or a std::stop_token (to be cancellable on pool shutdown).
    template <class F>
    auto submit(F&& f) {
        constexpr bool wants_token = std::is_invocable_v<F&, std::stop_token>;
        using R = typename detail::job_result<std::decay_t<F>>::type;
        auto task = std::make_shared<std::packaged_task<R(std::stop_token)>>(
            [fn = std::forward<F>(f)](std::stop_token st) mutable -> R {
                if constexpr (wants_token) return fn(st);
                else return fn();
            });
        auto fut = task->get_future();
        {
            std::lock_guard lk(m_);
            jobs_.emplace_back([task](std::stop_token st) { (*task)(st); });
        }
        cv_.notify_one();
        return fut;
    }
    // Destructor: implicit. ~jthread() = request_stop() + join(). That's the whole shutdown.
};

}  // namespace lab
