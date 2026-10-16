// Stage 11 / TOUGH - SCHED_FIFO + mlockall: shrink the worst case. Needs privileges; degrades gracefully.
#include <cerrno>
#include <chrono>
#include <cstring>
#include <lab/rt.hpp>
#include <lab/ui.hpp>
#include <thread>

int main(int argc, char** argv) {
    using namespace lab;
    using namespace std::chrono_literals;
    const int iters = argc > 1 ? std::stoi(argv[1]) : 10000;
    ui::banner(11, "TOUGH", "Real-time priority + locked memory", "SCHED_FIFO, mlockall, and why malloc is the enemy");

    ui::say("Linux has real-time scheduling classes. A SCHED_FIFO thread runs until it blocks or a "
            "HIGHER-priority SCHED_FIFO thread appears - ordinary threads can never pre-empt it. We "
            "measure the same 1 ms loop under CPU load, first as a normal thread, then as SCHED_FIFO 80.");
    ui::code(
        "sched_param sp{.sched_priority = 80};\n"
        "pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);   // needs root or CAP_SYS_NICE\n"
        "mlockall(MCL_CURRENT | MCL_FUTURE);                       // no page faults on the hot path");

    const int hogs = static_cast<int>(std::max(2u, std::thread::hardware_concurrency() * 2));
    rt::Hogs hog_threads(hogs);

    ui::section("A) normal priority, under load");
    const auto normal = rt::measure_lateness(1ms, iters, true);
    normal.print("normal thread", "us");

    ui::section("B) SCHED_FIFO priority 80, under the same load");
    const int rc = rt::set_fifo_priority(80);
    if (rc != 0) {
        ui::warn(std::string("Could not enable SCHED_FIFO (") + std::strerror(rc) + "). Real-time "
                 "scheduling needs privileges. Try:  sudo ./build/default/bin/s11_tough_rt_priority\n"
                 "or  sudo setcap cap_sys_nice+ep ./build/default/bin/s11_tough_rt_priority\n"
                 "Docker additionally needs --cap-add=SYS_NICE. The program still succeeds - this is not an error.");
        ui::note("Skipping part B. On a Raspberry Pi / dev board with sudo you will see the max shrink dramatically.");
        return ui::finish();
    }
    const int lock = rt::lock_memory();
    ui::kv("SCHED_FIFO 80", "enabled");
    ui::kv("mlockall", lock == 0 ? "enabled (pages cannot be swapped out)" : "failed (need RLIMIT_MEMLOCK / privileges)");
    const auto fifo = rt::measure_lateness(1ms, iters, true);
    fifo.print("real-time thread", "us");

    ui::kv("MAX lateness normal -> FIFO", ui::num(normal.max(), 0) + " -> " + ui::num(fifo.max(), 0) + " us");
    ui::kv("p99 lateness normal -> FIFO", ui::num(normal.percentile(99), 0) + " -> " + ui::num(fifo.percentile(99), 0) + " us");
    ui::expect(fifo.count() == normal.count(), "both runs completed (timing itself is reported, not asserted: shared machines are noisy)");

    ui::observe("With the hogs still running, the SCHED_FIFO thread wakes almost on time: the scheduler "
                "must run the highest-priority runnable thread. The remaining jitter is timer/interrupt "
                "latency (that is what PREEMPT_RT and hardware timers address).");
    ui::why("mlockall stops the kernel swapping your pages out; a page fault on the hot path costs "
            "microseconds to milliseconds. malloc/new can take a lock, call mmap/brk, and have unbounded "
            "worst-case time. Real-time rule: allocate everything BEFORE the loop starts "
            "(pools, reserve(), static buffers) and never touch the allocator inside it.");
    ui::warn("A SCHED_FIFO thread that never blocks can freeze the machine (Linux's RT throttling normally "
             "rescues it after 95% of a second). Always put a sleep/block in an RT loop.");
    return ui::finish();
}
