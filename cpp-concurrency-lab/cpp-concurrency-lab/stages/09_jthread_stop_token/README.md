# Stage 9 — `std::jthread` and `std::stop_token`

> **Goal:** safe thread lifetime and cooperative cancellation.

## The big idea

`std::jthread` = `std::thread` + **join in the destructor** + a built-in `std::stop_source`. If the callable's first
parameter is a `std::stop_token`, you get one for free.

```cpp
std::jthread worker([](std::stop_token st) {
    while (!st.stop_requested()) work();
    cleanup();                       // runs — the thread chose to return
});                                  // ~jthread: request_stop() + join()
```

Cancellation is **cooperative**: nothing is killed; the thread must poll the token, or block in something that
understands it — `condition_variable_any::wait(lock, stop_token, pred)` wakes up *by itself* on `request_stop()`.
`std::stop_callback` runs code immediately when stop is requested (poke a blocked syscall, close a socket).

Member order matters: declare the `jthread`s **last** so they are destroyed **first**.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s09_easy_jthread_basics.cpp` | auto-join during exception unwinding; token polling |
| 🟡 | `s09_medium_cv_any_stop.cpp` | stopping a *blocked* worker in < 1 ms, no flag, no notify |
| 🔴 | `s09_tough_jthread_pool.cpp` | `JThreadPool` side-by-side with stage 8; `stop_callback` cancelling a long job |

## Self-check

- [ ] What happens at `~jthread` if the thread ignores its token?
- [ ] Why does `condition_variable_any` accept a stop token but `condition_variable` does not?
- [ ] Which declaration order of members makes shutdown safe?
