// Stage 1 / TOUGH (part 2) - ScopedThread (RAII join) and what happens WITHOUT it.
#include <chrono>
#include <cstdlib>
#include <lab/ui.hpp>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

// RAII wrapper: the destructor joins. This is exactly the problem std::jthread (C++20) solves.
class ScopedThread {
    std::thread t_;
public:
    explicit ScopedThread(std::thread t) : t_(std::move(t)) {
        if (!t_.joinable()) throw std::logic_error("ScopedThread: no thread");
    }
    ~ScopedThread() { if (t_.joinable()) t_.join(); }
    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
};

static void do_work() { throw std::runtime_error("sensor read failed"); }

static void with_scoped_thread() {
    ScopedThread st(std::thread([] { std::this_thread::sleep_for(50ms); }));
    do_work();  // throws -> stack unwinding -> ~ScopedThread() joins -> all good
}

static void with_plain_thread() {
    std::thread t([] { std::this_thread::sleep_for(50ms); });
    do_work();  // throws -> unwinding destroys a JOINABLE std::thread -> std::terminate()
    t.join();   // never reached
}

int main() {
    using namespace lab;
    ui::banner(1, "TOUGH", "ScopedThread vs plain std::thread", "exceptions, RAII, and std::terminate");

    ui::section("1) ScopedThread survives an exception");
    try {
        with_scoped_thread();
    } catch (const std::exception& e) {
        ui::kv("exception caught", e.what());
    }
    ui::expect(true, "ScopedThread joined during stack unwinding, program still alive");

    ui::section("2) plain std::thread + exception = std::terminate");
    ui::say(
        "Now the same code with a bare std::thread. The exception unwinds the stack and destroys "
        "`t` while it is still joinable. The standard says: std::terminate(). Normally that is an "
        "abrupt abort(); we install a terminate handler so the program can tell you what happened.");
    std::set_terminate([] {
        std::cout << "\n    >>> std::terminate() was called! A joinable std::thread was destroyed. <<<\n"
                  << "    That is why you NEVER leave a thread un-joined: use ScopedThread or std::jthread.\n"
                  << std::flush;
        ui::println("");
        ui::say("(demo finished - this handler ends the process on purpose, with exit code 0)");
        std::_Exit(0);
    });
    try {
        with_plain_thread();
    } catch (...) {
        ui::println("this line is never printed");
    }
    return ui::finish();  // never reached
}
