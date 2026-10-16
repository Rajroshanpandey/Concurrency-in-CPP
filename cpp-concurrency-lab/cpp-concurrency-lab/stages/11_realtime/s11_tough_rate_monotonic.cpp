// Stage 11 / TOUGH - Schedulability on paper: CPU utilisation, the Rate-Monotonic bound, exact response-time analysis.
//   usage:  s11_tough_rate_monotonic [C:T ...]     e.g.  s11_tough_rate_monotonic 1:4 2:6 3:12   (ms)
#include <algorithm>
#include <cmath>
#include <lab/ui.hpp>
#include <sstream>
#include <vector>

struct Task { std::string name; double C; double T; };   // WCET C, period T == deadline D

int main(int argc, char** argv) {
    using namespace lab;
    ui::banner(11, "TOUGH", "Rate-Monotonic analysis", "can these periodic tasks ALWAYS meet their deadlines?");

    std::vector<Task> tasks;
    for (int i = 1; i < argc; ++i) {
        double c = 0, t = 0; char colon;
        std::istringstream is(argv[i]);
        if (is >> c >> colon >> t && t > 0) tasks.push_back({"task" + std::to_string(i), c, t});
    }
    if (tasks.empty()) tasks = {{"motor ", 1, 5}, {"can   ", 2, 10}, {"sensor", 4, 20}};   // default example (ms)

    // Rate-monotonic priority: SHORTER period = HIGHER priority.
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) { return a.T < b.T; });

    ui::say("Rate-Monotonic Scheduling: give the task with the shortest period the highest priority. "
            "Each task i has a worst-case execution time C and a period T (deadline = period).");
    ui::section("Task set");
    double U = 0;
    for (const auto& t : tasks) {
        U += t.C / t.T;
        ui::kv(t.name, "C = " + ui::num(t.C, 1) + " ms   T = " + ui::num(t.T, 1) + " ms   U = " + ui::num(t.C / t.T, 3));
    }
    const double n = static_cast<double>(tasks.size());
    const double bound = n * (std::pow(2.0, 1.0 / n) - 1.0);

    ui::section("Test 1 - Liu & Layland utilisation bound (sufficient, not necessary)");
    ui::kv("total utilisation U", ui::num(U, 3));
    ui::kv("bound  n(2^(1/n) - 1)", ui::num(bound, 3) + "   (-> 0.693 as n grows)");
    const bool bound_ok = U <= bound;
    ui::kv("U <= bound ?", bound_ok ? "YES -> guaranteed schedulable" : "NO -> inconclusive, run the exact test");

    ui::section("Test 2 - exact response-time analysis");
    ui::say("R_i = C_i + sum over higher-priority tasks j of ceil(R_i / T_j) * C_j   (iterate until it stops changing). "
            "Schedulable iff R_i <= T_i for every task.");
    bool all_ok = true;
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        double R = tasks[i].C, prev = 0;
        while (R != prev && R <= tasks[i].T * 10) {
            prev = R;
            R = tasks[i].C;
            for (std::size_t j = 0; j < i; ++j) R += std::ceil(prev / tasks[j].T) * tasks[j].C;
        }
        const bool ok = R <= tasks[i].T;
        all_ok = all_ok && ok;
        ui::kv(tasks[i].name, "worst-case response R = " + ui::num(R, 1) + " ms  vs deadline " + ui::num(tasks[i].T, 1) +
                                   " ms   " + (ok ? "OK" : "MISSES DEADLINE"));
    }
    ui::section("Verdict");
    ui::expect(all_ok, all_ok ? "task set is schedulable under Rate-Monotonic priorities" : "task set is NOT schedulable");
    if (bound_ok) ui::expect(all_ok, "consistency: bound said yes, exact test must agree");

    ui::observe("Try the sets:  1:4 2:6 3:12  (U = 0.917, above the 3-task bound 0.780 -> inconclusive; "
                "is it still schedulable?) and  1:2 1:3 1:6  and a task whose C is close to its T. The bound is "
                "pessimistic; the exact test is the truth for RM priorities.");
    ui::why("Everything in this analysis assumes WORST-CASE execution times and NO blocking. In a real system "
            "add blocking time from mutexes B_i (priority inheritance bounds it) and interrupt overhead. This "
            "is why 'average speed' never appears in a hard-real-time design document.");
    return ui::finish();
}
