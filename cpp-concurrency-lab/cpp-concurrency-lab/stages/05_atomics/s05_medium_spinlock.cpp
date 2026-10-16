// Stage 5 / MEDIUM - Build a spinlock from std::atomic, then watch it collapse when oversubscribed.
#include <atomic>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>
#include <vector>

class SpinLock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        // exchange(true) returns the OLD value: false means "I got it", true means "someone holds it".
        // acquire: everything the previous owner did before unlock() is visible to me now.
        while (locked_.exchange(true, std::memory_order_acquire)) { /* spin */ }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }   // release: publish my writes
};

// Same lock, but politely yields the CPU while waiting.
class YieldingSpinLock {
    std::atomic<bool> locked_{false};
public:
    void lock() { while (locked_.exchange(true, std::memory_order_acquire)) std::this_thread::yield(); }
    void unlock() { locked_.store(false, std::memory_order_release); }
};

template <class Lock>
static double bench(int threads, int iters, long long& counter) {
    Lock lock;
    counter = 0;
    lab::Stopwatch sw;
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < iters; ++i) {
                lock.lock();
                // A critical section that is long enough to be pre-empted while holding the lock:
                for (int k = 0; k < 50; ++k) asm volatile("" ::: "memory");
                ++counter;
                lock.unlock();
            }
        });
    for (auto& t : ts) t.join();
    return sw.ms();
}

int main(int argc, char** argv) {
    using namespace lab;
    const int iters = argc > 1 ? std::stoi(argv[1]) : 50000;
    const int hw = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    ui::banner(5, "MEDIUM", "Spinlock from std::atomic", "and why spinlocks + pre-emptive scheduling are a bad mix");

    ui::say("Spinlock = a mutex that waits by looping instead of sleeping. Great when the wait is a few "
            "nanoseconds on a dedicated core; terrible when the lock-holder can be descheduled.");
    ui::code(
        "while (locked.exchange(true, acquire)) {}   // lock\n"
        "locked.store(false, release);               // unlock");

    ui::kv("cores", std::to_string(hw));
    long long c1 = 0, c2 = 0, c3 = 0;
    const int few = std::max(2, hw), many = std::max(8, hw * 8);
    ui::section("A) threads == cores (" + std::to_string(few) + ")");
    const double a_spin = bench<SpinLock>(few, iters, c1);
    const double a_yield = bench<YieldingSpinLock>(few, iters, c2);
    auto ns_op = [&](double ms, int th) { return ui::num(ms * 1e6 / (static_cast<double>(th) * iters), 0) + " ns per lock/unlock"; };
    ui::kv("SpinLock", ui::num(a_spin) + " ms   " + ns_op(a_spin, few));
    ui::kv("YieldingSpinLock", ui::num(a_yield) + " ms   " + ns_op(a_yield, few));

    ui::section("B) threads >> cores (" + std::to_string(many) + ")  -  oversubscription");
    const double b_spin = bench<SpinLock>(many, iters, c3);
    ui::kv("SpinLock", ui::num(b_spin) + " ms   " + ns_op(b_spin, many));
    long long c4 = 0;
    const double b_yield = bench<YieldingSpinLock>(many, iters, c4);
    ui::kv("YieldingSpinLock", ui::num(b_yield) + " ms   " + ns_op(b_yield, many));

    ui::expect(c1 == static_cast<long long>(few) * iters, "SpinLock protects the counter (few threads)");
    ui::expect(c3 == static_cast<long long>(many) * iters, "SpinLock protects the counter (many threads)");
    ui::observe(
        "With more runnable threads than cores, the OS may pre-empt the thread that HOLDS the lock. "
        "Every other thread then burns its whole time-slice spinning on a lock that cannot be "
        "released until the holder gets the CPU back. (This also breaks the same way as priority "
        "inversion: a spinning high-priority task can starve the low-priority holder forever on "
        "a single core.) On a 1-core machine the effect is dramatic.");
    ui::why("Use spinlocks only for very short critical sections, with interrupts/pre-emption "
            "disabled, on multi-core hardware - i.e. inside kernels and drivers. In application "
            "code std::mutex (which sleeps via futex) is almost always right.");
    return ui::finish();
}
