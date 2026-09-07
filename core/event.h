#pragma once
#include <cstdint>

enum class EventType : uint8_t
{
    Add,
    Cancel,
    Trade
};

struct Event
{
    uint64_t seq;
    uint64_t order_id;
    int64_t price;
    uint32_t quantity;
    EventType type;
};