# Stage 4 — `condition_variable`

> **Goal:** wait efficiently for events instead of polling, and never lose a wake-up.

## The big idea

Polling (`while (!ready) {}`) burns a core. A condition variable lets a thread **sleep until something happens**:

```
consumer: lock → while(!pred) wait(lock)          producer: lock → change state → unlock → notify
                    └ atomically unlock + sleep
```

Five rules — each one is a bug if you break it:

1. State lives **next to** the condition variable, guarded by the **same mutex**.
2. Modify the state **under the lock**, then notify.
3. **Always wait with a predicate** (`cv.wait(lk, pred)`): notifications are not memory (lost wake-up) and wake-ups can be spurious.
4. Use `notify_all` when the news concerns every waiter (e.g. `close()`), `notify_one` for "one item arrived".
5. In embedded code prefer `wait_for` — **never wait forever**.

The **bounded blocking queue** (`common/include/lab/blocking_queue.hpp`) is the most reusable building block in the
course: back-pressure, clean shutdown (`close()`), timeouts (`pop_for`), and a non-blocking `try_push` for latency-critical producers.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s04_easy_busy_vs_cv.cpp` | ~1000 ms CPU (spin) vs ~0 ms CPU (cv) for the same wait |
| 🟡 | `s04_medium_blocking_queue.cpp` | back-pressure, `close()` drain semantics, `pop_for` timeout |
| 🔴 | `s04_tough_producers_consumers.cpp` | 3×3 threads, 100 000 items, sum/count/uniqueness checks, clean shutdown |
| 🔴 | `s04_tough_lost_wakeup.cpp` | break it on purpose: lost and spurious wake-ups |

## Break it

* Delete `|| closed_` from a predicate in the queue → consumers hang at shutdown.
* Replace `notify_all()` in `close()` by `notify_one()` and start 3 consumers.
* Close the queue **before** joining the producers (swap steps 1 and 2 in the tough demo) → data loss.

## Self-check

- [ ] Why must `wait` take a `unique_lock` rather than a `lock_guard`?
- [ ] Is it legal to `notify` without holding the mutex? Is it *correct*? (Hint: state change must be locked.)
- [ ] Why does a bounded queue improve robustness over an unbounded one?
