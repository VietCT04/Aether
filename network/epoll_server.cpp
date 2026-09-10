#include "epoll_server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/epoll.h>
#include <cerrno>
#include <array>
#include <iostream>

void EpollServer::stop() {
    running_.store(false);
}

EpollServer::~EpollServer() {
    for (int fd : clients_) {
        close(fd);
    }

    if (listen_fd_ != -1) {
        close(listen_fd_);
    }

    if (epoll_fd_ != -1) {
        close(epoll_fd_);
    }
}

EpollServer::EpollServer(uint16_t port)
    : port_(port) {
    try {
        setup_listener();
        setup_epoll();
    } catch (...) {
        if (epoll_fd_ != -1) {
            close(epoll_fd_);
        }

        if (listen_fd_ != -1) {
            close(listen_fd_);
        }

        throw;
    }
}

void EpollServer::remove_client(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    clients_.erase(fd);
    close(fd);
}

bool EpollServer::handle_client(int fd) {
    std::array<char, 4096> buffer;

    while (true) {
        ssize_t n = recv(fd, buffer.data(), buffer.size(), 0);

        if (n > 0) {
            bytes_received_ += static_cast<uint64_t>(n);
            std::cout << "fd " << fd
                    << " received " << n
                    << " bytes"
                    << " | total = " << bytes_received_
                    << '\n';
            continue;
        }

        if (n == 0) {
            return false;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }

        return false;
    }
}

void EpollServer::setup_epoll() {
    epoll_fd_ = epoll_create1(0);

    if (epoll_fd_ == -1) {
        throw std::runtime_error("epoll_create1 failed");
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listen_fd_;

    if (epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_ADD,
            listen_fd_,
            &event
        ) == -1) {
        throw std::runtime_error("epoll_ctl ADD listener failed");
    }
}
void EpollServer::set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void EpollServer::accept_clients() {
    while (true) {
        int client_fd = accept(listen_fd_, nullptr, nullptr);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            throw std::runtime_error("accept failed");
        }

        set_non_blocking(client_fd);

        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = client_fd;

        if (epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_ADD,
                client_fd,
                &event
            ) == -1) {
            close(client_fd);
            throw std::runtime_error("epoll_ctl ADD client failed");
        }

        clients_.insert(client_fd);
    }
}

void EpollServer::setup_listener() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        throw std::runtime_error("socket failed");
    }

    int opt = 1;
    if (setsockopt(
            listen_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) == -1) {
        throw std::runtime_error("setsockopt failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        ) == -1) {
        throw std::runtime_error("bind failed");
    }

    if (listen(listen_fd_, 128) == -1) {
        throw std::runtime_error("listen failed");
    }

    set_non_blocking(listen_fd_);
}

void EpollServer::run() {
    running_.store(true);

    constexpr int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];

    while (running_.load()) {
        int n = epoll_wait(
            epoll_fd_,
            events,
            MAX_EVENTS,
            100
        );

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("epoll_wait failed");
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;

            if (fd == listen_fd_) {
                if (flags & EPOLLIN) {
                    accept_clients();
                }

                if (flags & (EPOLLERR | EPOLLHUP)) {
                    throw std::runtime_error("listener epoll error");
                }

                continue;
            }

            bool alive = true;

            if (flags & EPOLLIN) {
                alive = handle_client(fd);
            }

            if (!alive) {
                remove_client(fd);
                continue;
            }

            if (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                remove_client(fd);
            }
        }
    }
}