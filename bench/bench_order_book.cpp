#include "../market/order_book.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

int main() {
    constexpr uint64_t N = 10'000'000;
    constexpr uint64_t BLOCKS = N / 10;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(9000, 11000);
    std::uniform_int_distribution<uint32_t> quantity_dist(2, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    std::vector<Event> events;
    events.reserve(N);

    uint64_t sequence = 1;
    uint64_t order_id = 1;

    for (uint64_t block = 0; block < BLOCKS; ++block) {
        uint64_t ids[5];

        for (int i = 0; i < 5; ++i) {
            ids[i] = order_id++;

            events.push_back(Event{
                .sequence = sequence++,
                .order_id = ids[i],
                .price = price_dist(rng),
                .quantity = quantity_dist(rng),
                .side = side_dist(rng) == 0 ? Side::Buy : Side::Sell,
                .type = EventType::Add
            });
        }

        for (int i = 0; i < 3; ++i) {
            events.push_back(Event{
                .sequence = sequence++,
                .order_id = ids[i],
                .price = 0,
                .quantity = 1,
                .side = Side::Buy,
                .type = EventType::Trade
            });
        }

        for (int i = 3; i < 5; ++i) {
            events.push_back(Event{
                .sequence = sequence++,
                .order_id = ids[i],
                .price = 0,
                .quantity = 0,
                .side = Side::Buy,
                .type = EventType::Cancel
            });
        }
    }

    OrderBook book;

    auto start = std::chrono::steady_clock::now();

    uint64_t processed = 0;

    for (const Event& event : events) {
        if (book.apply(event)) ++processed;
    }

    auto end = std::chrono::steady_clock::now();

    double seconds =
        std::chrono::duration<double>(end - start).count();

    double events_per_second = processed / seconds;
    double ns_per_event = seconds * 1e9 / processed;

    std::cout << "events: " << processed << '\n';
    std::cout << "time: " << seconds << " s\n";
    std::cout << "throughput: " << events_per_second << " events/s\n";
    std::cout << "latency: " << ns_per_event << " ns/event\n";
}