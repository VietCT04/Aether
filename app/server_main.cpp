#include "network/epoll_server.h"

#include <exception>
#include <iostream>

int main() {
    try {
        EpollServer server{8080};
        std::cout << "Aether listening on port 8080\n";
        server.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}