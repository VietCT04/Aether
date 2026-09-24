# Aether Performance

## End-to-End Pipeline

### v0.1 — TCP → epoll → framing → decoder → SPSC → OrderBook

### Environment

- 1,000,000 events
- 32-byte frames, 32 MB total
- 50% ADD / 30% TRADE / 20% CANCEL
- Seed: `42`
- Queue capacity: `65,536`
- Client thread pinned to CPU `2`
- Network thread pinned to CPU `4`
- Worker thread pinned to CPU `6`
- SMT siblings may remain online
- Intel Turbo remains enabled
- CPU governor is recorded for experiments but is not forced to `performance`
- C++20, `-O3 -DNDEBUG -pthread`

### Methodology

- Workload and TCP connection prepared before timing
- Timer: before transmission → all events processed
- 5 independent processes
- 1 warm-up + 5 measured runs per process
- 25 measured runs total
- Report each process median and the median of process medians
- Before/after experiments use the same machine configuration
- All runs validated with zero invalid/rejected/lost events

### Historical baseline

The following result was collected under the previous environment, which
disabled SMT siblings, disabled Turbo, and used the `performance` governor.
It is historical and is **not directly comparable** to experiments using the
revised methodology above.

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

## Performance experiments

### EXP-001 — SPSC cache-line separation

#### Hypothesis

The producer-written and consumer-written SPSC indexes were adjacent atomic
members and could share a cache line, causing cache-line contention between
the network producer and worker consumer.

#### Change

Added `std::hardware_destructive_interference_size` alignment to both index
atomics in `core/spsc_queue.h`. The queue algorithm, capacity, index types,
memory ordering, arithmetic, copies, and backpressure behavior were
unchanged.

#### Environment

- CPU: 12th Gen Intel(R) Core(TM) i5-12500H
- Client/network/worker affinity: CPUs `2/4/6`
- SMT siblings: online
- Intel Turbo: enabled (`intel_pstate/no_turbo=0`)
- Governors on CPUs `2/4/6`: `powersave` (recorded, not forced)
- Compiler: GCC 15.2.0
- Build: C++20, `-O3 -DNDEBUG -pthread`, no LTO
- Workload: 1,000,000 events, 32 MB, seed 42, 50% ADD / 30% TRADE / 20% CANCEL
- Method: one warm-up plus five measured runs, five independent processes

The project performance methodology was revised before EXP-001. SMT
siblings remain online and Intel Turbo remains enabled. Therefore the older
`243.446 ns/event` result is historical and is not directly used as the
EXP-001 baseline.

#### Results

| Process | Before median (ns/event) | After median (ns/event) |
|---:|---:|---:|
| 1 | 164.216 | 156.715 |
| 2 | 288.187 | 161.511 |
| 3 | 177.911 | 158.914 |
| 4 | 257.116 | 153.884 |
| 5 | 168.429 | 157.746 |

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| Median of process medians | 177.911 ns/event | 157.746 ns/event | -20.165 ns/event (-11.334%) |
| Median throughput | 5,620,788 events/s | 6,339,305 events/s | +718,517 events/s (+12.783%) |
| Process spread, max-min/min | 75.493% | 4.956% | -70.537 percentage points |
| Median process `queue_full_count` | 2,778,401 | 2,802,307 | +23,906 (+0.861%) |

All runs validated 32,000,000 bytes received, 1,000,000 decoded,
1,000,000 enqueued, 1,000,000 processed, zero invalid events, and zero
rejected events.

#### Correctness

- Temporary 1,000,000-event SPSC producer/consumer stress check: passed.
- `order_book_test`: passed.
- `wire_decoder_test`: passed.
- `network_pipeline_test`: passed with assertions enabled.
- `e2e_correctness_test`: passed with assertions enabled.
- `e2e_load_test`: passed with assertions enabled.

#### Decision

**KEEP**

#### Interpretation

The post-change measurements show a materially lower median and a much
tighter process spread under the same revised environment. This is
consistent with reduced cache-line contention at the SPSC handoff. The
`queue_full_count` did not decrease; it increased slightly, so reduced
producer backpressure is not demonstrated. The result supports keeping the
layout change, but does not prove that false sharing was the only source of
variance.

Next candidate: **EXP-002 — OrderBook ADD single-lookup insertion**.

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
