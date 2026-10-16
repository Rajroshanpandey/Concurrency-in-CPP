// Stage 10 / TOUGH - Break your own pop(): the use-after-free hazard of `delete` in a lock-free stack.
//
//   UNSAFE ON PURPOSE. Build and run it under AddressSanitizer:
//       cmake --preset asan && cmake --build --preset asan --target s10_tough_uaf_pop
//       ./build/asan/bin/s10_tough_uaf_pop --run
//
//   Without --run it only explains. It is excluded from ctest.
#include <atomic>
#include <lab/ui.hpp>
#include <string>
#include <thread>
#include <vector>

struct Node { int value; Node* next; };
static std::atomic<Node*> head{nullptr};

static void push(int v) {
    Node* n = new Node{v, head.load(std::memory_order_relaxed)};
    while (!head.compare_exchange_weak(n->next, n, std::memory_order_release, std::memory_order_relaxed)) {}
}

static bool pop(int& out) {
    Node* h = head.load(std::memory_order_acquire);
    while (h && !head.compare_exchange_weak(h, h->next /* <- READS h: may already be freed */,
                                            std::memory_order_acquire, std::memory_order_acquire)) {}
    if (!h) return false;
    out = h->value;
    delete h;                 // <- the bug: another thread may still be about to read h->next
    return true;
}

int main(int argc, char** argv) {
    using namespace lab;
    ui::banner(10, "TOUGH", "use-after-free in pop()", "why this stack must not delete popped nodes");
    ui::say("Timeline of the bug:\n"
            "   T1: h = head (node X)        T2: pops X, `delete X`\n"
            "   T1: reads h->next            <- reads FREED memory (use-after-free)\n"
            "The read happens BEFORE T1's CAS can fail, so no CAS check can protect you. This is memory "
            "corruption, not just a wrong answer.");
    if (argc < 2 || std::string(argv[1]) != "--run") {
        ui::note("Not running the dangerous part. Pass --run (preferably in the asan build) to try it.");
        return 0;
    }
    ui::warn("Running the unsafe stress test now. Expect an AddressSanitizer 'heap-use-after-free' report, or a crash.");
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([] {
            int v;
            for (int i = 0; i < 200000; ++i) { push(i); pop(v); pop(v); }
        });
    for (auto& t : ts) t.join();
    ui::say("Finished without a crash on this run - which proves nothing. Use the asan build.");
    return 0;
}
