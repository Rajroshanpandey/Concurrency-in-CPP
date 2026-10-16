// Stage 11 / TOUGH - Reproduce priority inversion, then fix it with a priority-inheritance mutex.
// Needs SCHED_FIFO (root / CAP_SYS_NICE). Pins everything to ONE CPU so inversion is guaranteed.
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <lab/rt.hpp>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <pthread.h>
#include <thread>

using namespace std::chrono_literals;

struct Scenario {
    pthread_mutex_t m;
    std::atomic<bool> low_has_lock{false};
    std::atomic<double> high_latency_ms{0};
};

struct ThreadArgs { Scenario* s; int prio; };

static void* low_fn(void* a) {
    auto* s = static_cast<ThreadArgs*>(a)->s;
    pthread_mutex_lock(&s->m);
    s->low_has_lock = true;
    lab::rt::busy_for(150ms);               // critical section: 150 ms of CPU
    pthread_mutex_unlock(&s->m);
    return nullptr;
}
static void* medium_fn(void* a) {
    (void)a;
    lab::rt::busy_for(600ms);               // CPU-bound work that does NOT need the lock
    return nullptr;
}
static void* high_fn(void* a) {
    auto* s = static_cast<ThreadArgs*>(a)->s;
    lab::Stopwatch sw;
    pthread_mutex_lock(&s->m);              // blocks while LOW holds it
    s->high_latency_ms = sw.ms();
    pthread_mutex_unlock(&s->m);
    return nullptr;
}

static pthread_t spawn(void* (*fn)(void*), ThreadArgs* args) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    sched_param sp{};
    sp.sched_priority = args->prio;
    pthread_attr_setschedparam(&attr, &sp);
    pthread_t t;
    if (pthread_create(&t, &attr, fn, args) != 0) throw std::runtime_error("pthread_create failed (privileges?)");
    pthread_attr_destroy(&attr);
    return t;
}

static double run(bool inherit) {
    Scenario s;
    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setprotocol(&ma, inherit ? PTHREAD_PRIO_INHERIT : PTHREAD_PRIO_NONE);
    pthread_mutex_init(&s.m, &ma);

    ThreadArgs lo{&s, 10}, mid{&s, 50}, hi{&s, 80};
    pthread_t tl = spawn(low_fn, &lo);
    while (!s.low_has_lock) std::this_thread::sleep_for(1ms);   // main runs at prio 99: low is pre-empted by us when we wake
    pthread_t tm = spawn(medium_fn, &mid);   // medium becomes runnable: it pre-empts LOW
    pthread_t th = spawn(high_fn, &hi);      // high preempts medium, then blocks on the mutex
    pthread_join(th, nullptr);
    pthread_join(tm, nullptr);
    pthread_join(tl, nullptr);
    pthread_mutex_destroy(&s.m);
    return s.high_latency_ms.load();
}

int main() {
    using namespace lab;
    ui::banner(11, "TOUGH", "Priority inversion", "LOW holds a lock, HIGH wants it, MEDIUM steals the CPU");

    ui::say("Three threads on ONE CPU:\n"
            "      LOW    (prio 10) takes a mutex and works 150 ms inside it\n"
            "      MEDIUM (prio 50) is CPU-bound for 600 ms and does not use the mutex\n"
            "      HIGH   (prio 80) wants the mutex and must run ASAP\n"
            "HIGH blocks on LOW. But MEDIUM out-ranks LOW, so LOW cannot run to release the lock.\n"
            "HIGH is effectively delayed by MEDIUM - a lower-priority task. That is priority inversion\n"
            "(it famously reset the Mars Pathfinder lander in 1997).");

    if (rt::pin_to_cpu(0) != 0) { ui::note("could not pin to CPU 0"); }
    const int rc = rt::set_fifo_priority(99);
    if (rc != 0) {
        ui::warn(std::string("SCHED_FIFO unavailable (") + std::strerror(rc) + "), so this demo cannot run here. "
                 "Run it as root / with CAP_SYS_NICE on a Linux machine:\n"
                 "      sudo ./build/default/bin/s11_tough_priority_inversion\n"
                 "In Docker add --cap-add=SYS_NICE. This is not a failure; exiting with success.");
        return 0;
    }
    ui::warn("This demo briefly runs real-time threads that hog a CPU for ~1 s. Linux RT throttling protects "
             "the machine, but do not run it on a production box.");

    double without = 0, with = 0;
    try {
        ui::section("A) ordinary mutex (PTHREAD_PRIO_NONE)");
        without = run(false);
        ui::kv("HIGH waited for the lock", ui::num(without, 0) + " ms");
        ui::section("B) priority-inheritance mutex (PTHREAD_PRIO_INHERIT)");
        with = run(true);
        ui::kv("HIGH waited for the lock", ui::num(with, 0) + " ms");
    } catch (const std::exception& e) {
        ui::warn(e.what());
        return 0;
    }
    ui::expect(without > with * 2, "priority inheritance shortened HIGH's blocking time");
    ui::observe("Without inheritance HIGH waits for MEDIUM's whole 600 ms PLUS LOW's remaining critical "
                "section. With inheritance, LOW is temporarily boosted to priority 80 while it holds the "
                "lock, finishes its 150 ms immediately, and releases it. HIGH's worst-case blocking time "
                "is now bounded by the length of the critical section - analysable.");
    ui::why("std::mutex gives you no control over this. On embedded Linux use pthread mutexes with "
            "PTHREAD_PRIO_INHERIT; FreeRTOS mutexes (xSemaphoreCreateMutex) implement inheritance, but its "
            "binary semaphores do NOT; Zephyr mutexes inherit. Always check your RTOS.");
    return ui::finish();
}
