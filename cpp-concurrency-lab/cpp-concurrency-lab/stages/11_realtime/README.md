# Stage 11 — Real-time scheduling

> **Goal:** shift from *fast* to *predictable*. Real-time means **meeting the deadline every time**, not on average.

## The big idea

A task that averages 0.2 ms but occasionally takes 10 ms *fails* a 1 ms deadline. So we measure and analyse the **worst case**.

* **Periodic tasks:** use `sleep_until(absolute time)`; `sleep_for(period)` adds work + wake-up latency every cycle (**drift**).
* **Jitter:** the spread of wake-up times. Read the **max** and p99.9, never the mean.
* **Priorities:** `SCHED_FIFO` threads run until they block or a higher priority appears. Needs privileges.
* **Don't allocate or page-fault in the loop:** `mlockall`, pre-allocate, no `malloc`/`new`/`std::string` growth on the hot path.
* **Priority inversion:** LOW holds a lock, HIGH waits, MEDIUM runs and starves LOW → HIGH is delayed by MEDIUM (Mars Pathfinder, 1997).
  **Priority inheritance** boosts the holder (`PTHREAD_PRIO_INHERIT`) so blocking time is bounded by the critical section. `std::mutex` offers no control — use pthread mutexes or your RTOS's.
* **Schedulability:** Rate-Monotonic (shortest period = highest priority). Utilisation bound `U ≤ n(2^(1/n) − 1)` is *sufficient*; exact **response-time analysis** is the truth.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s11_easy_periodic_drift.cpp` | seconds of accumulated drift with `sleep_for` vs none with `sleep_until` |
| 🟡 | `s11_medium_jitter.cpp` | min/mean/p99/p99.9/max, then CPU hogs: mean barely moves, max explodes |
| 🔴 | `s11_tough_rt_priority.cpp` | `SCHED_FIFO` + `mlockall` vs normal under load *(needs privileges)* |
| 🔴 | `s11_tough_priority_inversion.cpp` | inversion reproduced (~600 ms) and fixed by inheritance (~150 ms) *(needs privileges)* |
| 🔴 | `s11_tough_rate_monotonic.cpp` | utilisation bound + response-time analysis; pass your own `C:T` pairs |

```bash
sudo ./build/default/bin/s11_tough_priority_inversion
./build/default/bin/s11_tough_rate_monotonic 1:4 2:6 3:12
```

## Mapping to an RTOS

| Concept here | FreeRTOS | Zephyr |
|---|---|---|
| `jthread` / `std::thread` | `xTaskCreate` | `k_thread_create` |
| `std::mutex` (+ inheritance) | `xSemaphoreCreateMutex` (inherits) — *binary semaphores do not* | `k_mutex` (inherits) |
| `BlockingQueue` | `xQueueSend/Receive` | `k_msgq` |
| `condition_variable` | task notifications / event groups | `k_condvar`, `k_poll` |
| periodic `sleep_until` | `vTaskDelayUntil` | `k_timer` / `k_sleep` |

Port your `BlockingQueue` producer/consumer to FreeRTOS (the POSIX/Linux simulator port runs on your PC) to see the same ideas under different names.

## Self-check

- [ ] Why can the average look perfect while the system is unsafe?
- [ ] Draw the three-thread priority-inversion timeline and mark where inheritance changes it.
- [ ] Compute U and the RM bound for a 3-task set by hand, then check with the program.
