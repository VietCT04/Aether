#include "wire_decoder.h"
#include "wire_protocol.h"

#include <cstdint>

static uint64_t read_u64_be(
    std::span<const std::byte> data,
    std::size_t offset
) {
    uint64_t value = 0;

    for (int i = 0; i < 8; ++i) {
        value <<= 8;
        value |= std::to_integer<uint8_t>(data[offset + i]);
    }

    return value;
}

static uint32_t read_u32_be(
    std::span<const std::byte> data,
    std::size_t offset
) {
    uint32_t value = 0;

    for (int i = 0; i < 4; ++i) {
        value <<= 8;
        value |= std::to_integer<uint8_t>(data[offset + i]);
    }

    return value;
}

std::optional<Event> decode_event(
    std::span<const std::byte> frame
) {
    using namespace aether::wire;

    if (frame.size() != FRAME_SIZE) {
        return std::nullopt;
    }

    if (std::to_integer<uint8_t>(frame[RESERVED_OFFSET]) != 0 ||
        std::to_integer<uint8_t>(frame[RESERVED_OFFSET + 1]) != 0) {
        return std::nullopt;
    }

    uint64_t sequence = read_u64_be(frame, SEQUENCE_OFFSET);
    uint64_t order_id = read_u64_be(frame, ORDER_ID_OFFSET);
    int64_t price = static_cast<int64_t>(
        read_u64_be(frame, PRICE_OFFSET)
    );
    uint32_t quantity = read_u32_be(frame, QUANTITY_OFFSET);

    uint8_t side_raw =
        std::to_integer<uint8_t>(frame[SIDE_OFFSET]);

    uint8_t type_raw =
        std::to_integer<uint8_t>(frame[TYPE_OFFSET]);

    Side side;

    if (side_raw == SIDE_BUY) {
        side = Side::Buy;
    } else if (side_raw == SIDE_SELL) {
        side = Side::Sell;
    } else {
        return std::nullopt;
    }

    EventType type;

    if (type_raw == TYPE_ADD) {
        type = EventType::Add;
    } else if (type_raw == TYPE_CANCEL) {
        type = EventType::Cancel;
    } else if (type_raw == TYPE_TRADE) {
        type = EventType::Trade;
    } else {
        return std::nullopt;
    }

    if ((type == EventType::Add ||
         type == EventType::Trade) &&
        quantity == 0) {
        return std::nullopt;
    }

    return Event{
        .sequence = sequence,
        .order_id = order_id,
        .price = price,
        .quantity = quantity,
        .side = side,
        .type = type
    };
}