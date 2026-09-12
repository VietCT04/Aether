# Aether Performance

## End-to-End Pipeline

### v0.1 — TCP → epoll → framing → decoder → SPSC → OrderBook

### Environment

- 1,000,000 events
- 32-byte frames, 32 MB total
- 50% ADD / 30% TRADE / 20% CANCEL
- Seed: `42`
- Queue capacity: `65,536`
- Client / Network / Worker pinned to CPUs `2 / 4 / 6`
- Separate physical P-cores
- SMT siblings disabled
- `performance` governor, turbo disabled
- C++20, `-O3 -DNDEBUG -pthread`

### Methodology

- Workload and TCP connection prepared before timing
- Timer: before transmission → all events processed
- 5 independent processes
- 1 warm-up + 5 measured runs per process
- 25 measured runs total
- Report median across all 25 runs
- All runs validated with zero invalid/rejected/lost events

### Results

| Metric | Result |
|---|---:|
| Median | `243.446 ns/event` |
| Throughput | `~4.11M events/s` |
| Minimum | `184.395 ns/event` |
| Maximum | `269.403 ns/event` |
| Median of process medians | `242.850 ns/event` |

### Perf Stat

Median across 5 benchmark processes:

| Metric | Median |
|---|---:|
| CPU cycles | `7.385B` |
| Instructions | `4.868B` |
| IPC | `0.660` |
| Branch miss rate | `4.94%` |
| Cache misses | `13.82M` |
| Context switches | `309` |
| CPU migrations | `13` |
| Page faults | `26,633` |

Perf counters cover the whole benchmark process, including setup, warm-up,
and measured runs, so they are supplementary rather than hot-path
per-event counters.

### Notes

- `ns/event` is amortized throughput cost, not per-event latency.
- `queue_full_count` counts retry attempts, not dropped events.
- Cross-process variance remains significant.
- This is a development baseline for future A/B optimization comparisons.

## OrderBook

### v0.1 Baseline

#### Workload

- 10,000,000 valid events
- 50% ADD
- 30% TRADE
- 20% CANCEL
- fixed RNG seed: 42
- input generated before the timed region
- approximately 3,000,000 active orders remain after the workload
- one warm-up run followed by five measured runs

#### Build

```text
g++ -std=c++20 -O3 -DNDEBUG
````

#### Execution

Benchmark pinned to CPU 2, a P-core.

```text
taskset -c 2 /tmp/bench_order_book
```

#### Results

```text
Run 1: 103.325 ns/event
Run 2: 103.613 ns/event
Run 3: 104.558 ns/event
Run 4: 104.349 ns/event
Run 5: 104.967 ns/event

Median: 104.349 ns/event
Mean:   104.162 ns/event
Range:  103.325–104.967 ns/event
Median throughput: ~9.58 million events/s
```

This is the canonical Aether OrderBook v0.1 baseline.

No performance optimization has been applied based on these results.

#### perf stat

A supplementary whole-process `perf stat` run reported approximately:

```text
cycles:          4.73B
instructions:    5.23B
IPC:             1.11
branches:        1.05B
branch misses:   74.7M
branch miss rate: ~7.1%
cache misses:    22.4M
```

These counters include event generation, setup, timed processing, and destruction, so they are not treated as hot-path per-event measurements.
