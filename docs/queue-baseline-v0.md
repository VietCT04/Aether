# Aether Queue Baseline v0

## Goal

Establish a baseline for inter-thread event transport using:

- `std::queue<Event>`
- `std::mutex`
- `std::condition_variable`
- one producer thread
- one consumer thread

This implementation will be kept for comparison with the future SPSC queue.

## Architecture

```text
Producer thread
      │
      ▼
std::queue<Event>
std::mutex
std::condition_variable
      │
      ▼
Consumer thread
      │
      ▼
checksum processing
```

The mutex protects only the shared queue.

The consumer removes an event from the queue, releases the mutex, processes the event, then reacquires the mutex before accessing the queue again.

This allows the producer to continue publishing events while the consumer processes the previous event.

## Workload

* Events: 1,000,000
* Producer generates synthetic `Event` objects
* Consumer processes each event by accumulating:

  * `order_id`
  * `quantity`
* Producer calls `notify_one()` after each push
* Total wall-clock time includes both producer and consumer work

## Environment

* Platform: Ubuntu bare metal
* Compiler: GCC
* Build flags: `-O3 -std=c++20 -pthread`
* Event size: 32 bytes

## Results

| Run | Time (ms) | ns/event | Events/sec |
| --: | --------: | -------: | ---------: |
|   1 |   149.572 |  149.572 |      6.69M |
|   2 |   123.651 |  123.651 |      8.09M |
|   3 |    90.321 |   90.321 |     11.07M |
|   4 |   168.048 |  168.048 |      5.95M |
|   5 |   142.921 |  142.921 |      7.00M |

Median:

* Time: ~142.9 ms
* Cost: ~142.9 ns/event
* Throughput: ~7.0 million events/sec

## Observations

The benchmark shows significant run-to-run variance because performance depends on thread scheduling, mutex contention, wakeups, and timing between producer and consumer.

This benchmark should not be compared directly with the sequential `std::vector<Event>` scan because the workloads are different.

Its purpose is to provide a baseline for future comparison against Aether's SPSC queue implementation.

## Current Limitations

* `notify_one()` is called for every event
* No batching
* No CPU affinity
* No latency distribution
* No hardware performance counters
* Consumer processing is intentionally trivial

