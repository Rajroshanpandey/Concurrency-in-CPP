// Stage 4 / MEDIUM - Using the bounded BlockingQueue: back-pressure, close(), and timeouts.
#include <chrono>
#include <lab/blocking_queue.hpp>
#include <lab/ui.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(4, "MEDIUM", "The bounded blocking queue", "the most reusable building block in the course");

    ui::say(
        "Open common/include/lab/blocking_queue.hpp next to this file and read every line. It has "
        "ONE mutex, TWO condition variables (not_empty / not_full), a capacity and a 'closed' flag.");

    // --- 1. back-pressure ---
    ui::section("1) Back-pressure: a FAST producer is slowed down by a SLOW consumer");
    BlockingQueue<int> q(3);   // tiny capacity on purpose
    std::vector<std::string> events;
    std::mutex ev_m;
    auto ev = [&](std::string s) { std::lock_guard lk(ev_m); events.push_back(std::move(s)); };

    std::thread producer([&] {
        for (int i = 1; i <= 8; ++i) { q.push(i); ev("pushed " + std::to_string(i)); }
        q.close();                 // "no more data": wakes the consumer even if the queue is empty
        ev("closed");
    });
    std::thread consumer([&] {
        while (auto v = q.pop()) { std::this_thread::sleep_for(15ms); ev("   popped " + std::to_string(*v)); }
        ev("   consumer sees closed+empty -> exits");
    });
    producer.join();
    consumer.join();
    for (const auto& e : events) ui::println("      " + e);
    ui::observe(
        "The producer races ahead by only about 3-4 items (the capacity plus the one the consumer already took), then every push() blocks until the "
        "consumer frees a slot. Memory use is BOUNDED - an unbounded queue would grow without limit "
        "when the consumer is slower, the classic cause of out-of-memory crashes.");

    // --- 2. close semantics ---
    ui::section("2) close() semantics");
    BlockingQueue<int> q2(4);
    q2.push(1); q2.push(2);
    q2.close();
    ui::expect(!q2.push(3), "push() after close() returns false");
    ui::expect(q2.pop().value() == 1 && q2.pop().value() == 2, "items queued before close() are still delivered (drain)");
    ui::expect(!q2.pop().has_value(), "closed AND empty -> pop() returns nullopt instead of blocking");
    ui::why(
        "close() uses notify_all(), not notify_one(): EVERY blocked producer and consumer must learn "
        "that the world changed. With notify_one a second sleeping consumer would sleep forever.");

    // --- 3. timeouts ---
    ui::section("3) pop_for(): never wait forever (embedded rule #1)");
    BlockingQueue<int> q3(4);
    auto t0 = std::chrono::steady_clock::now();
    auto r = q3.pop_for(120ms);
    auto waited = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ui::kv("pop_for(120ms) on empty queue", std::string(r ? "got a value" : "timed out") + " after " + ui::num(waited, 0) + " ms");
    ui::expect(!r && waited >= 100, "timed out instead of hanging");
    ui::why("A task with a deadline (a watchdog kick, a control loop) must be able to say 'nothing arrived, "
            "do the fallback'. wait_for with a predicate gives you exactly that.");

    ui::section("Thought experiments (do these in the source!)");
    ui::say(
        "1. Delete the `|| closed_` part of a wait predicate. What hangs, and when?\n"
        "2. Replace notify_all() in close() with notify_one(). Start 3 consumers. What happens?\n"
        "3. Remove the predicate and use a bare wait(lk). Which of s04_tough_lost_wakeup's problems appear?");
    return ui::finish();
}
