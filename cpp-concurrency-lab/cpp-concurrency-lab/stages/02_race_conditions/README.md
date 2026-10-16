# Stage 2 — Race conditions

> **Goal:** recognise races on sight, and learn that *"it worked when I ran it"* proves nothing.

## The big idea

`counter++` is **three** steps — LOAD, ADD, STORE. Two threads can interleave them and lose an update:

```
T1: LOAD 0 ........ ADD → 1 ...... STORE 1
T2: ...... LOAD 0 ........ ADD → 1 ...... STORE 1      ← one increment LOST
```

Two different bugs hide under the word "race":

| | Data race | Race condition |
|---|---|---|
| What | unsynchronised conflicting access to the same memory | a logic bug that depends on timing |
| In C++ | **undefined behaviour** (the compiler may assume it never happens) | ordinary wrong behaviour |
| Found by | ThreadSanitizer | careful design, stress tests |
| Fixed by | mutex / atomic | making the *sequence* one operation |

Check-then-act (`if (balance >= x) balance -= x;`) is a race condition even if `balance` is atomic. And a
class whose every method locks can *still* be unsafe if its **interface** forces callers to combine calls
(`empty()` → `top()` → `pop()`). **Thread safety is a property of interfaces, not of individual operations.**

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s02_easy_counter_race.cpp` *(RACY)* | lost updates; run with `--widen` on a 1-core machine; run under TSan |
| 🟡 | `s02_medium_bank_race.cpp` | an atomic balance that still overdraws; the mutex fix |
| 🔴 | `s02_tough_stack_interface.cpp` | fully-locked stack that still loses/duplicates items; `try_pop` fix |

## Break it

* Run the easy demo under the `tsan` preset and read the two source lines.
* In the bank demo, replace the atomic with a plain `int` — the TSan report appears *in addition* to the logic bug.
* Add `std::stack`-style `top()`+`pop()` to your own container and write the failing test first.

## Self-check

- [ ] Why can the optimiser turn a racy `while(!flag){}` into an infinite loop?
- [ ] Name one race that no amount of atomics fixes.
- [ ] What does `try_pop` change about the *interface*?
