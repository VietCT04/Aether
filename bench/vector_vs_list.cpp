#include "../core/event.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>

int main() {
    constexpr std::size_t N = 1'000'000;

    std::vector<Event> events_vector;
    events_vector.reserve(N);

    std::list<Event> events_list;

    for (std::size_t i = 0; i < N; i++) {
        Event event{
            .seq = i,
            .order_id = i,
            .price = static_cast<int64_t>(100000 + i % 1000),
            .quantity = static_cast<uint32_t>(1 + i % 100),
            .type = EventType::Add
        };

        events_vector.push_back(event);
        events_list.push_back(event);
    }

    uint64_t vector_checksum = 0;

    auto vector_start = std::chrono::steady_clock::now();

    for (const auto& event : events_vector)
        vector_checksum += event.order_id;

    auto vector_end = std::chrono::steady_clock::now();

    uint64_t list_checksum = 0;

    auto list_start = std::chrono::steady_clock::now();

    for (const auto& event : events_list)
        list_checksum += event.order_id;

    auto list_end = std::chrono::steady_clock::now();

    auto vector_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            vector_end - vector_start
        ).count();

    auto list_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            list_end - list_start
        ).count();

    std::cout << "vector checksum: " << vector_checksum << '\n';
    std::cout << "vector time: " << vector_ns / 1e6 << " ms\n";
    std::cout << "vector ns/event: "
              << static_cast<double>(vector_ns) / N << '\n';

    std::cout << "list checksum: " << list_checksum << '\n';
    std::cout << "list time: " << list_ns / 1e6 << " ms\n";
    std::cout << "list ns/event: "
              << static_cast<double>(list_ns) / N << '\n';

    std::cout << "list/vector ratio: "
              << static_cast<double>(list_ns) / vector_ns << "x\n";
}