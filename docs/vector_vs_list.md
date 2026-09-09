# Vector vs List Traversal

## Hypothesis

`std::vector<Event>` should outperform `std::list<Event>` for sequential traversal because vector stores elements contiguously, improving spatial locality, cache-line utilization, hardware prefetching, and memory-level parallelism.

`std::list<Event>` introduces pointer chasing, extra per-node metadata, separate allocations, poorer locality, and potentially higher TLB pressure.

## Workload

- 1,000,000 `Event` objects
- Same event data in both containers
- Sequential traversal
- Work per event:

```cpp
checksum += event.order_id;
````

* Compiler: GCC
* Flags: `-O3 -std=c++20`
* Platform: Ubuntu bare metal

## Results

| Container            | Average Time | Approx. ns/event |
| -------------------- | -----------: | ---------------: |
| `std::vector<Event>` |     2.096 ms |          2.10 ns |
| `std::list<Event>`   |     4.588 ms |          4.59 ns |

`std::list` was approximately **2.19x slower** than `std::vector` for this workload.

## Explanation

Both traversals are `O(N)`, but their hardware behavior is different. Vector provides contiguous memory access, allowing efficient cache-line use and predictable hardware prefetching. List traversal follows dependent pointers between separately allocated nodes, reducing locality and making it harder for the CPU to hide memory latency.

The measured difference reflects the combined effects of memory layout, pointer chasing, allocation layout, cache behavior, and TLB behavior; this benchmark does not isolate each factor individually.

If per-event computation becomes much more expensive, the relative difference between the containers is expected to shrink because CPU computation would dominate total execution time.