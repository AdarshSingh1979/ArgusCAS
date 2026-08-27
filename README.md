# ArgusCAS

A real-time log monitoring and alerting service written in C++ on Linux. It watches log files as they grow, checks incoming lines against configurable rules, and fires alerts when a rule's threshold is crossed within a time window.

The name combines Argus, the mythological many-eyed watcher, with CAS (compare-and-swap) — the core primitive behind the project's lock-free queue implementation.

## What it does

- Watches one or more log files for new lines as they're written, using `inotify` (with log-rotation handling, so a rotated file doesn't break the watch).
- Feeds every new line into a shared queue.
- A rule engine consumes that queue and checks sliding-window thresholds — for example, firing an alert if the string `ERROR` appears 3 or more times within 10 seconds.
- Alerts are printed to the console as they fire.

## Architecture

Each log file gets its own tailer thread. Every rule runs on its own rule engine thread, consuming from the same shared queue. The queue is the one piece of shared state between producers and consumers, so its correctness and performance are the crux of the project.

## Two queues, one interface

The project implements the shared queue two ways, both behind the same interface, so they can be swapped and benchmarked directly against each other:

- **`ThreadSafeQueue`** — a conventional mutex + condition variable queue. Simple, correct, easy to reason about.
- **`LockFreeQueue`** — a Michael-Scott lock-free queue. Push and pop operate through compare-and-swap instead of a mutex, so no thread ever blocks another; a thread that loses a CAS race simply retries.

### Why lock-free, and what it actually buys you

It's tempting to reach for "lock-free" purely as a speed claim, but that's not really what it guarantees. A lock-free queue's real guarantee is a **progress guarantee**: at least one thread is always making forward progress in a finite number of steps, even if others are paused, descheduled, or stalled. A mutex-based queue has no such guarantee — if the thread holding the lock is preempted, every other thread waiting on that lock stalls with it.

That distinction matters more than raw throughput for a monitoring service, since the whole point is to keep processing log lines even under contention or scheduling pressure.

### Benchmark results

Both queues were benchmarked under identical conditions: 400,000 items pushed across 4 producer threads, measuring total time, per-item correctness (no lost or duplicated items), and push-latency percentiles. Figures below are the median of 5 consecutive runs.

| Queue            | Total time (median) | p50 push latency | p90 push latency | Correctness |
|------------------|---------------------:|------------------:|------------------:|:-----------:|
| `ThreadSafeQueue` | 122 ms                | 76 ns              | 727 ns             | ✅ Passed   |
| `LockFreeQueue`   | 176 ms                | 442 ns             | 1293 ns            | ✅ Passed   |

Two different reliability profiles showed up across repeated runs:

- **`ThreadSafeQueue` was consistent** — total time stayed in a tight 113–124 ms band across all 5 runs.
- **`LockFreeQueue`'s total time varied a lot** — anywhere from 97 ms to 203 ms run to run — so a single-run total-time number isn't trustworthy for this queue and shouldn't be read as a fixed result.
- **What was consistent for `LockFreeQueue`: its p50 and p90 push latency were higher than `ThreadSafeQueue`'s in every one of the 5 runs, with no exceptions.** CAS retries under contention aren't free, and this isn't a workload where lock-free wins on raw per-operation speed.

That's the real takeaway: lock-free doesn't win here on speed, consistently or otherwise. What it guarantees instead is the system-level progress property described above, which doesn't show up in a latency table but is the actual reason to choose it for this use case.

### A real concurrency bug, caught and fixed

Running the benchmark under ThreadSanitizer (`make benchmark-tsan`) surfaced a genuine use-after-free race in `LockFreeQueue::popItem()`: the original implementation deleted a node immediately after unlinking it, but under contention another thread could still hold a reference to that node.

The correct fix is a proper memory reclamation scheme — hazard pointers or epoch-based reclamation — so threads can tell when a node is truly safe to free. That's a substantial addition on its own, and out of scope here. The implemented fix instead **intentionally leaks the node** rather than deleting it: a deliberate memory-for-correctness tradeoff, documented in code, that eliminates the race (confirmed by a clean re-run under ThreadSanitizer with zero warnings) at the cost of unbounded memory growth over a long-running process.

## Building and running

```bash
make          # builds ./log_monitor
./log_monitor <path-to-log-file>

make benchmark       # runs the queue benchmark
make benchmark-tsan  # runs the benchmark under ThreadSanitizer
```

## Running with Docker

The project also ships with a multi-stage `Dockerfile` and a `docker-compose.yml`. The build stage compiles the binary with g++; the runtime image only contains the compiled binary, not the compiler or source.

```bash
mkdir -p logs && touch logs/app.log
docker compose up --build
```

`docker-compose.yml` mounts a local `./logs` directory into the container at `/var/log/watched`, so you can append lines to `./logs/app.log` from the host and watch ArgusCAS pick them up and fire alerts from inside the container:

```bash
echo "ERROR something broke" >> logs/app.log
```

## Project structure