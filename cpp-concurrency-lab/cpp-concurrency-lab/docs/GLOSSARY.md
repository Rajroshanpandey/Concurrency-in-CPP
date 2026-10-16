# Glossary

| Term | One-line meaning |
|---|---|
| **Data race** | two threads access the same non-atomic memory, ≥1 writes, no synchronisation → undefined behaviour |
| **Race condition** | a *logical* bug that depends on timing (can exist with no data race, e.g. check-then-act on an atomic) |
| **Critical section** | code that must not run concurrently with other code touching the same state |
| **RAII** | resource acquisition is initialisation: the destructor releases (`lock_guard`, `jthread`) |
| **Deadlock** | threads wait for each other in a cycle; needs mutual exclusion, hold-and-wait, no pre-emption, circular wait |
| **Spurious wake-up** | `wait()` returns although nobody notified — always wait with a predicate |
| **Lost wake-up** | `notify` before `wait`: a condition variable has no memory — state does |
| **Atomic** | an operation other threads see as all-or-nothing |
| **Happens-before** | the ordering guarantee that makes one thread's writes visible to another |
| **Release / acquire** | a store-release *publishes* earlier writes; a load-acquire that reads it *receives* them |
| **Lock-free** | system-wide progress: some thread always completes (not "no locks", not "faster") |
| **CAS** | compare-and-swap: atomically replace a value only if it is still what you expect |
| **ABA** | a value changes A→B→A; CAS cannot tell something happened |
| **False sharing** | independent variables on one cache line make cores fight; fix with `alignas(64)` |
| **Priority inversion** | a high-priority task waits on a low-priority one that a medium task keeps off the CPU |
| **Priority inheritance** | the lock holder temporarily runs at the waiter's priority |
| **WCET** | worst-case execution time — what real-time analysis uses, never the average |
| **Jitter** | variation in when a periodic task actually runs |
| **Hard / soft real-time** | missing a deadline is unacceptable / merely undesirable |
