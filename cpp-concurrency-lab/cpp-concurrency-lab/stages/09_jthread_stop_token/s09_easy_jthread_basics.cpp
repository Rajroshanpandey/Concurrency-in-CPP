// Stage 9 / EASY - std::jthread replaces your ScopedThread, and brings cooperative cancellation.
#include <atomic>
#include <chrono>
#include <lab/ui.hpp>
#include <stdexcept>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;

int main() {
    using namespace lab;
    ui::banner(9, "EASY", "std::jthread", "automatic join + stop_token");

    ui::section("1) Automatic join (goodbye ScopedThread)");
    std::atomic<bool> finished{false};
    try {
        std::jthread t([&] { std::this_thread::sleep_for(50ms); finished = true; });
        throw std::runtime_error("something failed while the thread runs");
    } catch (const std::exception& e) {
        ui::kv("exception", e.what());
    }
    ui::expect(finished.load(), "~jthread joined during unwinding - no std::terminate like plain std::thread");

    ui::section("2) Cooperative cancellation with stop_token");
    ui::say("If the callable's FIRST parameter is a std::stop_token, jthread passes one in. The destructor "
            "(or request_stop()) sets the stop flag; the thread must CHECK it. Nothing is ever killed "
            "from outside - that is what 'cooperative' means, and why resources get cleaned up properly.");
    ui::code(
        "std::jthread worker([](std::stop_token st) {\n"
        "    while (!st.stop_requested()) { do_one_unit_of_work(); }\n"
        "    cleanup();     // runs! the thread chose to return\n"
        "});");
    std::atomic<long> units{0};
    std::atomic<bool> cleaned{false};
    {
        std::jthread worker([&](std::stop_token st) {
            while (!st.stop_requested()) { ++units; std::this_thread::sleep_for(1ms); }
            cleaned = true;
        });
        std::this_thread::sleep_for(60ms);
    }   // ~jthread(): request_stop() then join()
    ui::kv("units of work done", std::to_string(units.load()));
    ui::expect(units > 0 && cleaned.load(), "worker noticed the stop request, cleaned up, and was joined");

    ui::section("3) Stopping on demand");
    std::jthread t2([](std::stop_token st) { while (!st.stop_requested()) std::this_thread::sleep_for(1ms); });
    ui::kv("stop_possible before", t2.get_stop_token().stop_possible() ? "yes" : "no");
    t2.request_stop();
    t2.join();
    ui::expect(t2.get_stop_token().stop_requested(), "request_stop() flag is visible through the token");

    ui::observe("A jthread that ignores its token is just a std::thread that joins on destruction: it will "
                "still hang the destructor if the loop never ends. The token only works if the code polls it "
                "or waits in something that understands it (next demo).");
    ui::why("In embedded shutdown sequences, 'every task owns a jthread and checks its token' gives you an "
            "orderly, deterministic stop: no thread outlives the objects it uses, as long as members are "
            "declared so the jthread is destroyed FIRST.");
    return ui::finish();
}
