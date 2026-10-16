// Stage 3 / TOUGH - Dining philosophers: the classic deadlock, fixed three ways.
#include <array>
#include <atomic>
#include <chrono>
#include <lab/timing.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
constexpr int N = 5;

struct Table {
    std::array<std::timed_mutex, N> fork;   // fork[i] is left of philosopher i; fork[(i+1)%N] is right
    std::atomic<int> meals{0};
    std::atomic<int> stalls{0};             // times a philosopher gave up (deadlock detected)
};

enum class Strategy { Naive, ScopedLock, Ordering, Limit4 };

static void eat() { std::this_thread::yield(); }

static void philosopher(Table& t, int id, int rounds, Strategy s, std::counting_semaphore<N>& seats) {
    const int left = id, right = (id + 1) % N;
    for (int r = 0; r < rounds; ++r) {
        switch (s) {
        case Strategy::Naive: {
            t.fork[left].lock();
            std::this_thread::sleep_for(2ms);  // everyone grabs the left fork "at the same time"
            if (!t.fork[right].try_lock_for(150ms)) {   // deadlock detector - real code would wait forever
                t.fork[left].unlock();
                t.stalls++;
                break;
            }
            eat(); t.meals++;
            t.fork[right].unlock();
            t.fork[left].unlock();
            break;
        }
        case Strategy::ScopedLock: {
            std::scoped_lock lk(t.fork[left], t.fork[right]);
            eat(); t.meals++;
            break;
        }
        case Strategy::Ordering: {   // always take the LOWER-numbered fork first
            const int a = std::min(left, right), b = std::max(left, right);
            std::lock_guard l1(t.fork[a]);
            std::lock_guard l2(t.fork[b]);
            eat(); t.meals++;
            break;
        }
        case Strategy::Limit4: {     // at most 4 philosophers sit -> at least one can always get both forks
            seats.acquire();
            {
                std::lock_guard l1(t.fork[left]);
                std::lock_guard l2(t.fork[right]);
                eat(); t.meals++;
            }
            seats.release();
            break;
        }
        }
    }
}

static void run(const char* name, Strategy s, int rounds, int expected_min, bool must_complete) {
    using namespace lab;
    Table t;
    std::counting_semaphore<N> seats(N - 1);
    Stopwatch sw;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(philosopher, std::ref(t), i, rounds, s, std::ref(seats));
    for (auto& th : ts) th.join();
    ui::kv(name, std::to_string(t.meals.load()) + " meals, " + std::to_string(t.stalls.load()) +
                     " deadlock stalls, " + ui::num(sw.ms(), 0) + " ms");
    if (must_complete) ui::expect(t.meals.load() == N * rounds, std::string(name) + ": everybody ate every round");
    else ui::expect(t.stalls.load() > 0, std::string(name) + ": deadlock detected at least once");
    (void)expected_min;
}

int main(int argc, char** argv) {
    using namespace lab;
    const int rounds = argc > 1 ? std::stoi(argv[1]) : 2000;
    ui::banner(3, "TOUGH", "Dining philosophers", "5 philosophers, 5 forks, everybody grabs the left one first");

    ui::say(
        "Five philosophers sit at a round table with five forks. To eat, one needs the fork on the "
        "left AND on the right. If all five pick up their left fork at the same moment, everybody "
        "waits for a right fork that is somebody's left fork: a perfect circular wait.\n"
        "      P0 -> F1 held by P1 -> F2 held by P2 -> F3 -> F4 -> F0 held by P0 ...");
    ui::say("Each philosopher eats " + std::to_string(rounds) + " times.");

    ui::section("Strategies");
    run("naive (left, then right)", Strategy::Naive, 3, 0, false);
    run("fix 1: std::scoped_lock(left,right)", Strategy::ScopedLock, rounds, 0, true);
    run("fix 2: global fork ordering", Strategy::Ordering, rounds, 0, true);
    run("fix 3: only 4 may sit (semaphore)", Strategy::Limit4, rounds, 0, true);

    ui::observe(
        "Naive: the first round deadlocks (all 5 hold the left fork); our 150 ms timeout lets each "
        "philosopher give up so the demo can report it. The three fixes each break a different "
        "deadlock condition: scoped_lock removes hold-and-wait, ordering removes the circular wait "
        "(P4 grabs fork 0 first, breaking the cycle), the semaphore guarantees one philosopher can "
        "always finish and release forks (a resource-limit / admission-control strategy).");
    ui::why("In embedded systems, admission control (fix 3) and ordering (fix 2) are the most common "
            "because they have bounded, analysable behaviour - scoped_lock's retry loop has no fixed worst-case.");
    return ui::finish();
}
