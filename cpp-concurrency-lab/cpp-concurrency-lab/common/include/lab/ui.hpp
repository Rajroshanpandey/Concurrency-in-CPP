#pragma once
// lab/ui.hpp - the "narrator". Every demo uses this so output looks consistent:
// banner -> story (say) -> numbers (kv) -> OBSERVE / WHY boxes -> PASS/FAIL summary.
// Colours switch off automatically when stdout is not a terminal or NO_COLOR is set.
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lab::ui {

namespace detail {
inline bool color_on() {
    static const bool on = ::isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR");
    return on;
}
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
inline std::mutex& io_mutex() { static std::mutex m; return m; }
}  // namespace detail

inline std::string style(std::string_view code, std::string_view text) {
    if (!detail::color_on()) return std::string(text);
    return "\033[" + std::string(code) + "m" + std::string(text) + "\033[0m";
}

// Word-wrap `text` (honours '\n'; lines starting with a space are printed verbatim,
// which is how little ASCII diagrams are embedded in narration).
inline void wrap(std::string_view first, std::string_view next, std::string_view text,
                 std::size_t width = 92) {
    std::lock_guard lk(detail::io_mutex());
    std::istringstream in{std::string(text)};
    std::string line;
    bool first_line = true;
    while (std::getline(in, line)) {
        auto prefix = [&] { auto p = first_line ? first : next; first_line = false; return std::string(p); };
        if (line.empty()) { std::cout << "\n"; continue; }
        if (line.front() == ' ') { std::cout << prefix() << line << "\n"; continue; }
        std::istringstream words(line);
        std::string w, cur;
        const std::size_t room = width > next.size() ? width - next.size() : 40;
        while (words >> w) {
            if (!cur.empty() && cur.size() + 1 + w.size() > room) {
                std::cout << prefix() << cur << "\n";
                cur.clear();
            }
            cur += (cur.empty() ? "" : " ") + w;
        }
        if (!cur.empty()) std::cout << prefix() << cur << "\n";
    }
}

inline void println(const std::string& s) {
    std::lock_guard lk(detail::io_mutex());
    std::cout << s << '\n';
}

inline void rule(char c = '-', int n = 78) { println(style("90", std::string(static_cast<std::size_t>(n), c))); }

inline void banner(int stage, std::string_view tier, std::string_view title, std::string_view subtitle = {}) {
    std::string t(tier);
    const char* col = (t == "EASY") ? "1;32" : (t == "MEDIUM") ? "1;33" : (t == "TOUGH") ? "1;31" : "1;36";
    println("");
    rule('=');
    std::string head = "  STAGE " + (stage ? std::to_string(stage) : std::string("-")) + "  [" + t + "]  " + std::string(title);
    println(style(col, head));
    if (!subtitle.empty()) println(style("2", "  " + std::string(subtitle)));
    rule('=');
}

inline void section(std::string_view title) {
    println("");
    println(style("1;36", "-- " + std::string(title) + " "));
}

// Narration: the author explaining what is about to happen.
inline void say(std::string_view text) { wrap("  ", "  ", text); }

inline void code(std::string_view text) {
    std::istringstream in{std::string(text)};
    std::string l;
    while (std::getline(in, l)) println(style("2", "      | " + l));
}

inline void observe(std::string_view text) {
    println("");
    wrap("  " + style("1;33", "OBSERVE  "), "            ", text);
}
inline void why(std::string_view text) { wrap("  " + style("1;35", "WHY      "), "            ", text); }
inline void warn(std::string_view text) { wrap("  " + style("1;31", "WARNING  "), "            ", text); }
inline void note(std::string_view text) { wrap("  " + style("2", "note     "), "            ", text); }

inline void kv(std::string_view key, std::string_view value) {
    std::string k(key);
    if (k.size() < 34) k.append(34 - k.size(), ' ');
    println("    " + style("2", k) + " " + std::string(value));
}

template <class T>
inline std::string num(T v, int precision = 2) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(precision);
    os << v;
    return os.str();
}

// A check that is printed and counted. Only use for facts that are *deterministic*.
inline bool expect(bool cond, std::string_view msg) {
    ++detail::checks();
    if (!cond) ++detail::failures();
    println("    " + (cond ? style("1;32", "[PASS] ") : style("1;31", "[FAIL] ")) + std::string(msg));
    return cond;
}

// Call as the last line of main(): prints a summary, returns the process exit code.
inline int finish() {
    println("");
    rule();
    if (detail::failures() == 0) {
        println(style("1;32", "  done: " + std::to_string(detail::checks()) + " check(s) passed"));
    } else {
        println(style("1;31", "  " + std::to_string(detail::failures()) + " check(s) FAILED"));
    }
    rule();
    return detail::failures() == 0 ? 0 : 1;
}

}  // namespace lab::ui
