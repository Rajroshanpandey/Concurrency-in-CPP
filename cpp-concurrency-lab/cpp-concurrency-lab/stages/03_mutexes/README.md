# Stage 3 — `mutex`, `lock_guard`, `unique_lock`, `scoped_lock`

> **Goal:** protect shared state correctly, never leak a lock, and never deadlock.

## The big idea

A mutex serialises access to a **critical section**. Manual `lock()/unlock()` leaks the lock on every early
`return` or exception, so always use RAII:

| Type | Use it when |
|---|---|
| `std::lock_guard` | simple scope-long critical section (default choice) |
| `std::unique_lock` | you must unlock early, re-lock, defer locking, or wait on a condition variable |
| `std::scoped_lock` | you need **several** mutexes at once (deadlock-avoiding, C++17) |

**Deadlock** needs four ingredients — mutual exclusion, hold-and-wait, no pre-emption, circular wait. Remove one:

* lock everything at once (`scoped_lock`, `std::lock`) → removes hold-and-wait
* a global lock order → removes circular wait
* admission control (semaphore) → bounds the resource demand

**Rules to memorise:** hold locks for the shortest time possible · never call unknown code (callbacks, virtuals)
while holding one · always lock in a consistent order · keep the mutex *and the data it protects* private in one class.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s03_easy_counter_lock_guard.cpp` *(RACY timing run)* | mutex vs atomic vs unsynchronised: correctness and cost |
| 🟡 | `s03_medium_thread_safe_logger.cpp` | encapsulated mutex; callback-under-lock hazard (detected with a timeout) |
| 🔴 | `s03_tough_deadlock_lab.cpp` | ABBA deadlock reproduced; fixed with `scoped_lock`, `std::lock`+`defer_lock`, ordering |
| 🔴 | `s03_tough_dining_philosophers.cpp` | naive deadlock detected; three fixes |

## Break it

* In the deadlock lab, swap `scoped_lock` for two nested `lock_guard`s and watch the hang (replace `try_lock_for` with `lock` — careful, it really hangs).
* Add a method to the logger that returns `std::vector<std::string>&` — now your mutex protects nothing.
* Make the philosophers' fork order depend on `id % 2` — is that a valid fix? (Yes — why?)

## Self-check

- [ ] Why does `scoped_lock(a, b)` not deadlock when another thread calls `scoped_lock(b, a)`?
- [ ] When is `lock_guard` *not* enough?
- [ ] What is the danger of returning a reference to guarded data from a locked method?
