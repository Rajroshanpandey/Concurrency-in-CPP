# Stage 8 — Thread pools

> **Goal:** combine mutex, condition variable, queue, shutdown and futures into one reusable component.

## The big idea

Creating a thread costs tens of microseconds and a stack. A pool creates **N threads once**; jobs
(`std::function<void()>`) wait in a queue; workers loop *wait → pop → run*.

```
submit(f) ─► [ job queue ] ─► worker 1
                           ─► worker 2      ← fixed count, fixed memory
                           ─► worker N
```

Details that decide quality (`common/include/lab/thread_pool.hpp`):

* `submit` wraps the call in a `std::packaged_task` → returns a `std::future`; **exceptions land in the future, not the worker**.
* `packaged_task` is move-only but `std::function` needs copyable → keep it in a `shared_ptr` (C++23: `move_only_function`).
* Shutdown = set `stopping_` under the lock, `notify_all`, workers exit only when *stopping AND queue empty* (no job dropped).
* **Starvation deadlock:** a job that blocks waiting for another job *of the same pool* can occupy every worker. Never block a worker on pool work; use continuations or a bigger design.

## Programs

| Tier | File | What you will see |
|---|---|---|
| 🟢 | `s08_easy_single_worker.cpp` | one worker + `BlockingQueue<std::function<void()>>` |
| 🟡 | `s08_medium_pool.cpp` | 4 workers, 40 jobs, destructor drains the queue |
| 🔴 | `s08_tough_pool_futures.cpp` | futures, exceptions, 1000 jobs, `submit` after shutdown |
| 🔴 | `s08_tough_nested_starvation.cpp` | pool starvation deadlock, reproduced deterministically |
| 🔴 | `s08_tough_benchmark.cpp` | 100 000 tiny jobs: thread-per-job vs pool |

## Self-check

- [ ] Why must `worker_loop` re-check emptiness after `wait` returns?
- [ ] Give two fixes for nested-job starvation that do not involve "more threads".
- [ ] On an MCU, why are threads usually created once at start-up?
