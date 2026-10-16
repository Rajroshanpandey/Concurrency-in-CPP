// Stage 7 / MEDIUM - promise/future between threads, exceptions that cross threads, broken_promise.
#include <future>
#include <lab/ui.hpp>
#include <stdexcept>
#include <thread>

int main() {
    using namespace lab;
    ui::banner(7, "MEDIUM", "promise / future and error propagation", "the one-shot channel between two threads");

    ui::say("A promise is the WRITING end of a one-shot channel, a future is the READING end. The shared "
            "state between them replaces the hand-written 'mutex + condition_variable + variable' trio.\n"
            "      worker thread: promise.set_value(42)  ===>  main thread: future.get() -> 42");

    ui::section("1) A value travels");
    {
        std::promise<int> p;
        std::future<int> f = p.get_future();
        std::thread t([p = std::move(p)]() mutable { p.set_value(42); });
        ui::expect(f.get() == 42, "future.get() returned the value set in another thread");
        t.join();
    }

    ui::section("2) An EXCEPTION travels");
    {
        std::promise<int> p;
        std::future<int> f = p.get_future();
        std::thread t([p = std::move(p)]() mutable {
            try { throw std::runtime_error("sensor timeout"); }
            catch (...) { p.set_exception(std::current_exception()); }
        });
        try {
            f.get();
            ui::expect(false, "should have thrown");
        } catch (const std::runtime_error& e) {
            ui::kv("caught in main thread", e.what());
            ui::expect(true, "the exception was rethrown by get() in the CALLER's thread");
        }
        t.join();
    }
    ui::why("An exception escaping a plain std::thread calls std::terminate. Through a promise/"
            "packaged_task/async it is captured and handed to whoever calls get() - failure becomes "
            "ordinary data.");

    ui::section("3) Destroyed without a value: broken_promise");
    {
        std::future<int> f;
        {
            std::promise<int> p;
            f = p.get_future();
        }   // p dies without set_value() or set_exception()
        try {
            f.get();
            ui::expect(false, "should have thrown");
        } catch (const std::future_error& e) {
            ui::kv("future_error", e.code().message());
            ui::expect(e.code() == std::future_errc::broken_promise, "get() reports broken_promise instead of hanging forever");
        }
    }

    ui::section("4) get() works only once");
    {
        std::promise<int> p;
        auto f = p.get_future();
        p.set_value(1);
        f.get();
        ui::expect(!f.valid(), "after get() the future is no longer valid (use shared_future for many readers)");
    }

    ui::observe("Three outcomes for every future: a value, an exception, or broken_promise. Code that "
                "consumes futures must be ready for all three.");
    return ui::finish();
}
