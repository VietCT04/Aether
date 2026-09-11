#pragma once

#include <cstdint>
#include <atomic>
#include <cstddef>
#include <unordered_map>
#include <vector>

class EpollServer {
public:
    explicit EpollServer(uint16_t port);
    ~EpollServer();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

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

    void setup_listener();
    void setup_epoll();
    void accept_clients();
    bool handle_client(int fd);
    void remove_client(int fd);
    void process_frames(Connection& connection);

    static void set_non_blocking(int fd);
};