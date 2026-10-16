<div align="center">

# C++ Concurrency Lab

**From `std::thread` to lock-free queues and real-time scheduling — learn by running, breaking and fixing real programs.**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue) ![CMake](https://img.shields.io/badge/build-CMake%203.20%2B-064F8C) ![Sanitizers](https://img.shields.io/badge/TSan%20%7C%20ASan-ready-success) ![License](https://img.shields.io/badge/license-MIT-green)

</div>

Most concurrency tutorials show you code that works. This lab shows you code that **fails** — on purpose — then
shows the sanitizer catching it, then the fix. Every exercise is a small program that *narrates itself*: it
explains the idea, runs an experiment, prints numbers, and tells you what to **observe** and **why**.

```
==============================================================================
  STAGE 4  [EASY]  Busy-wait vs condition_variable
  same job, wildly different CPU bill
==============================================================================
  ...
    CPU time burned                    997 ms      <- spin loop
    CPU time burned                    1 ms        <- condition_variable
  OBSERVE  Same wall time (~1000 ms), but the spin loop burned roughly a full core ...
  WHY      wait() atomically (1) releases the mutex and (2) puts the thread to sleep ...
    [PASS] condition_variable used far less CPU than the spin loop
```

> Built for **embedded / modern C++ developers**: the course ends with a Mini ECU (sensor, CAN, control loop,
> logger) and real-time analysis, but everything runs on a normal Linux/macOS PC.

---

## Quick start

```bash
git clone https://github.com/<you>/cpp-concurrency-lab.git
cd cpp-concurrency-lab

./scripts/lab.sh build        # configure + build everything (CMake preset "default")
./scripts/lab.sh              # interactive menu: pick a stage, pick a demo, read the output
```

Or the plain CMake way:

```bash
cmake --preset default && cmake --build --preset default -j
./build/default/bin/s04_easy_busy_vs_cv
ctest --preset default                      # runs every self-checking demo
```

**Requirements:** a C++20 compiler (GCC ≥ 11 or Clang ≥ 14 with `<stop_token>`/`std::jthread`), CMake ≥ 3.20, Linux
or macOS. Windows: use WSL2. (Stage 11's real-time demos are Linux-specific and degrade gracefully elsewhere.)

### The build flavours you will use constantly

| Preset | Command | Why |
|---|---|---|
| `default` | `cmake --preset default` | normal build, fast |
| `tsan` | `cmake --preset tsan` | **ThreadSanitizer** — names the two source lines that race |
| `asan` | `cmake --preset asan` | **AddressSanitizer** — use-after-free, leaks (stage 10) |
| `release` | `cmake --preset release` | `-O3`, for honest benchmarks |

```bash
LAB_PRESET=tsan ./scripts/lab.sh build
LAB_PRESET=tsan ./scripts/lab.sh run s06_easy_relaxed_broken     # watch TSan find the bug
```

---

## The roadmap

Each stage has **🟢 easy → 🟡 medium → 🔴 tough** programs. Do them in order; every stage's theory lives in
its own `README.md`.

| # | Stage | You will learn | Programs |
|---|---|---|---|
| 0 | [Setup & sanitizers](docs/00_setup.md) | build presets, TSan/ASan, how to read a race report | — |
| 1 | [`std::thread`, `join`, `detach`](stages/01_threads/README.md) | thread lifetime, `std::ref`, dangling captures, RAII threads | 4 |
| 2 | [Race conditions](stages/02_race_conditions/README.md) | lost updates, check-then-act, interface-level thread safety | 3 |
| 3 | [Mutexes](stages/03_mutexes/README.md) | `lock_guard`, `unique_lock`, `scoped_lock`, deadlock, dining philosophers | 4 |
| 4 | [Condition variables](stages/04_condition_variables/README.md) | wait/notify, predicates, lost wake-ups, bounded blocking queue | 4 |
| 5 | [`std::atomic`](stages/05_atomics/README.md) | atomics vs mutex, spinlock, torn reads, seqlock | 3 |
| 6 | [Memory ordering](stages/06_memory_ordering/README.md) | release/acquire, relaxed, lock-free SPSC ring buffer | 5 |
| 7 | [Futures & promises](stages/07_futures_promises/README.md) | `async`, exceptions across threads, parallel merge sort | 3 |
| 8 | [Thread pools](stages/08_thread_pool/README.md) | pool with futures, starvation deadlock, pool vs thread-per-job | 5 |
| 9 | [`jthread` & `stop_token`](stages/09_jthread_stop_token/README.md) | cooperative cancellation, `stop_callback`, jthread pool | 3 |
| 10 | [Lock-free programming](stages/10_lock_free/README.md) | CAS loops, Treiber stack, **ABA**, use-after-free, MPMC queue | 5 |
| 11 | [Real-time scheduling](stages/11_realtime/README.md) | drift, jitter, `SCHED_FIFO`, priority inversion, Rate-Monotonic analysis | 5 |
| 12 | [**Capstone: Mini ECU**](stages/12_capstone_mini_ecu/README.md) | everything, in one system with measurable acceptance criteria | 1 |

A suggested 8-week schedule is in [docs/LEARNING_PATH.md](docs/LEARNING_PATH.md).

---

## How to use each exercise (the loop that makes it stick)

1. **Read** the stage `README.md` (5 minutes of theory).
2. **Predict** — read the demo's source, guess what it will print.
3. **Run** it — `./scripts/lab.sh run s03_tough_deadlock_lab`. Read the `OBSERVE` / `WHY` blocks.
4. **Break** it — delete a `lock_guard`, change `acquire` to `relaxed`, remove a predicate. Each stage README has a
   "Break it" list.
5. **Prove it** — rebuild with `tsan`/`asan` and watch the tool agree with you.
6. **Rewrite** it from memory in `playground/` — you are done with a stage when you can write it without looking
   and explain what breaks if any line is removed.

### What the program output means

| Marker | Meaning |
|---|---|
| `OBSERVE` | the thing to look at in the numbers above |
| `WHY` | the explanation / the rule to memorise |
| `WARNING` | a real hazard (undefined behaviour, danger) |
| `[PASS]` / `[FAIL]` | a **deterministic** invariant — these are what `ctest` checks |

Timing-dependent results (speed-ups, jitter) are only *reported*, never asserted, so CI is not flaky.
Colours switch off automatically in pipes and with `NO_COLOR=1`.

> **Single-core machine / VM / container?** Races and speed-ups are harder to see. `s02_easy_counter_race` has a
> `--widen` option (and the stage READMEs say what to expect); the numbers still teach — read the `OBSERVE` text.

---

## Repository layout

```
cpp-concurrency-lab/
├── CMakeLists.txt, CMakePresets.json     build + presets (default / tsan / asan / release)
├── cmake/LabHelpers.cmake                lab_demo(): one line per exercise (+ RACY / UNSAFE tagging)
├── common/include/lab/
│   ├── ui.hpp                            the "narrator": banners, say/observe/why, PASS/FAIL
│   ├── timing.hpp, rt.hpp                stopwatch, percentiles, jitter, SCHED_FIFO helpers
│   ├── blocking_queue.hpp                bounded MPMC queue (mutex + 2 CVs)           stage 4
│   ├── seqlock.hpp, spsc_queue.hpp       lock-free building blocks                     stage 5/6
│   ├── thread_pool.hpp, jthread_pool.hpp pools with futures                            stage 8/9
│   └── mpmc_queue.hpp                    Vyukov bounded MPMC queue                     stage 10
├── stages/NN_name/                       README.md (theory) + sNN_tier_name.cpp (one program per exercise)
├── stages/12_capstone_mini_ecu/          multi-file Mini ECU
├── scripts/lab.sh                        interactive runner      scripts/run_all_demos.sh
├── docs/                                 setup, learning path, glossary
└── .github/workflows/ci.yml              builds + tests under default, TSan, ASan
```

**Naming convention:** `s<stage>_<easy|medium|tough>_<topic>.cpp`. Executables land in `build/<preset>/bin/`.

### Special demo tags

* **`RACY`** — contains an *intentional* data race. Skipped by `ctest` under TSan; run it by hand to see the report.
* **`UNSAFE`** — intentionally dangerous (e.g. `s10_tough_uaf_pop`); never run by `ctest`, needs `--run`.
* Programs that need privileges (`SCHED_FIFO`) explain how to enable them and **exit 0** when unavailable.

---

## Run a stage from CMake

```bash
cmake --build --preset default --target stage06     # build only stage 6
ctest --preset default -L stage06                   # test only stage 6
```

## Contributing

Ideas welcome: new exercises, an RTOS port (FreeRTOS/Zephyr), Windows support, a Clang `-fsanitize=thread` CI leg.
New demo checklist: one file, narrate with `lab::ui`, assert only deterministic facts, add one `lab_demo(...)` line.

## Resources

* *C++ Concurrency in Action* (2nd ed.) — Anthony Williams
* [Jeff Preshing's blog](https://preshing.com) — acquire/release, lock-free fundamentals
* Herb Sutter — *atomic<> Weapons* (CppCon)
* [cppreference](https://en.cppreference.com/w/cpp/thread) — `<thread>`, `<atomic>`, `<stop_token>`
* *Mastering the FreeRTOS Real Time Kernel* (free PDF)

## License

[MIT](LICENSE)
