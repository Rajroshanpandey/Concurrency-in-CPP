// Stage 2 / EASY - Two threads, counter++ a million times each. The answer will not be 2,000,000.
// THIS PROGRAM CONTAINS AN INTENTIONAL DATA RACE (undefined behaviour). Run it under TSan to see the report:
//     cmake --preset tsan && cmake --build --preset tsan --target s02_easy_counter_race
//     ./build/tsan/bin/s02_easy_counter_race
#include <lab/ui.hpp>
#include <string>
#include <thread>

static int counter = 0;  // plain int, no protection - the bug

static bool g_widen = false;

static void increment_many(int n) {
    for (int i = 0; i < n; ++i) {
        if (g_widen) {
            // --widen: split the increment and yield in the middle, so even a 1-core machine
            // gets pre-empted between LOAD and STORE. Same bug, just easier to trigger.
            int tmp = counter;
            if ((i & 255) == 0) std::this_thread::yield();
            counter = tmp + 1;
        } else {
            ++counter;
        }
        // GCC/Clang: forces the compiler to really load+store each time. Without it the optimiser may
        // collapse the loop into "counter += n", hiding the race. It does NOT make anything thread-safe.
        asm volatile("" ::: "memory");
    }
}

int main(int argc, char** argv) {
    using namespace lab;
    g_widen = argc > 1 && std::string(argv[1]) == "--widen";
    ui::banner(2, "EASY", "The lost update", "counter++ is three steps, not one");

    ui::say(
        "counter++ looks atomic but compiles to LOAD -> ADD -> STORE. Two threads can both LOAD 0, "
        "both ADD 1, both STORE 1: two increments, one effect.");
    ui::say(
        "      Thread 1: LOAD 0 ........ ADD -> 1 ..... STORE 1\n"
        "      Thread 2: ...... LOAD 0 ........ ADD -> 1 ..... STORE 1     <- one increment LOST");

    constexpr int per_thread = 1'000'000;
    int worst = 2 * per_thread;
    for (int run = 1; run <= 5; ++run) {
        counter = 0;
        std::thread a(increment_many, per_thread), b(increment_many, per_thread);
        a.join();
        b.join();
        ui::kv("run " + std::to_string(run), std::to_string(counter) + "   (lost " + std::to_string(2 * per_thread - counter) + ")");
        if (counter < worst) worst = counter;
    }

    if (!g_widen)
        ui::note("Seeing 2000000 every time? You are probably on a 1-core machine. Re-run with --widen to make the race visible.");
    ui::observe(
        "Expected 2000000 every time. You get less, and a DIFFERENT wrong number each run. On a "
        "single-core machine you may get lucky and see 2000000 - that proves nothing: the bug is "
        "still there, the scheduler just did not interleave at the wrong instruction.");
    ui::warn(
        "In C++ this is not merely 'a wrong number': an unsynchronised conflicting access is a DATA "
        "RACE and the whole program has undefined behaviour. The compiler may assume it cannot happen.");
    ui::why("Run this under ThreadSanitizer: it prints the two source lines that race. Fixing it is stage 3.");
    return ui::finish();
}
