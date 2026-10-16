// Stage 3 / MEDIUM - Encapsulate the mutex with its data, and never call unknown code under a lock.
#include <chrono>
#include <functional>
#include <future>
#include <lab/ui.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Good design: mutex and data are PRIVATE. No reference/pointer to the data ever escapes.
class ThreadSafeLogger {
    mutable std::mutex m_;
    std::vector<std::string> lines_;
public:
    void log(std::string s) {
        std::lock_guard lk(m_);
        lines_.push_back(std::move(s));
    }
    std::size_t size() const { std::lock_guard lk(m_); return lines_.size(); }

    // Returns a COPY: the caller gets a snapshot, not a window into protected state.
    std::vector<std::string> snapshot() const { std::lock_guard lk(m_); return lines_; }

    // DANGEROUS API: runs user code while holding our lock.
    void for_each_locked(const std::function<void(const std::string&)>& cb) const {
        std::lock_guard lk(m_);
        for (const auto& l : lines_) cb(l);
    }
};

int main() {
    using namespace lab;
    ui::banner(3, "MEDIUM", "A thread-safe logger", "encapsulation, snapshots, and callbacks under a lock");

    ThreadSafeLogger logger;
    ui::section("1) Normal use from 4 threads");
    {
        std::vector<std::thread> ts;
        for (int t = 0; t < 4; ++t)
            ts.emplace_back([&, t] { for (int i = 0; i < 250; ++i) logger.log("thread " + std::to_string(t) + " line " + std::to_string(i)); });
        for (auto& t : ts) t.join();
    }
    ui::kv("lines logged", std::to_string(logger.size()));
    ui::expect(logger.size() == 1000, "no line lost");
    ui::say("Because data and mutex are private members, a caller CANNOT forget to lock - the class "
            "makes the wrong thing impossible. That is the design goal.");

    ui::section("2) The danger: calling unknown code while holding a lock");
    ui::say(
        "for_each_locked() holds the logger's mutex while running YOUR callback. If your callback "
        "needs the same logger from another thread (say it hands work to a helper and waits), "
        "you get a deadlock. We detect it with a timeout so this demo cannot hang:");
    ui::code(
        "logger.for_each_locked([&](const std::string&) {\n"
        "    auto helper = std::async(std::launch::async, [&]{ logger.log(\"from helper\"); });\n"
        "    helper.wait_for(300ms);   // would be wait() in naive code -> hangs forever\n"
        "});");
    bool deadlock_detected = false;
    std::future<void> helper;
    logger.for_each_locked([&](const std::string&) {
        if (helper.valid()) return;  // only do it once
        helper = std::async(std::launch::async, [&] { logger.log("from helper"); });
        if (helper.wait_for(300ms) == std::future_status::timeout) deadlock_detected = true;
    });  // lock released here -> the helper can finally proceed
    helper.get();
    ui::kv("helper blocked for the whole callback?", deadlock_detected ? "YES (would have deadlocked)" : "no");
    ui::expect(deadlock_detected, "callback under lock blocked another thread that needed the same mutex");

    ui::observe(
        "The helper could not log until the callback returned, because the callback held the lock "
        "while WAITING for the helper. If it had used a plain wait(), both threads would sleep "
        "forever. Nothing was 'wrong' with any single line - the design (callback under lock) was.");
    ui::why(
        "Rule: never call unknown code (callbacks, virtual functions, user lambdas) while holding a "
        "lock. Safer designs: (a) copy the data out under the lock (snapshot()) and run the "
        "callback on the copy, or (b) document precisely what the callback may not do.");
    return ui::finish();
}
