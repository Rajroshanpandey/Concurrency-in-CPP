# Stage 0 — Set up your lab (30 minutes)

Concurrency bugs are **invisible** without tools. Do this once, then use it for every stage.

## 1. Toolchain

| OS | Install |
|---|---|
| Ubuntu / Debian | `sudo apt install build-essential cmake ninja-build` |
| Fedora | `sudo dnf install gcc-c++ cmake ninja-build` |
| macOS | `xcode-select --install && brew install cmake ninja` |
| Windows | use **WSL2** with Ubuntu |

Check: `g++ --version` ≥ 11 (or `clang++` ≥ 14) and `cmake --version` ≥ 3.20.

## 2. Build

```bash
./scripts/lab.sh build              # = cmake --preset default && cmake --build --preset default
LAB_PRESET=tsan ./scripts/lab.sh build
LAB_PRESET=asan ./scripts/lab.sh build
```

## 3. ThreadSanitizer — your best teacher

TSan instruments every memory access and reports two accesses that conflict without a *happens-before* edge.

```bash
LAB_PRESET=tsan ./scripts/lab.sh run s02_easy_counter_race
```

How to read a report:

```
WARNING: ThreadSanitizer: data race
  Write of size 4 at 0x... by thread T2:        <- access #1  (who, where)
    #0 increment_many s02_easy_counter_race.cpp:18
  Previous write of size 4 at 0x... by thread T1:   <- access #2
    #0 increment_many s02_easy_counter_race.cpp:18
  Location is global 'counter'                  <- WHICH variable
```

Two accesses, at least one a write, no synchronisation between them = **data race = undefined behaviour**.

**Rules for the whole course**

1. Every program you write gets run under TSan (and, for memory-owning code, ASan).
2. Run timing-sensitive tests many times: `for i in {1..200}; do ./app || break; done`.
3. A green run on your laptop proves nothing. A clean TSan run proves *much* more (but not everything — it only
   sees races that actually happen in that run).

## 4. Known sanitizer quirks

* TSan and ASan cannot be combined in one binary — use separate builds.
* On very new kernels TSan may print *"unexpected memory mapping"*: `sudo sysctl vm.mmap_rnd_bits=28`.
* `std::atomic_thread_fence` triggers a TSan warning on GCC ≥ 12 (`seqlock.hpp`); the code is still correct.
* TSan does not support `mlockall` / real-time priorities well — run stage 11 with the `default` or `release` preset.

## 5. Real-time privileges (stage 11 only)

```bash
sudo ./build/default/bin/s11_tough_priority_inversion      # or:
sudo setcap cap_sys_nice+ep build/default/bin/s11_tough_rt_priority
# Docker: add --cap-add=SYS_NICE
```
Without privileges those demos explain what they would show and exit successfully.
