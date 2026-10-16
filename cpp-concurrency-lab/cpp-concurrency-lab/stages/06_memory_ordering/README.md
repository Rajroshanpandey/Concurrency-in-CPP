# Stage 6 — Memory ordering

> **Goal:** understand *visibility*, not just atomicity. The hardest conceptual jump — go slowly.

## The big idea

Atomicity answers "can another thread see half a value?". **Ordering** answers "when thread 2 sees my flag, does it
also see the data I wrote *before* the flag?" CPUs and compilers reorder; the memory order tells them what they must not.

| Order | Guarantee | Typical use |
|---|---|---|
| `seq_cst` (default) | one global order of all seq_cst ops | start here; correct and simple |
| `release` store / `acquire` load | **handshake**: everything before the release is visible after the acquire that reads it | publish data, flags, queue indices |
| `relaxed` | atomicity only, no ordering | statistics counters read after `join()` |

```
Producer                         Consumer
data = 42            (A)
flag.store(true, release)  ──►   flag.load(acquire) == true
                                 read data == 42   (A happens-before this read)
```

**SPSC ring buffer** — each index has exactly one writer, so no CAS is needed, only the right orders. It is *the*
ISR→task queue of embedded systems. `alignas(64)` keeps `head` and `tail` on separate cache lines (no **false sharing**).

> ⚠ x86 is strongly ordered: broken relaxed code often *appears* to work there and fails on ARM. TSan finds it anywhere.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s06_easy_message_passing.cpp` | release/acquire handshake, 200 000 verified messages |
| 🟢 | `s06_easy_relaxed_broken.cpp` *(RACY)* | the same code with `relaxed` → TSan data race |
| 🟡 | `s06_medium_relaxed_stats.cpp` | relaxed used **correctly** (statistics counter) |
| 🔴 | `s06_tough_spsc_stress.cpp` | SPSC queue: 10 M items in order, ~25× the throughput of a mutex queue |
| 🔴 | `s06_tough_spsc_broken.cpp` *(RACY)* | every order relaxed → TSan flags `spsc_queue.hpp` |

## Your tasks (write them down!)

For each of the 4 atomic operations in `spsc_queue.hpp` write *why* that order is needed and *what breaks* if weaker
(the narration of `s06_tough_spsc_stress` gives the answer — try before you read). Then remove `alignas(64)` and
benchmark on a multi-core machine.

## Self-check

- [ ] State "synchronizes-with" in one sentence.
- [ ] Why is `relaxed` safe for the stats counter but not for the `ready` flag?
- [ ] Why can the producer read its own `tail_` with `relaxed`?
