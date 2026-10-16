# Stage 5 — `std::atomic`

> **Goal:** know exactly when an atomic is enough — and when it is not.

## The big idea

An atomic operation is **indivisible**: no thread ever observes it half-done. Use atomics for **one small piece of
state** (flag, counter, index); use a mutex for **several related fields that must change together**.

* `std::atomic<bool> running` — correct stop flag (a plain `bool` is a data race *and* may be hoisted out of the loop).
* `fetch_add`, `exchange`, `compare_exchange_*` — atomic read-modify-write.
* `is_lock_free()` — large `atomic<Struct>` silently uses a hidden lock. Check on your target!
* **Atomics don't compose.** Two atomics ≠ one atomic pair: a reader can see `t` from write *n+1* and `p` from write *n*.
* A **seqlock** gives a writer that never blocks and readers that retry — ideal for "sensor snapshot" data.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s05_easy_atomic_basics.cpp` | atomic counter, atomic stop flag, lock-free sizes |
| 🟡 | `s05_medium_spinlock.cpp` | a spinlock from `exchange`; collapse when threads > cores; the `yield` variant |
| 🔴 | `s05_tough_torn_pair.cpp` | torn pair with two atomics → fixed by mutex and by `SeqLock` |

## Break it

* Make `SpinLock::unlock` use `relaxed` and run under TSan.
* Pin the spinlock demo to one core (`taskset -c 0 ./...`) and compare.
* Put a `std::string` in the seqlock payload — the `static_assert` tells you why not.

## Self-check

- [ ] Give an invariant that two separate atomics cannot maintain.
- [ ] Why do spinlocks and pre-emptive scheduling mix badly?
- [ ] Why can a seqlock reader see garbage *temporarily* and still be correct?
