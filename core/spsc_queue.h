#pragma once

#include "core/event.h"

#include <array>
#include <atomic>
#include <cstddef>

template <std::size_t Capacity>
class SPSCQueue {
public:
    bool try_push(const Event& event) {
        std::size_t write =
            write_index_.load(std::memory_order_relaxed);

        std::size_t next =
            (write + 1) % STORAGE_SIZE;

        if (next ==
            read_index_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[write] = event;

        write_index_.store(
            next,
            std::memory_order_release
        );

        return true;
    }
    bool try_pop(Event& event) {
        std::size_t read =
            read_index_.load(std::memory_order_relaxed);

        if (read ==
            write_index_.load(std::memory_order_acquire)) {
            return false;
        }

        event = buffer_[read];

        std::size_t next =
            (read + 1) % STORAGE_SIZE;

        read_index_.store(
            next,
            std::memory_order_release
        );

        return true;
    }

private:
    static constexpr std::size_t STORAGE_SIZE = Capacity + 1;

    std::array<Event, STORAGE_SIZE> buffer_;
    std::atomic<std::size_t> write_index_{0};
    std::atomic<std::size_t> read_index_{0};
};