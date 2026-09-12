#include "core/event_queue.h"
#include "market/order_book.h"
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
    std::vector<std::byte>& data,
    std::size_t offset,
    uint64_t value
) {
    for (int i = 7; i >= 0; --i) {
        data[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static void write_u32_be(
    std::vector<std::byte>& data,
    std::size_t offset,
    uint32_t value
) {
    for (int i = 3; i >= 0; --i) {
        data[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static void append_frame(
    std::vector<std::byte>& stream,
    uint64_t sequence,
    uint64_t order_id,
    int64_t price,
    uint32_t quantity,
    uint8_t side,
    uint8_t type
) {
    std::size_t base = stream.size();
    stream.resize(base + FRAME_SIZE);

    write_u64_be(
        stream,
        base + SEQUENCE_OFFSET,
        sequence
    );

    write_u64_be(
        stream,
        base + ORDER_ID_OFFSET,
        order_id
    );

    write_u64_be(
        stream,
        base + PRICE_OFFSET,
        static_cast<uint64_t>(price)
    );

    write_u32_be(
        stream,
        base + QUANTITY_OFFSET,
        quantity
    );

    stream[base + SIDE_OFFSET] = std::byte{side};
    stream[base + TYPE_OFFSET] = std::byte{type};
    stream[base + RESERVED_OFFSET] = std::byte{0};
    stream[base + RESERVED_OFFSET + 1] = std::byte{0};
}

static void send_all(
    int fd,
    const std::vector<std::byte>& stream
) {
    std::size_t sent = 0;

    while (sent < stream.size()) {
        ssize_t n = send(
            fd,
            stream.data() + sent,
            stream.size() - sent,
            0
        );

        assert(n > 0);
        sent += static_cast<std::size_t>(n);
    }
}

int main() {
    constexpr uint64_t EVENT_COUNT = 1000000;

    static_assert(EVENT_COUNT % 10 == 0);

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

    std::thread network([&] {
        server.run();
    });

    std::vector<std::byte> stream;
    stream.reserve(EVENT_COUNT * FRAME_SIZE);

    uint64_t sequence = 1;
    uint64_t next_order_id = 1;

    for (
        uint64_t block = 0;
        block < EVENT_COUNT / 10;
        ++block
    ) {
        uint64_t ids[5];

        for (int i = 0; i < 5; ++i) {
            ids[i] = next_order_id++;

            append_frame(
                stream,
                sequence++,
                ids[i],
                100 + i,
                10,
                SIDE_BUY,
                TYPE_ADD
            );
        }

        for (int i = 0; i < 3; ++i) {
            append_frame(
                stream,
                sequence++,
                ids[i],
                0,
                1,
                SIDE_BUY,
                TYPE_TRADE
            );
        }

        for (int i = 3; i < 5; ++i) {
            append_frame(
                stream,
                sequence++,
                ids[i],
                0,
                0,
                SIDE_BUY,
                TYPE_CANCEL
            );
        }
    }

    assert(
        stream.size()
        == EVENT_COUNT * FRAME_SIZE
    );

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_fd != -1);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);

    assert(
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &addr.sin_addr
        ) == 1
    );

    assert(
        connect(
            client_fd,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        ) == 0
    );

    send_all(client_fd, stream);

    close(client_fd);

    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(30);

    while (
        events_processed.load(std::memory_order_relaxed) +
            events_rejected.load(std::memory_order_relaxed)
            < EVENT_COUNT &&
        std::chrono::steady_clock::now() < deadline
    ) {
        std::this_thread::yield();
    }

    bool completed =
        events_processed.load(std::memory_order_relaxed) +
        events_rejected.load(std::memory_order_relaxed)
        == EVENT_COUNT;

    server.stop();
    network.join();

    worker_running.store(
        false,
        std::memory_order_release
    );

    worker.join();

    assert(completed);

    assert(
        server.bytes_received()
        == EVENT_COUNT * FRAME_SIZE
    );

    assert(server.events_decoded() == EVENT_COUNT);
    assert(server.events_enqueued() == EVENT_COUNT);
    assert(server.invalid_events() == 0);

    assert(events_processed.load() == EVENT_COUNT);
    assert(events_rejected.load() == 0);

    auto bid = order_book.best_bid();
    auto ask = order_book.best_ask();

    assert(bid.has_value());
    assert(*bid == 102);
    assert(!ask.has_value());

    return 0;
}