#include "../core/event.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

int main() {
    constexpr uint64_t N = 1'000'000;

    std::queue<Event> q;
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    uint64_t checksum = 0;

    auto start = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (uint64_t i = 0; i < N; i++) {
            Event event{
                .seq = i,
                .order_id = i,
                .price = static_cast<int64_t>(100000 + i % 1000),
                .quantity = static_cast<uint32_t>(1 + i % 100),
                .type = EventType::Add
            };

            {
                std::lock_guard<std::mutex> lock(m);
                q.push(event);
            }

            cv.notify_one();
        }

        {
            std::lock_guard<std::mutex> lock(m);
            done = true;
        }

        cv.notify_one();
    });

    std::thread consumer([&] {
        std::unique_lock<std::mutex> lock(m);

        while (true) {
            cv.wait(lock, [&] {
                return !q.empty() || done;
            });

            if (q.empty() && done) {
                break;
            }

            Event event = q.front();
            q.pop();

            lock.unlock();

            checksum += event.order_id;
            checksum += event.quantity;

            lock.lock();
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::steady_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start
    ).count();

    std::cout << "events: " << N << '\n';
    std::cout << "checksum: " << checksum << '\n';
    std::cout << "time: " << ns / 1e6 << " ms\n";
    std::cout << "ns/event: "
              << static_cast<double>(ns) / N << '\n';
    std::cout << "events/sec: "
              << static_cast<double>(N) * 1e9 / ns << '\n';
}