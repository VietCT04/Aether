#include "core/event_queue.h"
#include "market/order_book.h"
#include "network/epoll_server.h"
#include "network/wire_protocol.h"

#include <arpa/inet.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace aether::wire;

static constexpr uint64_t EVENT_COUNT = 1000000;

static constexpr int CLIENT_CPU  = 2;
static constexpr int NETWORK_CPU = 4;
static constexpr int WORKER_CPU  = 6;
static constexpr uint16_t PORT = 18080;

struct RunResult {
    double elapsed_ms;
    double events_per_second;
    double ns_per_event;
    uint64_t queue_full_count;
    uint64_t bytes_received;
};

static void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void pin_current_thread(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    int rc = pthread_setaffinity_np(
        pthread_self(),
        sizeof(set),
        &set
    );

    if (rc != 0) {
        std::cerr
            << "Failed to pin thread to CPU "
            << cpu
            << '\n';

        std::abort();
    }
}

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

static std::vector<std::byte> generate_workload() {
    std::vector<std::byte> stream;
    stream.reserve(EVENT_COUNT * FRAME_SIZE);

    std::mt19937_64 rng{42};

    std::uniform_int_distribution<int64_t> price_dist{
        10000,
        11000
    };

    std::uniform_int_distribution<uint32_t> quantity_dist{
        2,
        100
    };

    std::uniform_int_distribution<int> side_dist{
        0,
        1
    };

    uint64_t sequence = 1;
    uint64_t next_order_id = 1;

    for (
        uint64_t block = 0;
        block < EVENT_COUNT / 10;
        ++block
    ) {
        uint64_t ids[5];
        uint8_t sides[5];

        for (int i = 0; i < 5; ++i) {
            ids[i] = next_order_id++;

            sides[i] =
                side_dist(rng) == 0
                    ? SIDE_BUY
                    : SIDE_SELL;

            append_frame(
                stream,
                sequence++,
                ids[i],
                price_dist(rng),
                quantity_dist(rng),
                sides[i],
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
                sides[i],
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
                sides[i],
                TYPE_CANCEL
            );
        }
    }

    require(
        stream.size() == EVENT_COUNT * FRAME_SIZE,
        "invalid workload size"
    );

    return stream;
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

        if (n <= 0) {
            throw std::runtime_error("send failed");
        }

        sent += static_cast<std::size_t>(n);
    }
}

static RunResult run_once(
    const std::vector<std::byte>& stream
) {
    EventQueue queue;
    OrderBook order_book;

    std::atomic<bool> worker_running{true};
    std::atomic<bool> worker_ready{false};
    std::atomic<bool> network_ready{false};
    std::atomic<uint64_t> events_processed{0};
    std::atomic<uint64_t> events_rejected{0};

    std::thread worker([&] {
        pin_current_thread(WORKER_CPU);

        worker_ready.store(
            true,
            std::memory_order_release
        );

        Event event{};

        while (
            worker_running.load(std::memory_order_acquire)
        ) {
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

    EpollServer server{PORT, queue};

    std::exception_ptr network_error;

    std::thread network([&] {
        try {
            pin_current_thread(NETWORK_CPU);

            network_ready.store(
                true,
                std::memory_order_release
            );

            server.run();
        } catch (...) {
            network_error = std::current_exception();
        }
    });

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    require(
        client_fd != -1,
        "client socket failed"
    );

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    require(
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &addr.sin_addr
        ) == 1,
        "inet_pton failed"
    );

    while (
        !worker_ready.load(std::memory_order_acquire) ||
        !network_ready.load(std::memory_order_acquire)
    ) {
        std::this_thread::yield();
    }

    require(
        connect(
            client_fd,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        ) == 0,
        "connect failed"
    );

    auto start =
        std::chrono::steady_clock::now();

    send_all(client_fd, stream);

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

    auto end =
        std::chrono::steady_clock::now();

    close(client_fd);

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

    if (network_error) {
        std::rethrow_exception(network_error);
    }

    require(completed, "benchmark timed out");

    require(
        server.bytes_received()
            == EVENT_COUNT * FRAME_SIZE,
        "bytes_received mismatch"
    );

    require(
        server.events_decoded() == EVENT_COUNT,
        "events_decoded mismatch"
    );

    require(
        server.events_enqueued() == EVENT_COUNT,
        "events_enqueued mismatch"
    );

    require(
        events_processed.load() == EVENT_COUNT,
        "events_processed mismatch"
    );

    require(
        events_rejected.load() == 0,
        "OrderBook rejected event"
    );

    require(
        server.invalid_events() == 0,
        "invalid event detected"
    );

    double elapsed_ns =
        std::chrono::duration<double, std::nano>(
            end - start
        ).count();

    double elapsed_ms =
        elapsed_ns / 1e6;

    double events_per_second =
        static_cast<double>(EVENT_COUNT)
        / (elapsed_ns / 1e9);

    double ns_per_event =
        elapsed_ns
        / static_cast<double>(EVENT_COUNT);

    return {
        elapsed_ms,
        events_per_second,
        ns_per_event,
        server.queue_full_count(),
        server.bytes_received()
    };
}

int main() {
    pin_current_thread(CLIENT_CPU);
    
    try {
        std::cout
            << "Generating e2e-v1 workload...\n";

        auto stream = generate_workload();

        std::cout
            << "Events: "
            << EVENT_COUNT
            << '\n'
            << "Bytes: "
            << stream.size()
            << '\n'
            << "Client CPU: "
            << CLIENT_CPU
            << '\n'
            << "Network CPU: "
            << NETWORK_CPU
            << '\n'
            << "Worker CPU: "
            << WORKER_CPU
            << "\n\n";

        std::cout << "Warm-up\n";

        RunResult warmup =
            run_once(stream);

        std::cout
            << "  "
            << warmup.ns_per_event
            << " ns/event\n\n";

        constexpr int RUNS = 5;

        std::vector<RunResult> results;
        results.reserve(RUNS);

        for (int i = 0; i < RUNS; ++i) {
            RunResult result =
                run_once(stream);

            results.push_back(result);

            std::cout
                << "Run "
                << i + 1
                << ": "
                << std::fixed
                << std::setprecision(3)
                << result.elapsed_ms
                << " ms | "
                << result.events_per_second
                << " events/s | "
                << result.ns_per_event
                << " ns/event | queue_full="
                << result.queue_full_count
                << '\n';
        }

        std::vector<double> ns_values;

        for (const auto& result : results) {
            ns_values.push_back(
                result.ns_per_event
            );
        }

        std::sort(
            ns_values.begin(),
            ns_values.end()
        );

        double min_ns =
            ns_values.front();

        double median_ns =
            ns_values[RUNS / 2];

        double max_ns =
            ns_values.back();

        double median_eps =
            1e9 / median_ns;

        std::cout
            << "\nSummary\n"
            << "min_ns_per_event="
            << min_ns
            << '\n'
            << "median_ns_per_event="
            << median_ns
            << '\n'
            << "max_ns_per_event="
            << max_ns
            << '\n'
            << "median_events_per_second="
            << median_eps
            << '\n';

    } catch (const std::exception& e) {
        std::cerr
            << "Benchmark failed: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}
