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

bool OrderBook::cancel(const Event& event){
    if (event.type != EventType::Cancel) return false;
    auto it = orders_.find(event.order_id);
    if (it == orders_.end()) return false;
    auto [price, quantity, side] = it->second;
    if (side == Side::Buy){
        bids_[price] -= quantity;
        if (bids_[price] == 0) bids_.erase(price);
    } else {
        asks_[price] -= quantity;
        if (asks_[price] == 0) asks_.erase(price);
    }
    orders_.erase(it);
    return true;
}

bool OrderBook::trade(const Event& event){
    if (event.type != EventType::Trade) return false;
    if (event.quantity == 0) return false;
    auto it = orders_.find(event.order_id);
    if (it == orders_.end()) return false;
    auto& [price, quantity, side] = it->second;
    if (quantity < event.quantity) return false;
    if (side == Side::Buy){
        bids_[price] -= event.quantity;
        if (bids_[price] == 0) bids_.erase(price);
    } else {
        asks_[price] -= event.quantity;
        if (asks_[price] == 0) asks_.erase(price); 
    }
    quantity = quantity - event.quantity;
    if (quantity == 0){
        orders_.erase(it);
    }
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