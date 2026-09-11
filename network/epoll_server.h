#pragma once

#include "core/event_queue.h"
#include <cstdint>
#include <atomic>
#include <cstddef>
#include <unordered_map>
#include <vector>

class EpollServer {
public:
    EpollServer(uint16_t port, EventQueue& event_queue);
    ~EpollServer();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    uint64_t events_decoded() const {
        return events_decoded_.load(std::memory_order_relaxed);
    }

    uint64_t events_enqueued() const {
        return events_enqueued_.load(std::memory_order_relaxed);
    }

    uint64_t invalid_events() const {
        return invalid_events_.load(std::memory_order_relaxed);
    }

    uint64_t queue_full_count() const {
        return queue_full_count_.load(std::memory_order_relaxed);
    }

    void run();
    void stop();

private:
    struct Connection {
        int fd;
        std::vector<std::byte> buffer;
    };
    uint16_t port_;
    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};
    static constexpr std::size_t FRAME_SIZE = 32;
    uint64_t frames_completed_ = 0;

    uint64_t bytes_received_ = 0;

    std::unordered_map<int, Connection> clients_;

    EventQueue& event_queue_;

    std::atomic<uint64_t> events_decoded_{0};
    std::atomic<uint64_t> events_enqueued_{0};
    std::atomic<uint64_t> invalid_events_{0};
    std::atomic<uint64_t> queue_full_count_{0};

    void setup_listener();
    void setup_epoll();
    void accept_clients();
    bool handle_client(int fd);
    void remove_client(int fd);
    void process_frames(Connection& connection);

    static void set_non_blocking(int fd);
};