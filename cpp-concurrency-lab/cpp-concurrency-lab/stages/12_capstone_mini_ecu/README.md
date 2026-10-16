# Stage 12 — Capstone: the Mini ECU

> Build (and read) one system that uses **every** concept, with **measurable acceptance criteria**.

```
sensor jthread ──(SeqLock snapshot)────────────────────────┐
CAN "ISR" ──(SPSC lock-free queue)──► CAN worker ─(mutex: setpoint)─► shared state ─► CONTROL LOOP 1 ms
atomic<bool> fault ───────────────────────────────────────────────────────────────────────┘      │
                                                                                      try_push (never blocks)
                                                                                                   ▼
                                                                          LOGGER (50 ms/write, BlockingQueue)
```

| Component | Concept | Stage |
|---|---|---|
| sensor 10 ms periodic task | `jthread`, `sleep_until` | 9, 11 |
| CAN ISR → worker | lock-free SPSC queue | 6 |
| setpoint config | `mutex` + `lock_guard` | 3 |
| sensor snapshot | `SeqLock` | 5 |
| control loop 1 ms | real-time scheduling, jitter measurement | 11 |
| logger (slow) | bounded blocking queue + CV, `pop_for` | 4 |
| fault flag | `atomic<bool>` | 5 |
| start-up self-test, sensor-ready handshake | `std::async`, `promise`/`future` | 7 |
| shutdown | `stop_token`, jthread destruction order | 9 |

## Run it

```bash
./build/default/bin/mini_ecu --seconds 5            # good design: logger decoupled
./build/default/bin/mini_ecu --seconds 5 --coupled  # bad design: control loop blocks on the logger
sudo ./build/default/bin/mini_ecu --rt              # SCHED_FIFO control loop
```

## Acceptance criteria

* clean under **TSan and ASan** (`ctest --preset tsan`, `ctest --preset asan`)
* no deadlocks after 1 000 runs: `for i in $(seq 1000); do ./build/default/bin/mini_ecu --seconds 1 >/dev/null || break; done`
* control-loop **max jitter** reported
* **the logger's slowness must not show up in the control loop's jitter** — compare `--coupled` (max ≈ seconds, hundreds of overruns) with the default (max ≈ scheduler noise, few overruns). That comparison is the entire reason for the architecture.

## Extensions

1. Replace the CAN worker's polling with a semaphore signalled by the ISR thread.
2. Add a watchdog task the control loop must "kick" every 5 ms; trigger a fault if it stops.
3. Replace `BlockingQueue` in the logger with the `MpmcQueue` + a `counting_semaphore` for wake-ups.
4. Port the whole thing to FreeRTOS or Zephyr.
5. Compute the schedulability of its task set with `s11_tough_rate_monotonic`.
