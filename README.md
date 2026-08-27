# ArgusCAS

A real-time log monitoring and alerting service written in C++ on Linux. It watches log files as they grow, checks incoming lines against configurable rules, and fires alerts when a rule's threshold is crossed within a time window.

The name combines Argus, the mythological many-eyed watcher, with CAS (compare-and-swap) — the core primitive behind the project's lock-free queue.

## What it does

- Watches log files for new lines using `inotify`, with log-rotation handling so a rotated file doesn't break the watch.
- Feeds every new line into a shared queue.
- A rule engine consumes the queue and checks sliding-window thresholds — e.g. firing an alert if `ERROR` appears 3+ times within 10 seconds.
- Alerts print to the console as they fire.

## Architecture

Each log file gets its own tailer thread. Every rule runs on its own rule-engine thread, consuming from the same shared queue. The queue is the only shared state between producers and consumers, so its correctness and performance are the crux of the project.

## Two queues, one interface

The shared queue is implemented two ways behind the same interface, so they can be benchmarked directly against each other:

- **`ThreadSafeQueue`** — a mutex + condition variable queue. Simple, correct.
- **`LockFreeQueue`** — a Michael-Scott lock-free queue. Push and pop manipulate the list through compare-and-swap, not a mutex — a thread that loses a CAS race just retries. It's not fully lock-free end-to-end, though: when the queue is empty, `popItem` blocks on a mutex/condition variable rather than spin, so that one path still relies on a lock even though the list operations themselves never do.

### Why lock-free, and what it actually buys you

Lock-free isn't primarily a speed claim — it's a progress guarantee: at least one thread is always making forward progress in a finite number of steps, even if others are paused or descheduled. A mutex-based queue has no such guarantee — if the thread holding the lock is preempted, every thread waiting on it stalls with it.

That property matters more than raw throughput for a monitoring service, since the goal is to keep processing log lines under contention or scheduling pressure.

### Benchmark results

Both queues were benchmarked under identical conditions: 400,000 items pushed across 4 producer threads, measuring total time, correctness, and push-latency percentiles. Figures below are the median of 5 consecutive runs.

| Queue            | Total time (median) | p50 push latency | p90 push latency | Correctness |
|------------------|---------------------:|------------------:|------------------:|:-----------:|
| `ThreadSafeQueue` | 122 ms                | 76 ns              | 727 ns             | ✅ Passed   |
| `LockFreeQueue`   | 176 ms                | 442 ns             | 1293 ns            | ✅ Passed   |

`ThreadSafeQueue`'s total time was consistent across runs (113–124 ms). `LockFreeQueue`'s varied a lot (97–203 ms), so that column isn't a stable number for this queue — but its p50/p90 latency was higher than `ThreadSafeQueue`'s in every one of the 5 runs, with no exceptions. CAS retries under contention aren't free, and this isn't a workload where lock-free wins on raw speed. The progress guarantee above is the actual reason to choose it here, not this table.

### A real concurrency bug, caught and fixed

Running the benchmark under ThreadSanitizer (`make benchmark-tsan`) surfaced a use-after-free race in `LockFreeQueue::popItem()`: the original implementation deleted a node immediately after unlinking it, but under contention another thread could still hold a reference to it.

The correct fix is a proper memory reclamation scheme — hazard pointers or epoch-based reclamation — so threads can tell when a node is safe to free. That's a substantial addition on its own and out of scope here. The current fix intentionally leaks the node instead of deleting it: a memory-for-correctness tradeoff that eliminates the race (clean ThreadSanitizer run after the change) at the cost of unbounded memory growth over a long-running process.

## Building and running

```bash
make          # builds ./log_monitor
./log_monitor <path-to-log-file>

make benchmark       # runs the queue benchmark
make benchmark-tsan  # runs the benchmark under ThreadSanitizer
```

## Running with Docker

Multi-stage `Dockerfile` and `docker-compose.yml` are included. The build stage compiles with g++; the runtime image only contains the compiled binary, not the compiler or source.

```bash
mkdir -p logs && touch logs/app.log
docker compose up --build
```

`docker-compose.yml` mounts a local `./logs` directory into the container at `/var/log/watched`, so appending to `./logs/app.log` on the host triggers alerts from inside the container:

```bash
echo "ERROR something broke" >> logs/app.log
```

## Project structure

```
include/    Header files (queue implementations, tailer, rule engine, alert, log entry)
src/        Implementation files
tests/      Benchmark harness
Makefile    Build, run, and benchmark targets
Dockerfile  Multi-stage build for a minimal runtime image
```