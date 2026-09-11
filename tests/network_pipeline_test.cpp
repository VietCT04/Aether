#include "core/event_queue.h"
#include "network/epoll_server.h"
#include "network/wire_protocol.h"

#include <arpa/inet.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace aether::wire;

static void write_u64_be(
    std::vector<std::byte>& frame,
    std::size_t offset,
    uint64_t value
) {
    for (int i = 7; i >= 0; --i) {
        frame[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static void write_u32_be(
    std::vector<std::byte>& frame,
    std::size_t offset,
    uint32_t value
) {
    for (int i = 3; i >= 0; --i) {
        frame[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static std::vector<std::byte> make_add_frame(
    uint64_t sequence,
    uint64_t order_id
) {
    std::vector<std::byte> frame(FRAME_SIZE);

    write_u64_be(frame, SEQUENCE_OFFSET, sequence);
    write_u64_be(frame, ORDER_ID_OFFSET, order_id);
    write_u64_be(frame, PRICE_OFFSET, 100);
    write_u32_be(frame, QUANTITY_OFFSET, 10);

    frame[SIDE_OFFSET] = std::byte{SIDE_BUY};
    frame[TYPE_OFFSET] = std::byte{TYPE_ADD};
    frame[RESERVED_OFFSET] = std::byte{0};
    frame[RESERVED_OFFSET + 1] = std::byte{0};

    return frame;
}

int main() {
    constexpr uint64_t EVENT_COUNT = 1000;

    EventQueue queue;

    std::atomic<bool> worker_running{true};
    std::atomic<uint64_t> events_consumed{0};

    std::thread worker([&] {
        Event event{};

        while (worker_running.load(std::memory_order_acquire)) {
            if (queue.try_pop(event)) {
                events_consumed.fetch_add(
                    1,
                    std::memory_order_relaxed
                );
            } else {
                std::this_thread::yield();
            }
        }

        while (queue.try_pop(event)) {
            events_consumed.fetch_add(
                1,
                std::memory_order_relaxed
            );
        }
    });

    EpollServer server{8080, queue};

    std::thread network([&] {
        server.run();
    });
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_fd != -1);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    assert(connect(
        client_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    ) == 0);

    for (uint64_t i = 0; i < EVENT_COUNT; ++i) {
        auto frame = make_add_frame(i + 1, i + 1);

        std::size_t sent = 0;

        while (sent < frame.size()) {
            ssize_t n = send(
                client_fd,
                frame.data() + sent,
                frame.size() - sent,
                0
            );

            assert(n > 0);
            sent += static_cast<std::size_t>(n);
        }
    }

    close(client_fd);

    while (
        events_consumed.load(std::memory_order_relaxed)
        < EVENT_COUNT
    ) {
        std::this_thread::yield();
    }

    server.stop();
    network.join();

    worker_running.store(
        false,
        std::memory_order_release
    );

    worker.join();

    assert(server.events_decoded() == EVENT_COUNT);
    assert(server.events_enqueued() == EVENT_COUNT);
    assert(events_consumed.load() == EVENT_COUNT);
    assert(server.invalid_events() == 0);

    return 0;
}