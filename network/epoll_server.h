#pragma once

#include <cstdint>
#include <unordered_set>
#include <atomic>
#include <cstdint>

class EpollServer {
public:
    explicit EpollServer(uint16_t port);
    ~EpollServer();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void run();
    void stop();

private:
    uint16_t port_;
    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};

    uint64_t bytes_received_ = 0;

    std::unordered_set<int> clients_;

    void setup_listener();
    void setup_epoll();
    void accept_clients();
    bool handle_client(int fd);
    void remove_client(int fd);

    static void set_non_blocking(int fd);
};