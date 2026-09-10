# Aether Performance

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
