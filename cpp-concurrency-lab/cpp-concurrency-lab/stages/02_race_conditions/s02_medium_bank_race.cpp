// Stage 2 / MEDIUM - check-then-act: atomics alone cannot fix this race.
#include <atomic>
#include <latch>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>

// The balance is std::atomic, so there is NO data race and TSan stays silent.
// The bug is a LOGICAL race: the decision and the action are two separate steps.
struct BrokenAccount {
    std::atomic<int> balance{100};
    bool withdraw(int amt) {
        if (balance.load() >= amt) {           // CHECK
            std::this_thread::yield();         // (widens the window a little - real code has one too)
            balance.fetch_sub(amt);            // ACT  <- someone else may have acted in between
            return true;
        }
        return false;
    }
};

struct SafeAccount {
    int balance = 100;
    std::mutex m;
    bool withdraw(int amt) {
        std::lock_guard lk(m);                 // check and act are ONE indivisible step
        if (balance >= amt) { balance -= amt; return true; }
        return false;
    }
};

template <class Account>
static int count_overdrafts(int trials) {
    int overdrafts = 0;
    for (int i = 0; i < trials; ++i) {
        Account acc;
        std::latch start(3);                    // 2 workers + main: all begin at the same instant
        auto worker = [&] { start.arrive_and_wait(); acc.withdraw(100); };
        std::thread a(worker), b(worker);
        start.arrive_and_wait();
        a.join();
        b.join();
        int bal;
        if constexpr (std::is_same_v<Account, BrokenAccount>) bal = acc.balance.load(); else bal = acc.balance;
        if (bal < 0) ++overdrafts;
    }
    return overdrafts;
}

int main() {
    using namespace lab;
    ui::banner(2, "MEDIUM", "Check-then-act (the bank account)", "why 'make it atomic' is not always the answer");

    ui::say(
        "Balance is 100. Two threads each try to withdraw 100. Exactly ONE may succeed. We repeat "
        "the experiment 3000 times and count how often the balance ends up negative.");
    ui::code(
        "if (balance >= amt)     // thread A and B both see 100 >= 100 ...\n"
        "    balance -= amt;     // ... both subtract: balance = -100");

    const int trials = 3000;
    const int broken = count_overdrafts<BrokenAccount>(trials);
    const int safe = count_overdrafts<SafeAccount>(trials);
    ui::kv("atomic balance, check-then-act", std::to_string(broken) + " overdrafts / " + std::to_string(trials));
    ui::kv("mutex around check AND act", std::to_string(safe) + " overdrafts / " + std::to_string(trials));

    ui::observe(
        "The atomic version still overdraws (on a 1-core box the count may be small - increase "
        "trials, or run on a multi-core machine). Every single operation was atomic, yet the "
        "program is wrong. The mutex version is always 0.");
    ui::why(
        "Atomicity protects ONE operation. Correctness needs the invariant 'balance >= 0' to hold "
        "across a sequence of operations. That sequence must be a single critical section - or a "
        "single compare-and-swap loop (stage 10).");
    ui::expect(safe == 0, "mutex version never overdrafts");
    return ui::finish();
}
