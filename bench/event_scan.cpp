#include "../core/event.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    constexpr std::size_t N = 1'000'000;

    std::vector<Event> events;
    events.reserve(N);

    for (std::size_t i = 0; i < N; ++i) {
        events.push_back(Event{
            .seq = i,
            .order_id = i,
            .price = static_cast<int64_t>(100000 + i % 1000),
            .quantity = static_cast<uint32_t>(1 + i % 100),
            .type = EventType::Add
        });
    }

    std::cout << "sizeof(Event): " << sizeof(Event) << '\n';
    std::cout << "events: " << events.size() << '\n';

    uint64_t checksum = 0;

    auto start = std::chrono::steady_clock::now();

    for (const auto& event : events) {
        checksum += event.order_id;
        checksum += event.quantity;
    }

    auto end = std::chrono::steady_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start
    ).count();

    std::cout << "checksum: " << checksum << '\n';
    std::cout << "time: " << ns / 1e6 << " ms\n";
    std::cout << "ns/event: "
              << static_cast<double>(ns) / N
              << '\n';
}
