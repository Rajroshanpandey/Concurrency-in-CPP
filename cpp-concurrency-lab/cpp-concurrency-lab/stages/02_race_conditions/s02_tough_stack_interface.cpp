// Stage 2 / TOUGH - "Every method is locked" does NOT make the stack thread-safe.
#include <atomic>
#include <lab/ui.hpp>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

// Looks thread-safe: each method takes the lock. But the INTERFACE is racy.
class NaiveStack {
    mutable std::mutex m_;
    std::vector<int> v_;
public:
    void push(int x) { std::lock_guard lk(m_); v_.push_back(x); }
    bool empty() const { std::lock_guard lk(m_); return v_.empty(); }
    int top() const {
        std::lock_guard lk(m_);
        if (v_.empty()) throw std::logic_error("top() on empty");
        return v_.back();
    }
    void pop() {
        std::lock_guard lk(m_);
        if (v_.empty()) throw std::logic_error("pop() on empty");
        v_.pop_back();
    }
};

// Fixed interface: the check, the read and the removal are ONE operation.
class SafeStack {
    std::mutex m_;
    std::vector<int> v_;
public:
    void push(int x) { std::lock_guard lk(m_); v_.push_back(x); }
    std::optional<int> try_pop() {
        std::lock_guard lk(m_);
        if (v_.empty()) return std::nullopt;
        int x = v_.back();
        v_.pop_back();
        return x;
    }
};

struct Result { long long sum = 0; long long count = 0; int exceptions = 0; };

int main() {
    using namespace lab;
    ui::banner(2, "TOUGH", "Thread safety is a property of the INTERFACE", "the empty()/top()/pop() trap");

    constexpr int N = 200000;
    const long long expected = static_cast<long long>(N) * (N + 1) / 2;

    ui::say(
        "Both stacks hold the numbers 1.." + std::to_string(N) + ". Two consumer threads empty them. "
        "Each number must be consumed exactly once, so the grand total must be " + std::to_string(expected) + ".");
    ui::code(
        "// NaiveStack consumer                 // SafeStack consumer\n"
        "if (!s.empty()) {                      while (auto x = s.try_pop()) {\n"
        "    int x = s.top();                       use(*x);\n"
        "    s.pop();                           }\n"
        "    use(x);\n"
        "}");

    // ---- naive ----
    NaiveStack ns;
    for (int i = 1; i <= N; ++i) ns.push(i);
    Result r1[2];
    {
        auto consume = [&](Result& r) {
            while (true) {
                try {
                    if (ns.empty()) break;
                    int x = ns.top();   // another thread may pop between these two calls ...
                    ns.pop();           // ... so we may remove a DIFFERENT element than we read
                    r.sum += x; ++r.count;
                } catch (const std::logic_error&) { ++r.exceptions; }
            }
        };
        std::thread a(consume, std::ref(r1[0])), b(consume, std::ref(r1[1]));
        a.join(); b.join();
    }
    // ---- safe ----
    SafeStack ss;
    for (int i = 1; i <= N; ++i) ss.push(i);
    Result r2[2];
    {
        auto consume = [&](Result& r) {
            while (auto x = ss.try_pop()) { r.sum += *x; ++r.count; }
        };
        std::thread a(consume, std::ref(r2[0])), b(consume, std::ref(r2[1]));
        a.join(); b.join();
    }

    auto total = [](Result(&r)[2]) { return Result{r[0].sum + r[1].sum, r[0].count + r[1].count, r[0].exceptions + r[1].exceptions}; };
    const Result n = total(r1), s = total(r2);
    ui::section("Results");
    ui::kv("expected sum / count", std::to_string(expected) + " / " + std::to_string(N));
    ui::kv("NaiveStack  sum / count", std::to_string(n.sum) + " / " + std::to_string(n.count) + "   exceptions: " + std::to_string(n.exceptions));
    ui::kv("SafeStack   sum / count", std::to_string(s.sum) + " / " + std::to_string(s.count));

    ui::observe(
        "The naive stack usually reports a wrong sum (items consumed twice or silently dropped) "
        "and/or exceptions from pop() on an empty stack - even though EVERY method is locked and "
        "TSan reports no data race. On a 1-core machine the interleaving is rarer; run it a few times.");
    ui::why(
        "The sequence empty() -> top() -> pop() is not protected as a whole. Between two locked "
        "calls the world can change. Fix = change the interface so one call does the whole job: "
        "try_pop(). This is why std::stack is NOT thread-safe by design, and why real concurrent "
        "containers expose operations like try_pop / pop_for instead of top()+pop().");
    ui::expect(s.sum == expected && s.count == N, "SafeStack: every element consumed exactly once");
    return ui::finish();
}
