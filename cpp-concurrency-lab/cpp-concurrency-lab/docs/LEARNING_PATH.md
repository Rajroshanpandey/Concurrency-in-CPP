# Learning path

| Week | Stages | Goal |
|---|---|---|
| 1 | 0, 1, 2, 3 | create threads, recognise races, protect state, survive deadlocks |
| 2 | 4, 5 | wait without polling, know when atomics suffice |
| 3 | 6, 7 | understand visibility (release/acquire), pass results and errors between threads |
| 4 | 8, 9 | thread pools; safe lifetime and cancellation |
| 5–6 | 10 | CAS loops, ABA, reclamation — slowly, this is the hardest material |
| 7 | 11 | determinism: drift, jitter, priorities, schedulability |
| 8 | 12 | capstone with acceptance criteria |

## You are done with a stage when you can

1. write the code **without looking**,
2. explain **what breaks if each line is removed**,
3. **deliberately create the bug** and show a sanitizer (or a counter) catching it.

## If you only have a weekend

Stage 2 → 3 → 4 (`s04_tough_producers_consumers`) → 6 (`s06_easy_*`) → 12. That covers the questions interviewers ask.

## Biggest mistake to avoid

Jumping to lock-free before `thread → race → mutex → RAII → condition_variable → atomic → memory ordering` feels
intuitive. Lock-free is the *last* tool you reach for, only with a measured need.
