#pragma once
#include <cstdint>

enum class Side : uint8_t {
    Buy,
    Sell
};

enum class EventType : uint8_t {
    Add,
    Cancel,
    Trade
};

struct Event {
    uint64_t sequence;
    uint64_t order_id;
    int64_t price;
    uint32_t quantity;
    Side side;
    EventType type;
};
