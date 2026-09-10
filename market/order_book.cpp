#include "order_book.h"

bool OrderBook::add(const Event& event) {
    if (event.type != EventType::Add) return false;
    if (event.quantity == 0) return false;
    if (orders_.find(event.order_id) != orders_.end()) return false;
    orders_[event.order_id] = Order{event.price, event.quantity, event.side};
    if (event.side == Side::Buy) bids_[event.price] += event.quantity;
    else asks_[event.price] += event.quantity;
    return true;
}

bool OrderBook::cancel(const Event& event) {
    if (event.type != EventType::Cancel) return false;

    auto it = orders_.find(event.order_id);
    if (it == orders_.end()) return false;

    auto [price, quantity, side] = it->second;

    if (side == Side::Buy) {
        auto level = bids_.find(price);
        if (level == bids_.end() || level->second < quantity) return false;

        level->second -= quantity;
        if (level->second == 0) bids_.erase(level);
    } else {
        auto level = asks_.find(price);
        if (level == asks_.end() || level->second < quantity) return false;

        level->second -= quantity;
        if (level->second == 0) asks_.erase(level);
    }

    orders_.erase(it);
    return true;
}

bool OrderBook::trade(const Event& event) {
    if (event.type != EventType::Trade) return false;
    if (event.quantity == 0) return false;

    auto it = orders_.find(event.order_id);
    if (it == orders_.end()) return false;

    auto& [price, quantity, side] = it->second;
    if (quantity < event.quantity) return false;

    if (side == Side::Buy) {
        auto level = bids_.find(price);
        if (level == bids_.end() || level->second < event.quantity) return false;

        level->second -= event.quantity;
        if (level->second == 0) bids_.erase(level);
    } else {
        auto level = asks_.find(price);
        if (level == asks_.end() || level->second < event.quantity) return false;

        level->second -= event.quantity;
        if (level->second == 0) asks_.erase(level);
    }

    quantity -= event.quantity;
    if (quantity == 0) orders_.erase(it);

    return true;
}

std::optional<int64_t> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<int64_t> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}