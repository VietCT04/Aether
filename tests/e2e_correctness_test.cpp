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

static void send_chunked(
    int fd,
    const std::vector<std::byte>& stream
) {
    std::vector<std::size_t> chunks{
        1, 3, 17, 5, 64, 2, 11, 7
    };

    std::size_t offset = 0;
    std::size_t chunk_index = 0;

    while (offset < stream.size()) {
        std::size_t chunk =
            chunks[chunk_index % chunks.size()];

        std::size_t remaining =
            stream.size() - offset;

        if (chunk > remaining) {
            chunk = remaining;
        }

        std::size_t sent = 0;

        while (sent < chunk) {
            ssize_t n = send(
                fd,
                stream.data() + offset + sent,
                chunk - sent,
                0
            );

            assert(n > 0);
            sent += static_cast<std::size_t>(n);
        }

        offset += chunk;
        ++chunk_index;
    }
}

int main() {
    constexpr uint64_t EVENT_COUNT = 5;

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

    std::vector<std::byte> stream;
    stream.reserve(EVENT_COUNT * FRAME_SIZE);

    append_frame(
        stream,
        1,
        1,
        100,
        10,
        SIDE_BUY,
        TYPE_ADD
    );

    append_frame(
        stream,
        2,
        2,
        101,
        5,
        SIDE_BUY,
        TYPE_ADD
    );

    append_frame(
        stream,
        3,
        3,
        103,
        7,
        SIDE_SELL,
        TYPE_ADD
    );

    append_frame(
        stream,
        4,
        2,
        0,
        5,
        SIDE_BUY,
        TYPE_TRADE
    );

    append_frame(
        stream,
        5,
        3,
        0,
        0,
        SIDE_SELL,
        TYPE_CANCEL
    );

    assert(
        stream.size()
        == EVENT_COUNT * FRAME_SIZE
    );

    send_chunked(client_fd, stream);

    close(client_fd);

    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

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
    assert(*bid == 100);
    assert(!ask.has_value());

    return 0;
}