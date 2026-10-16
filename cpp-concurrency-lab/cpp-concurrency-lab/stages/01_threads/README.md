# Stage 1 — `std::thread`, `join`, `detach`

> **Goal:** create threads, pass data in, and manage their lifetime without leaking or crashing.

## The big idea

A `std::thread` is an *independent line of execution*. The OS scheduler — not you — decides which thread runs
when, so **order is never guaranteed**. Two rules cover 90% of beginner bugs:

1. A joinable `std::thread` must be **joined or detached** before it is destroyed, or the program calls `std::terminate()`.
2. A thread must never outlive the objects it uses (a `[&]` capture of a local is a time bomb).

```
main thread ──create──> worker ──────────────> finished
      │                                           │
      └────────────── join() waits here ──────────┘
```

`join()` = *"I wait for you."*  `detach()` = *"Run on your own; I no longer manage you."* Prefer `join()` (or
C++20 `std::jthread`) — a detached thread cannot be waited for, and nobody can tell when it is safe to destroy what it uses.

`std::thread` **copies** its arguments. To pass a reference you must say so: `std::ref(x)` / `std::cref(x)`.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s01_easy_hello_threads.cpp` | 4 threads, finish order changes between rounds (scheduler decides) |
| 🟡 | `s01_medium_parallel_sum.cpp` | parallel sum with one output slot per thread; why more threads ≠ faster |
| 🔴 | `s01_tough_detach_bugs.cpp` | the two bugs of `detach()` (dangling reference, "is it done yet?") and fixes |
| 🔴 | `s01_tough_scoped_thread.cpp` | RAII `ScopedThread`; a plain `std::thread` + exception → `std::terminate` |

## Break it

* In `s01_medium_parallel_sum`, make every iteration write straight into `partial[i]` (not a local) and benchmark on a multi-core box → **false sharing**.
* Remove the `join()` in the medium demo → `terminate`.
* In `s01_tough_detach_bugs`, change `[value]` back to `[&]`, build with `asan`, and watch stack-use-after-return.

## Self-check

- [ ] Why does `std::thread t(f, x)` not pass `x` by reference even if `f` takes `int&`?
- [ ] What exactly happens if a joinable thread is destroyed? Why did the standard choose that?
- [ ] Give two reasons `sleep_for(50ms)` is *not* a substitute for `join()`.
