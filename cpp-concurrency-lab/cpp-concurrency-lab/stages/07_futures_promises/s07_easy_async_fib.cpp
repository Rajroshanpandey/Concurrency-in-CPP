// Stage 7 / EASY - std::async and std::future: "start now, collect the result later".
#include <future>
#include <lab/timing.hpp>
#include <lab/ui.hpp>

static long long fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }   // deliberately slow

int main() {
    using namespace lab;
    ui::banner(7, "EASY", "std::async + std::future", "a value that will exist later");

    ui::say("A future is a receipt: 'your result will be ready later'. future.get() blocks until it is "
            "ready (and returns it exactly once). We compute two Fibonacci numbers - first one after "
            "the other, then concurrently.");
    ui::code(
        "auto f = std::async(std::launch::async, fib, 38);   // starts a thread NOW\n"
        "...do other things...\n"
        "long long r = f.get();                               // wait + fetch the result");

    Stopwatch sw;
    const long long a = fib(38), b = fib(38);
    const double seq_ms = sw.ms();

    sw.reset();
    auto fa = std::async(std::launch::async, fib, 38);
    auto fb = std::async(std::launch::async, fib, 38);
    const long long a2 = fa.get(), b2 = fb.get();
    const double par_ms = sw.ms();

    ui::kv("sequential  fib(38)+fib(38)", ui::num(seq_ms, 0) + " ms");
    ui::kv("two async tasks", ui::num(par_ms, 0) + " ms");
    ui::kv("results", std::to_string(a2) + ", " + std::to_string(b2));
    ui::expect(a == a2 && b == b2, "async results equal the sequential results");

    ui::section("std::launch policy matters");
    auto lazy = std::async(std::launch::deferred, fib, 20);   // does NOT start a thread
    ui::kv("deferred future status", lazy.wait_for(std::chrono::seconds(0)) == std::future_status::deferred
                                         ? "deferred (nothing has run yet)" : "other");
    ui::expect(lazy.get() == 6765, "deferred task runs on the thread that calls get()");

    ui::observe("On a multi-core machine the two async tasks take about half the time. On 1 core they "
                "take the same total time - concurrency is not parallelism.");
    ui::why("Always pass std::launch::async explicitly. The default policy lets the implementation "
            "choose async|deferred, so your 'background' task might silently run on get().");
    return ui::finish();
}
