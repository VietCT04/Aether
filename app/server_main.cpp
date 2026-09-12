#include "network/epoll_server.h"
#include "core/event_queue.h"
#include "market/order_book.h"

#include <atomic>
#include <thread>
#include <exception>
#include <iostream>

int main() {
    try {
        EventQueue queue;
        OrderBook order_book;

        std::atomic<bool> worker_running{true};
        std::atomic<uint64_t> events_processed{0};
        std::atomic<uint64_t> events_rejected{0};

        std::thread worker([&] {
            Event event{};

            while (worker_running.load(std::memory_order_acquire)) {
                if (queue.try_pop(event)) {
                    if (order_book.apply(event)) {
                        events_processed.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    } else {
                        events_rejected.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                } else {
                    std::this_thread::yield();
                }
            }

            while (queue.try_pop(event)) {
                if (order_book.apply(event)) {
                    events_processed.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                } else {
                    events_rejected.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            }
        });

        EpollServer server{8080, queue};

        std::cout << "Aether listening on port 8080\n";

        server.run();

        worker_running.store(
            false,
            std::memory_order_release
        );

        worker.join();

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}