# Stage 10 — Lock-free programming

> **Goal:** understand CAS loops and why lock-free is *hard*. Do not skip the failures.

## The big idea

**Compare-and-swap:** "if memory still holds what I *expect*, replace it with my *desired* value; otherwise tell me what is there now."
A CAS **loop** (read → compute → CAS → retry on failure) builds any read-modify-write. *Lock-free* means
**system-wide progress** — a failed CAS means *someone else* succeeded — not "no locks" and not "faster".

The hard part is not the CAS. It is **memory reclamation**:

| Hazard | What happens | Remedies |
|---|---|---|
| **Use-after-free** | thread X reads `h->next` after thread Y deleted `h` | hazard pointers, epoch-based reclamation, never free (graveyard) |
| **ABA** | head is `A`, becomes `B`, then `A` again; your CAS succeeds on a *different* world | version-tagged pointer, hazard pointers, no node reuse |

`weak` vs `strong`: `compare_exchange_weak` may fail spuriously (cheaper on LL/SC CPUs) — use it in loops; `strong` when not looping.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s10_easy_atomic_max.cpp` | lock-free max with a CAS loop |
| 🟡 | `s10_medium_treiber_stack.cpp` | lock-free stack (safe-memory version), 400 000 operations verified |
| 🔴 | `s10_tough_aba_demo.cpp` | ABA replayed step by step; tag fixes it |
| 🔴 | `s10_tough_uaf_pop.cpp` *(UNSAFE)* | `delete` in `pop` → use-after-free. Needs `--run`, use the `asan` preset |
| 🔴 | `s10_tough_mpmc_queue.cpp` | Vyukov MPMC queue vs mutex queue — be honest about who wins |

## Your tasks

1. Run `LAB_PRESET=asan ./scripts/lab.sh run s10_tough_uaf_pop --run` and read the ASan report line by line.
2. Add a tagged-pointer `pop` to the Treiber stack (check `is_lock_free()`; 128-bit CAS may need `-latomic`).
3. Benchmark the MPMC queue with 1, 2, 4, 8 threads; write down where lock-free *lost*.

## Self-check

- [ ] Why is "lock-free" not a synonym for "fast"?
- [ ] Draw the ABA interleaving on paper without looking.
- [ ] Why does the Treiber stack in `s10_medium` never call `delete` in `pop`?
