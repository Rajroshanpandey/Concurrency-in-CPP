// Stage 1 / TOUGH (part 1) - The two bugs hiding in "t.detach()", and how to fix them.
#include <atomic>
#include <chrono>
#include <lab/ui.hpp>
#include <thread>

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------------------------
// THE BUGGY CODE FROM THE EXERCISE (shown as text; we do not run it because it is undefined behaviour):
//
//   void start() {
//       int value = 10;
//       std::thread t([&] { std::this_thread::sleep_for(100ms); std::cout << value; });
//       t.detach();
//   }
//
// BUG 1: [&] captures `value` BY REFERENCE. start() returns immediately, `value` is destroyed
//        (dangling reference). 100 ms later the thread reads a dead stack slot.  -> UB
// BUG 2: detach() means nobody will ever join. If main() ends first, the process exits while the
//        thread is mid-flight: its output is lost, and it may touch already-destroyed globals
//        (std::cout itself!) during shutdown.
// ---------------------------------------------------------------------------------------------

static std::atomic<int> printed_value{-1};

// FIX for bug 1: capture BY VALUE - the thread owns its own copy.
static void start_fixed_capture() {
    int value = 10;
    std::thread t([value] {
        std::this_thread::sleep_for(100ms);
        printed_value = value;  // safe: `value` here is the thread's private copy
    });
    t.detach();
}  // `value` dies here, the thread does not care

int main() {
    using namespace lab;
    ui::banner(1, "TOUGH", "The two bugs of detach()", "dangling references and threads that outlive their owner");

    ui::section("Bug 1 - dangling reference");
    ui::say(
        "The original code captured `value` with [&]. We cannot demonstrate it safely (undefined "
        "behaviour can 'work' on your machine and crash on your colleague's), so read the comment "
        "block at the top of this file. Fix: capture by value.");
    start_fixed_capture();
    ui::say("Fixed version started. start() has already returned, the thread is still sleeping...");
    ui::expect(printed_value.load() == -1, "thread has not finished yet when start() returned");
    std::this_thread::sleep_for(250ms);  // crude - and that is exactly bug 2's smell
    ui::expect(printed_value.load() == 10, "thread later saw its OWN copy of value == 10");

    ui::section("Bug 2 - nobody knows when a detached thread is finished");
    ui::say(
        "A detached thread cannot be waited for. Watch: main wants to continue only after the work "
        "is done, but all it can do is guess with sleep_for().");
    std::atomic<bool> done{false};
    std::thread worker([&] {
        std::this_thread::sleep_for(150ms);
        done = true;
    });
    worker.detach();
    std::this_thread::sleep_for(50ms);  // "50 ms should be enough" - famous last words
    ui::kv("work finished after 50 ms guess?", done ? "yes" : "NO - main would have continued too early");
    ui::expect(!done.load(), "guessing with sleep is wrong (the worker needed 150 ms)");

    ui::say("With join() there is no guessing - the thread's end IS the signal:");
    std::atomic<bool> done2{false};
    std::thread worker2([&] {
        std::this_thread::sleep_for(150ms);
        done2 = true;
    });
    worker2.join();
    ui::expect(done2.load(), "after join() the work is guaranteed complete");

    // `worker` is detached and captured `done` by reference; wait so we do not leave it touching main's
    // stack while we return (this IS bug 2, avoided here by sleeping - which proves the point).
    std::this_thread::sleep_for(200ms);

    ui::observe(
        "Every time you are tempted to use detach(), ask: 'what object does this thread use, and "
        "who guarantees it outlives the thread?' If you cannot answer, use join()/jthread.");
    return ui::finish();
}
