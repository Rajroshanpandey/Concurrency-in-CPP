// Stage 10 / MEDIUM - Treiber stack: a lock-free LIFO. Safe-memory version (popped nodes are retired, not freed).
#include <atomic>
#include <lab/ui.hpp>
#include <thread>
#include <vector>

// Nodes are never deleted while the stack is alive (they go on a "graveyard" list and are freed in
// the destructor). That sidesteps use-after-free AND ABA, because an address cannot be recycled.
// Real systems need hazard pointers / epoch reclamation - see s10_tough_aba_demo and s10_tough_uaf_pop.
class TreiberStack {
    struct Node {
        int value;
        Node* next;           // stack link
        Node* retired_next;   // graveyard link
    };
    std::atomic<Node*> head_{nullptr};
    std::atomic<Node*> graveyard_{nullptr};

public:
    void push(int v) {
        Node* n = new Node{v, head_.load(std::memory_order_relaxed), nullptr};
        // On failure, compare_exchange_weak writes the current head into n->next, so we just retry.
        while (!head_.compare_exchange_weak(n->next, n, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    bool pop(int& out) {
        Node* h = head_.load(std::memory_order_acquire);
        // acquire pairs with push's release: we may now read h->value and h->next safely.
        while (h && !head_.compare_exchange_weak(h, h->next, std::memory_order_acquire, std::memory_order_acquire)) {}
        if (!h) return false;
        out = h->value;
        // retire (lock-free push onto the graveyard)
        h->retired_next = graveyard_.load(std::memory_order_relaxed);
        while (!graveyard_.compare_exchange_weak(h->retired_next, h, std::memory_order_release, std::memory_order_relaxed)) {}
        return true;
    }

    ~TreiberStack() {
        for (Node* n = graveyard_.load(); n;) { Node* nx = n->retired_next; delete n; n = nx; }
        for (Node* n = head_.load(); n;) { Node* nx = n->next; delete n; n = nx; }
    }
};

int main(int argc, char** argv) {
    using namespace lab;
    const int per = argc > 1 ? std::stoi(argv[1]) : 200000;
    constexpr int PUSHERS = 2, POPPERS = 2;
    ui::banner(10, "MEDIUM", "Treiber stack", "lock-free push/pop with a CAS on the head pointer");

    ui::say("push: new node -> point it at the current head -> CAS(head, old, new).\n"
            "pop : read head -> CAS(head, h, h->next) -> the node is ours.\n"
            "No mutex: if two threads collide, one CAS fails and retries. We run " + std::to_string(PUSHERS) +
            " pushers (values 1.." + std::to_string(per) + " each) and " + std::to_string(POPPERS) + " poppers.");

    TreiberStack st;
    std::atomic<int> producers_done{0};
    std::atomic<long long> popped_sum{0}, popped_cnt{0};
    std::vector<std::thread> ts;
    for (int p = 0; p < PUSHERS; ++p)
        ts.emplace_back([&] {
            for (int i = 1; i <= per; ++i) st.push(i);
            ++producers_done;
        });
    for (int c = 0; c < POPPERS; ++c)
        ts.emplace_back([&] {
            long long s = 0, n = 0;
            int v;
            for (;;) {
                if (st.pop(v)) { s += v; ++n; }
                else if (producers_done == PUSHERS) { if (!st.pop(v)) break; s += v; ++n; }  // re-check after seeing 'done'
                else std::this_thread::yield();
            }
            popped_sum += s;
            popped_cnt += n;
        });
    for (auto& t : ts) t.join();

    const long long expected_cnt = static_cast<long long>(PUSHERS) * per;
    const long long expected_sum = static_cast<long long>(PUSHERS) * per * (per + 1LL) / 2;
    ui::kv("popped count / expected", std::to_string(popped_cnt.load()) + " / " + std::to_string(expected_cnt));
    ui::kv("popped sum / expected", std::to_string(popped_sum.load()) + " / " + std::to_string(expected_sum));
    ui::expect(popped_cnt == expected_cnt && popped_sum == expected_sum, "every pushed value popped exactly once");

    ui::why("Memory orders: push uses release so a popper that sees the new head also sees the node's "
            "value/next; pop uses acquire for the matching half. Everything else can be relaxed.");
    ui::observe("Now ask the uncomfortable question: why does this class never call `delete` in pop()? "
                "Run s10_tough_aba_demo and read s10_tough_uaf_pop.cpp to find out.");
    return ui::finish();
}
