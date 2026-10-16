#pragma once
// lab/spsc_queue.hpp - lock-free single-producer / single-consumer ring buffer.
// The template flag `Broken` replaces every acquire/release with relaxed so you can watch it fail.
#include <atomic>
#include <cstddef>

namespace lab {

template <class T, std::size_t N, bool Broken = false>
class SpscQueue {
    static_assert((N & (N - 1)) == 0 && N >= 2, "N must be a power of two");
    static constexpr std::size_t mask_ = N - 1;
    static constexpr auto acq_ = Broken ? std::memory_order_relaxed : std::memory_order_acquire;
    static constexpr auto rel_ = Broken ? std::memory_order_relaxed : std::memory_order_release;

    T buf_[N];
    alignas(64) std::atomic<std::size_t> head_{0};  // next slot to READ  - written by consumer only
    alignas(64) std::atomic<std::size_t> tail_{0};  // next slot to WRITE - written by producer only

public:
    // PRODUCER thread only.
    bool push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);  // (1) my own variable: relaxed is enough
        const std::size_t next = (t + 1) & mask_;
        if (next == head_.load(acq_)) return false;                   // (2) acquire: pairs with (4); slot is really free
        buf_[t] = v;
        tail_.store(next, rel_);                                      // (3) release: publishes buf_[t] to the consumer
        return true;
    }
    // CONSUMER thread only.
    bool pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);  // (1') my own variable
        if (h == tail_.load(acq_)) return false;                      // (2') acquire: pairs with (3); data is visible
        out = buf_[h];
        head_.store((h + 1) & mask_, rel_);                           // (4) release: tells producer the slot is free
        return true;
    }
    static constexpr std::size_t capacity() { return N - 1; }
};

}  // namespace lab
