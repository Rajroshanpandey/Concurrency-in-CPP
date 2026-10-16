#pragma once
// lab/mpmc_queue.hpp - bounded lock-free multi-producer / multi-consumer queue
// (Dmitry Vyukov's design: one sequence number per cell, CAS only on the shared index).
#include <atomic>
#include <cstddef>
#include <memory>

namespace lab {

template <class T>
class MpmcQueue {
    struct Cell {
        std::atomic<std::size_t> seq;
        T data;
    };
    std::unique_ptr<Cell[]> buf_;
    std::size_t mask_;
    alignas(64) std::atomic<std::size_t> enq_{0};
    alignas(64) std::atomic<std::size_t> deq_{0};

public:
    explicit MpmcQueue(std::size_t capacity_pow2) : buf_(new Cell[capacity_pow2]), mask_(capacity_pow2 - 1) {
        for (std::size_t i = 0; i < capacity_pow2; ++i) buf_[i].seq.store(i, std::memory_order_relaxed);
    }

    bool push(T v) {
        std::size_t pos = enq_.load(std::memory_order_relaxed);
        Cell* c;
        for (;;) {
            c = &buf_[pos & mask_];
            const std::size_t seq = c->seq.load(std::memory_order_acquire);
            const auto dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
            if (dif == 0) {  // cell is free for this ticket: try to claim it
                if (enq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
            } else if (dif < 0) {
                return false;  // full
            } else {
                pos = enq_.load(std::memory_order_relaxed);  // someone else got there first
            }
        }
        c->data = std::move(v);
        c->seq.store(pos + 1, std::memory_order_release);  // publish
        return true;
    }

    bool pop(T& out) {
        std::size_t pos = deq_.load(std::memory_order_relaxed);
        Cell* c;
        for (;;) {
            c = &buf_[pos & mask_];
            const std::size_t seq = c->seq.load(std::memory_order_acquire);
            const auto dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
            if (dif == 0) {
                if (deq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
            } else if (dif < 0) {
                return false;  // empty
            } else {
                pos = deq_.load(std::memory_order_relaxed);
            }
        }
        out = std::move(c->data);
        c->seq.store(pos + mask_ + 1, std::memory_order_release);  // hand the cell back for the next lap
        return true;
    }
};

}  // namespace lab
