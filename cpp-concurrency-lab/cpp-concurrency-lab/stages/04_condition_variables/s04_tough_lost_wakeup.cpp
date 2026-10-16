// Stage 4 / TOUGH - Break it on purpose: lost wake-up and why the predicate is not optional.
#include <chrono>
#include <condition_variable>
#include <lab/ui.hpp>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(4, "TOUGH", "Lost wake-ups and spurious wake-ups", "why cv.wait(lock) without a predicate is a bug");

    ui::say(
        "A condition variable has NO memory. notify_one() wakes a thread that is ALREADY waiting; "
        "if nobody waits yet, the notification evaporates. The only durable thing is the STATE you "
        "keep next to it (the `ready` variable). The predicate is how you consult that state.");

    // ---------------- Part A ----------------
    ui::section("A) Lost wake-up: notify happens BEFORE the wait");
    {
        std::mutex m;
        std::condition_variable cv;
        bool ready = false;

        std::thread producer([&] {
            { std::lock_guard lk(m); ready = true; }
            cv.notify_one();                       // fires immediately - nobody is waiting yet
        });
        std::this_thread::sleep_for(100ms);        // consumer is "late"
        producer.join();

        ui::say("Consumer arrives late and uses a BARE wait_for (no predicate). A bare wait would hang forever; "
                "wait_for(500ms) lets us observe it:");
        std::unique_lock lk(m);
        const auto st = cv.wait_for(lk, 500ms);   // BUG: ignores `ready`
        ui::kv("bare wait_for returned", st == std::cv_status::timeout ? "TIMEOUT - the notify was lost" : "no_timeout");
        ui::expect(st == std::cv_status::timeout, "bare wait missed the notification that already happened");
        ui::kv("...but the state says ready ==", ready ? "true  (the information was there all along!)" : "false");

        const bool ok = cv.wait_for(lk, 500ms, [&] { return ready; });   // predicate checks state FIRST
        ui::expect(ok, "with a predicate the consumer sees ready==true immediately, no wait at all");
    }
    ui::why("wait(lock, pred) is shorthand for:  while (!pred()) wait(lock);  - it tests the STATE before sleeping.");

    // ---------------- Part B ----------------
    ui::section("B) Spurious / unrelated wake-up: waking up does not mean the condition is true");
    {
        std::mutex m;
        std::condition_variable cv;
        bool ready = false;

        std::thread noisy([&] {
            std::this_thread::sleep_for(100ms);
            cv.notify_one();                       // notify WITHOUT making ready true
            std::this_thread::sleep_for(100ms);
            { std::lock_guard lk(m); ready = true; }
            cv.notify_one();
        });

        std::unique_lock lk(m);
        cv.wait(lk);                               // BUG: single bare wait, trusts the wake-up
        const bool wrongly_proceeding = !ready;
        ui::kv("woke up, but ready ==", ready ? "true" : "false  <- we would proceed on a FALSE condition");
        ui::expect(wrongly_proceeding, "bare wait returned although the condition was not true");
        cv.wait(lk, [&] { return ready; });        // the correct way: loop until it IS true
        ui::expect(ready, "predicate version only continues when the condition really holds");
        lk.unlock();
        noisy.join();
    }
    ui::why(
        "Implementations are allowed to wake threads spuriously (kernel quirks, multiple waiters "
        "racing for the same item). So: ALWAYS wait with a predicate, ALWAYS change the state under "
        "the mutex, THEN notify.");

    ui::observe(
        "Checklist for every condition variable you ever write: (1) shared state guarded by the "
        "SAME mutex, (2) modify the state under the lock, (3) notify after (or while) modifying, "
        "(4) wait with a predicate, (5) in embedded code prefer wait_for with a timeout.");
    return ui::finish();
}
