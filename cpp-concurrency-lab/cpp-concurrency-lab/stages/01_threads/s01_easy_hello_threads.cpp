// Stage 1 / EASY - Launch 4 threads and watch the order change between runs.
#include <atomic>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    using namespace lab;
    ui::banner(1, "EASY", "Hello, threads", "std::thread, join(), and the scheduler you do not control");

    ui::say(
        "We start 4 threads. Every thread waits at a 'starting gate' (an atomic flag), then records "
        "its own id. We do this 6 times. Same code, same input - but the OS scheduler decides who "
        "gets the CPU first, so the order is NOT the same in every round.");
    ui::code(
        "std::vector<std::thread> ts;\n"
        "for (int i = 0; i < 4; ++i) ts.emplace_back([&, i]{ wait_for_gate(); record(i); });\n"
        "for (auto& t : ts) t.join();      // 'wait until you are finished'");

    bool all_ok = true;
    for (int round = 1; round <= 6; ++round) {
        std::atomic<bool> gate{false};
        std::mutex m;
        std::vector<int> order;
        std::vector<std::thread> ts;
        for (int i = 0; i < 4; ++i)
            ts.emplace_back([&, i] {
                while (!gate.load()) std::this_thread::yield();  // wait at the gate
                std::lock_guard lk(m);
                order.push_back(i);
            });
        gate = true;                 // fire the starting gun
        for (auto& t : ts) t.join(); // main thread blocks here until every thread has finished

        std::string line;
        for (int id : order) line += "T" + std::to_string(id) + "  ";
        ui::kv("round " + std::to_string(round) + " finish order", line);
        all_ok = all_ok && order.size() == 4;
    }

    ui::observe(
        "Compare the six lines. On a multi-core machine the order usually differs. On a 1-core "
        "container it may look stable - that does not mean it is deterministic, it means the "
        "scheduler happened to be polite. You may NEVER rely on thread order.");
    ui::why(
        "join() is the only thing that makes the main thread wait. Without it, main() could return "
        "while threads are still running - and destroying a joinable std::thread calls "
        "std::terminate (you will trigger that on purpose in s01_tough_scoped_thread).");
    ui::expect(all_ok, "all 4 threads ran and were joined in every round");
    return ui::finish();
}
