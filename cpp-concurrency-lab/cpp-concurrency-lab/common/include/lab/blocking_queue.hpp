#pragma once
// lab/blocking_queue.hpp - bounded, closable, blocking MPMC queue (mutex + 2 condition variables).
// This is the reusable building block of stages 4, 8, 9, 10 and 12.
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace lab {

template <class T>
class BlockingQueue {
    mutable std::mutex m_;
    std::condition_variable not_empty_, not_full_;
    std::queue<T> q_;
    std::size_t cap_;
    bool closed_ = false;

public:
    explicit BlockingQueue(std::size_t capacity) : cap_(capacity) {}

    // Blocks while full. Returns false if the queue was closed.
    bool push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });  // predicate: see stage 4
        if (closed_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // Never blocks. Returns false if full or closed (the caller decides: drop / count / retry).
    bool try_push(T v) {
        {
            std::lock_guard lk(m_);
            if (closed_ || q_.size() >= cap_) return false;
            q_.push(std::move(v));
        }
        not_empty_.notify_one();
        return true;
    }

    // Blocks while empty. nullopt means "closed AND drained".
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    // Embedded rule #1: never wait forever. nullopt = timeout, or closed and drained.
    template <class Rep, class Period>
    std::optional<T> pop_for(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock lk(m_);
        if (!not_empty_.wait_for(lk, timeout, [&] { return !q_.empty() || closed_; })) return std::nullopt;
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    // Wakes EVERY waiter (notify_all): a closed queue is news for all of them.
    void close() {
        {
            std::lock_guard lk(m_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const { std::lock_guard lk(m_); return q_.size(); }
    bool closed() const { std::lock_guard lk(m_); return closed_; }
};

}  // namespace lab
