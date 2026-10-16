# Stage 7 — `future`, `promise`, `async`

> **Goal:** pass results *and errors* between threads without hand-rolling mutex + condition variable + variable.

## The big idea

```
promise  ── set_value(42) / set_exception(e) ──►  shared state  ──►  future.get()
 (writer)                                                              (reader)
```

A future has exactly three possible outcomes: **a value, an exception, or `broken_promise`** (the promise died
without answering). `get()` works once. Exceptions are captured and **rethrown in the caller's thread** — failure
becomes ordinary data instead of `std::terminate`.

`std::async(std::launch::async, f)` starts a thread; **its future blocks in the destructor** until the task is
done — so discarding the return value makes the call synchronous. Always pass the launch policy explicitly.

Parallelism needs **grain size**: tasks must be larger than their own creation cost.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s07_easy_async_fib.cpp` | async vs sequential, `launch::deferred` |
| 🟡 | `s07_medium_promise_exception.cpp` | value, exception and `broken_promise` crossing threads |
| 🔴 | `s07_tough_parallel_merge_sort.cpp` | naive (thousands of tasks) vs depth-limited; the blocking destructor proof |

## Self-check

- [ ] Why does an exception inside a plain `std::thread` kill the process but not inside `std::async`?
- [ ] What does `shared_future` add?
- [ ] Why `depth < log2(cores)` as the recursion cut-off?
