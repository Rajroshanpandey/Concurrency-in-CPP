// Stage 8 / TOUGH - submit() returning futures: results, exceptions that don't kill workers, shutdown rules.
#include <lab/thread_pool.hpp>
#include <lab/ui.hpp>
#include <numeric>
#include <stdexcept>
#include <vector>

static int add(int a, int b) { return a + b; }
struct Multiplier { int k; int operator()(int x) const { return x * k; } };

int main() {
    using namespace lab;
    ui::banner(8, "TOUGH", "Pool with futures", "packaged_task, shared_ptr trick, exceptions, shutdown");

    ui::say("submit() is a template: it deduces the return type with std::invoke_result_t, wraps the call "
            "in a std::packaged_task<R()>, and returns the task's future. A packaged_task is move-only "
            "but std::function requires copyable, so we keep it in a std::shared_ptr and capture that. "
            "(C++23's std::move_only_function removes the need.)");

    ThreadPool pool(3);

    ui::section("1) Any callable, any arguments");
    auto f1 = pool.submit(add, 20, 22);                       // free function + arguments
    auto f2 = pool.submit(Multiplier{3}, 14);                 // functor
    auto f3 = pool.submit([](const std::string& s) { return s.size(); }, std::string("hello"));
    ui::expect(f1.get() == 42 && f2.get() == 42 && f3.get() == 5, "functions, functors and lambdas all return their results");

    ui::section("2) Exceptions land in the future, not in the worker");
    auto bad = pool.submit([]() -> int { throw std::runtime_error("job exploded"); });
    try { bad.get(); ui::expect(false, "expected an exception"); }
    catch (const std::runtime_error& e) { ui::kv("rethrown from get()", e.what()); ui::expect(true, "caller receives the job's exception"); }
    auto after = pool.submit([] { return 7; });
    ui::expect(after.get() == 7, "the pool is still healthy after a failing job");

    ui::section("3) 1000 jobs, sum their futures");
    std::vector<std::future<int>> fs;
    for (int i = 1; i <= 1000; ++i) fs.push_back(pool.submit([i] { return i; }));
    long long sum = 0;
    for (auto& f : fs) sum += f.get();
    ui::kv("sum of results", std::to_string(sum));
    ui::expect(sum == 500500, "1+2+...+1000 collected through futures");

    ui::section("4) submit() after shutdown");
    pool.shutdown();
    try { pool.submit([] { return 1; }); ui::expect(false, "should throw"); }
    catch (const std::runtime_error& e) { ui::kv("submit after shutdown", e.what()); ui::expect(true, "rejected loudly instead of queuing a job nobody will run"); }

    ui::observe("A job whose future is never read still runs, but its exception is silently lost - "
                "always get() (or at least wait()) the futures you care about.");
    ui::why("Design choice: after shutdown, submit() throws. The alternative (return an invalid future, or "
            "silently drop) hides bugs. Embedded code without exceptions would return a bool/optional instead.");
    return ui::finish();
}
