# Aether Benchmark Log

This document tracks performance baselines and major benchmark results throughout Aether's development.

## Baseline 001 — Sequential Event Scan

### Goal

Establish the first traversal-performance baseline for Aether's event representation.

### Environment

- Platform: WSL2 Ubuntu
- Compiler: GCC
- Build: `-O3 -std=c++20`
- Event count: 1,000,000
- `sizeof(Event)`: 32 bytes
- `alignof(Event)`: 8 bytes
- Total event storage: ~32 MB

### Workload

Sequentially traverse a `std::vector<Event>` and accumulate:

- `order_id`
- `quantity`

Event generation is excluded from the timed region.

### Results

| Run | Time (ms) | ns/event |
| --: | --------: | -------: |
|   1 |   2.78163 |  2.78163 |
|   2 |   2.09019 |  2.09019 |
|   3 |   2.49359 |  2.49359 |
|   4 |   2.62501 |  2.62501 |
|   5 |   2.56654 |  2.56654 |

Average:

- Time: ~2.51 ms
- Latency: ~2.51 ns/event
- Throughput: ~398 million events/second

### Notes

`Event` objects are stored contiguously in `std::vector`, enabling efficient sequential memory access, spatial locality, and hardware prefetching.

This benchmark is an initial development baseline, not a canonical hardware benchmark. Results may be affected by WSL, CPU scheduling, cache state, and CPU frequency.
