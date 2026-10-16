#pragma once
// lab/seqlock.hpp - single-writer, many-reader consistent snapshot of a small struct.
// Readers never block the writer; they retry if they overlapped a write.
// Payload words are std::atomic (relaxed) so the code is free of data races in the C++ model.
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <type_traits>

namespace lab {

template <class T>
class SeqLock {
    static_assert(std::is_trivially_copyable_v<T>, "SeqLock<T>: T must be trivially copyable");
    static constexpr std::size_t W = (sizeof(T) + 7) / 8;
    std::atomic<std::uint32_t> seq_{0};   // odd = write in progress
    std::atomic<std::uint64_t> words_[W]{};

public:
    void store(const T& v) {  // SINGLE writer only
        std::uint64_t tmp[W]{};
        std::memcpy(tmp, &v, sizeof(T));
        const auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < W; ++i) words_[i].store(tmp[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);
    }

    T load() const {
        for (;;) {
            const auto s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1u) { std::this_thread::yield(); continue; }
            std::uint64_t tmp[W];
            for (std::size_t i = 0; i < W; ++i) tmp[i] = words_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s1) {
                T out{};
                std::memcpy(&out, tmp, sizeof(T));
                return out;
            }
        }
    }
};

}  // namespace lab
