#pragma once
#include "../core/event.h"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

class OrderBook {
public:
    bool add(const Event& event);
    bool cancel(const Event& event);
    bool trade(const Event& event);

    std::optional<int64_t> best_bid() const;
    std::optional<int64_t> best_ask() const;

private:
    struct Order {
        int64_t price;
        uint32_t quantity;
        Side side;
    };

    std::unordered_map<uint64_t, Order> orders_;
    std::map<int64_t, uint64_t, std::greater<int64_t>> bids_;
    std::map<int64_t, uint64_t> asks_;
};