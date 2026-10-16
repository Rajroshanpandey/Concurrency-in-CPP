// Stage 10 / TOUGH - The ABA problem, replayed step by step (deterministic), and fixed with a version tag.
#include <array>
#include <atomic>
#include <cstdint>
#include <lab/ui.hpp>
#include <string>

// A tiny stack living in a fixed pool. Nodes are indices, so we can "free and reuse" them on purpose.
struct Node { std::string name; int next; };   // next == -1 means end
using Pool = std::array<Node, 4>;

static std::string show(const Pool& p, int head) {
    std::string s;
    for (int i = head; i != -1; i = p[i].next) s += p[i].name + " -> ";
    return s + "(end)";
}

// ---------------- UNTAGGED: head is just an index ----------------
struct Untagged {
    std::atomic<int> head{-1};
    Pool nodes;
    void push(int i) { nodes[i].next = head.load(); head.store(i); }          // single-threaded setup helper
    int  pop_simple() { int h = head.load(); head.store(nodes[h].next); return h; }
};

// ---------------- TAGGED: head = (tag << 32) | index; every successful change bumps the tag ----------------
struct Tagged {
    std::atomic<std::uint64_t> head{pack(0, -1)};
    Pool nodes;
    static std::uint64_t pack(std::uint32_t tag, int idx) { return (std::uint64_t{tag} << 32) | static_cast<std::uint32_t>(idx); }
    static int idx(std::uint64_t h) { return static_cast<std::int32_t>(h & 0xffffffffu); }
    static std::uint32_t tag(std::uint64_t h) { return static_cast<std::uint32_t>(h >> 32); }
    void push(int i) {
        auto h = head.load();
        nodes[i].next = idx(h);
        head.store(pack(tag(h) + 1, i));
    }
    int pop_simple() {
        auto h = head.load();
        int top = idx(h);
        head.store(pack(tag(h) + 1, nodes[top].next));
        return top;
    }
};

int main() {
    using namespace lab;
    ui::banner(10, "TOUGH", "The ABA problem", "same pointer, different world - replayed deterministically");

    ui::say("Thread 1 is in the middle of pop(): it has read head = A and A.next = B, and is about to run\n"
            "      CAS(head, expected = A, desired = B)\n"
            "It gets pre-empted. Meanwhile Thread 2 does: pop A, pop B, (B is freed and reused elsewhere), push A back.\n"
            "The head is A again. Thread 1 wakes up, its CAS compares head == A: TRUE. It installs B - a node that\n"
            "is no longer part of the stack. We simulate the exact interleaving, step by step.");

    ui::section("UNTAGGED stack");
    {
        Untagged s;
        s.nodes[0] = {"A", -1}; s.nodes[1] = {"B", -1}; s.nodes[2] = {"C", -1}; s.nodes[3] = {"", -1};
        s.push(2); s.push(1); s.push(0);                                   // A -> B -> C
        ui::kv("initial stack", show(s.nodes, s.head.load()));

        int t1_head = s.head.load(), t1_next = s.nodes[t1_head].next;       // T1: reads A, next = B ... pre-empted
        ui::kv("T1 read head/next", s.nodes[t1_head].name + " / " + s.nodes[t1_next].name + "   (T1 is now paused)");

        s.pop_simple();                                                     // T2: pop A
        s.pop_simple();                                                     // T2: pop B
        s.nodes[1] = {"B-reused", -1};                                      // B's memory now holds something else (next = end)
        s.nodes[0].next = s.head.load(); s.head.store(0);                   // T2: push A back (A -> C)
        ui::kv("after T2's pop,pop,reuse,push", show(s.nodes, s.head.load()) + "   <- head == A again!");

        int expected = t1_head;                                             // T1 resumes: CAS(head, A, B)
        const bool ok = s.head.compare_exchange_strong(expected, t1_next);
        ui::kv("T1's CAS(head, A -> B) succeeded?", ok ? "YES (it should not have!)" : "no");
        ui::kv("stack now", show(s.nodes, s.head.load()));
        ui::expect(ok && s.nodes[s.head.load()].name == "B-reused", "ABA corrupted the stack: C is lost, a recycled node is on top");
    }

    ui::section("TAGGED stack (tag bumps on every change)");
    {
        Tagged s;
        s.nodes[0] = {"A", -1}; s.nodes[1] = {"B", -1}; s.nodes[2] = {"C", -1}; s.nodes[3] = {"", -1};
        s.push(2); s.push(1); s.push(0);
        ui::kv("initial stack", show(s.nodes, Tagged::idx(s.head.load())));

        std::uint64_t t1_head = s.head.load();                              // T1 reads (tag 3, A) and next = B
        int t1_next = s.nodes[Tagged::idx(t1_head)].next;
        s.pop_simple(); s.pop_simple();
        s.nodes[1] = {"B-reused", -1};
        s.push(0);                                                          // A is back - but with a NEW tag
        ui::kv("after T2", show(s.nodes, Tagged::idx(s.head.load())) +
                               "   head index == A again, tag " + std::to_string(Tagged::tag(t1_head)) + " -> " +
                               std::to_string(Tagged::tag(s.head.load())));

        std::uint64_t expected = t1_head;
        const bool ok = s.head.compare_exchange_strong(expected, Tagged::pack(Tagged::tag(t1_head) + 1, t1_next));
        ui::kv("T1's CAS succeeded?", ok ? "yes" : "NO - tag mismatch, T1 must re-read and retry");
        ui::expect(!ok, "the version tag turned a silent corruption into a harmless retry");
    }

    ui::observe("The tagged head is a 64-bit value (32-bit tag + 32-bit index), so it is still a single "
                "lock-free atomic on every 64-bit CPU. With real pointers you pack a counter next to the "
                "pointer (needs a double-width CAS: cmpxchg16b / LDXP-STXP) or use fewer pointer bits.");
    ui::why("Other fixes: hazard pointers (a thread announces 'I am looking at X, do not recycle it'), "
            "epoch-based reclamation (free only after every thread has passed a quiescent point), or never "
            "reuse memory (what s10_medium does). Memory RECLAMATION - not the CAS - is the hard part of "
            "lock-free programming.");
    return ui::finish();
}
