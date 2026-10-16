// Stage 3 / TOUGH - Deadlock lab: create it, detect it, fix it three ways.
#include <chrono>
#include <lab/ui.hpp>
#include <latch>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

// ---- Part 1 uses timed_mutex so we can DETECT the deadlock instead of hanging. ----
struct TimedAccount {
    std::timed_mutex m;
    int balance = 1000;
};

static bool naive_transfer_detecting(TimedAccount& from, TimedAccount& to, int amt, std::latch& go, std::latch& reported) {
    std::unique_lock l1(from.m);
    go.arrive_and_wait();            // both threads now hold their FIRST lock
    std::this_thread::sleep_for(20ms);
    std::unique_lock l2(to.m, std::defer_lock);
    const bool got = l2.try_lock_for(300ms);      // a real program would block here forever
    reported.arrive_and_wait();                   // keep holding our first lock until BOTH have reported,
                                                  // otherwise the first to give up would free the other
    if (!got) return false;
    from.balance -= amt;
    to.balance += amt;
    return true;
}

// ---- Parts 2-4: plain mutexes. ----
struct Account {
    std::mutex m;
    int balance = 1000;
};

static void transfer_scoped(Account& from, Account& to, int amt) {
    std::scoped_lock lock(from.m, to.m);    // C++17: locks both with a deadlock-avoidance algorithm
    from.balance -= amt;
    to.balance += amt;
}

static void transfer_manual(Account& from, Account& to, int amt) {
    std::unique_lock l1(from.m, std::defer_lock);   // "I will lock later"
    std::unique_lock l2(to.m, std::defer_lock);
    std::lock(l1, l2);                              // lock both atomically w.r.t. deadlock
    from.balance -= amt;
    to.balance += amt;
}

static void transfer_ordered(Account& from, Account& to, int amt) {
    Account* first = &from < &to ? &from : &to;     // global order: by address
    Account* second = first == &from ? &to : &from;
    std::lock_guard l1(first->m);
    std::lock_guard l2(second->m);
    from.balance -= amt;
    to.balance += amt;
}

template <class Fn>
static bool hammer(Fn transfer, int rounds) {
    Account x, y;
    std::thread a([&] { for (int i = 0; i < rounds; ++i) transfer(x, y, 1); });
    std::thread b([&] { for (int i = 0; i < rounds; ++i) transfer(y, x, 1); });
    a.join();
    b.join();
    return x.balance + y.balance == 2000;   // money conserved?
}

int main() {
    using namespace lab;
    ui::banner(3, "TOUGH", "Deadlock lab", "ABBA deadlock, scoped_lock, std::lock + defer_lock, lock ordering");

    ui::section("Part 1 - create the deadlock (safely detected)");
    ui::say(
        "Thread 1 runs transfer(x -> y): lock x, then y.  Thread 2 runs transfer(y -> x): lock y, then x.\n"
        "      T1 holds X, wants Y\n"
        "      T2 holds Y, wants X     -> each waits for the other forever (a cycle)\n"
        "A latch makes both threads take their first lock before either takes its second, so the "
        "deadlock happens every time. We use try_lock_for(300ms) so the demo reports it instead of hanging.");
    TimedAccount x, y;
    std::latch go(2), reported(2);
    bool ok1 = true, ok2 = true;
    std::thread t1([&] { ok1 = naive_transfer_detecting(x, y, 10, go, reported); });
    std::thread t2([&] { ok2 = naive_transfer_detecting(y, x, 10, go, reported); });
    t1.join();
    t2.join();
    ui::kv("T1 got both locks?", ok1 ? "yes" : "NO");
    ui::kv("T2 got both locks?", ok2 ? "yes" : "NO");
    ui::expect(!ok1 && !ok2, "DEADLOCK reproduced: neither thread could take its second lock");

    ui::section("Part 2 - fix #1: std::scoped_lock(a.m, b.m)");
    ui::say("20000 opposing transfers, two threads. If this deadlocked the program would hang.");
    ui::expect(hammer(transfer_scoped, 20000), "scoped_lock: finished, money conserved");
    ui::why(
        "scoped_lock locks all its mutexes with std::lock's algorithm: it takes the first, "
        "try_locks the others, and if any fails it RELEASES everything and retries in a different "
        "order. It never sits holding one lock while blocking on another, so a wait-cycle cannot form.");

    ui::section("Part 3 - fix #2: unique_lock(defer_lock) + std::lock (the manual version)");
    ui::expect(hammer(transfer_manual, 20000), "std::lock: finished, money conserved");
    ui::say("This is what scoped_lock does for you; useful when you need unique_lock's flexibility "
            "(e.g. to unlock one early or hand the lock to a condition variable).");

    ui::section("Part 4 - fix #3: a global lock ORDER (by address)");
    ui::expect(hammer(transfer_ordered, 20000), "lock ordering: finished, money conserved");
    ui::why(
        "A deadlock needs a cycle. If every thread acquires locks in the same global order, a cycle "
        "is impossible. It works - but the rule lives in your head and every future author must "
        "obey it. scoped_lock enforces it for you.");

    ui::observe(
        "Memorise the four deadlock ingredients: mutual exclusion, hold-and-wait, no pre-emption, "
        "circular wait. Every fix above removes one (scoped_lock removes hold-and-wait; ordering "
        "removes circular wait). Rules of thumb: hold locks briefly, never call unknown code under a "
        "lock, lock in a consistent order - or lock everything at once.");
    return ui::finish();
}
