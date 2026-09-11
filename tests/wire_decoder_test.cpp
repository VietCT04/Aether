#include "network/wire_decoder.h"
#include "network/wire_protocol.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

using namespace aether::wire;

static void write_u64_be(
    std::array<std::byte, FRAME_SIZE>& frame,
    std::size_t offset,
    uint64_t value
) {
    for (int i = 7; i >= 0; --i) {
        frame[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static void write_u32_be(
    std::array<std::byte, FRAME_SIZE>& frame,
    std::size_t offset,
    uint32_t value
) {
    for (int i = 3; i >= 0; --i) {
        frame[offset + i] = std::byte(value & 0xFF);
        value >>= 8;
    }
}

static std::array<std::byte, FRAME_SIZE> make_frame(
    uint64_t sequence,
    uint64_t order_id,
    int64_t price,
    uint32_t quantity,
    uint8_t side,
    uint8_t type
) {
    std::array<std::byte, FRAME_SIZE> frame{};

    write_u64_be(frame, SEQUENCE_OFFSET, sequence);
    write_u64_be(
        frame,
        ORDER_ID_OFFSET,
        static_cast<uint64_t>(order_id)
    );
    write_u64_be(
        frame,
        PRICE_OFFSET,
        static_cast<uint64_t>(price)
    );
    write_u32_be(frame, QUANTITY_OFFSET, quantity);

    frame[SIDE_OFFSET] = std::byte(side);
    frame[TYPE_OFFSET] = std::byte(type);

    frame[RESERVED_OFFSET] = std::byte{0};
    frame[RESERVED_OFFSET + 1] = std::byte{0};

    return frame;
}

int main() {
    {
        auto frame = make_frame(
            1,
            42,
            100,
            10,
            SIDE_BUY,
            TYPE_ADD
        );

        auto event = decode_event(frame);

        assert(event.has_value());
        assert(event->sequence == 1);
        assert(event->order_id == 42);
        assert(event->price == 100);
        assert(event->quantity == 10);
        assert(event->side == Side::Buy);
        assert(event->type == EventType::Add);
    }
    {
        auto frame = make_frame(
            2,
            100,
            105,
            0,
            SIDE_SELL,
            TYPE_CANCEL
        );

        auto event = decode_event(frame);

        assert(event.has_value());
        assert(event->type == EventType::Cancel);
        assert(event->quantity == 0);
    }
    {
    auto frame = make_frame(
            3,
            200,
            101,
            5,
            SIDE_BUY,
            TYPE_TRADE
        );

        auto event = decode_event(frame);

        assert(event.has_value());
        assert(event->type == EventType::Trade);
        assert(event->quantity == 5);
    }
    {
        auto frame = make_frame(
            4,
            300,
            100,
            10,
            9,
            TYPE_ADD
        );

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    {
        auto frame = make_frame(
            5,
            400,
            100,
            10,
            SIDE_BUY,
            9
        );

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    {
        auto frame = make_frame(
            6,
            500,
            100,
            10,
            SIDE_BUY,
            TYPE_ADD
        );

        frame[RESERVED_OFFSET] = std::byte{1};

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    {
        auto frame = make_frame(
            7,
            600,
            100,
            0,
            SIDE_BUY,
            TYPE_ADD
        );

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    {
        auto frame = make_frame(
            8,
            700,
            100,
            0,
            SIDE_BUY,
            TYPE_TRADE
        );

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    {
        auto frame = make_frame(
            9,
            800,
            100,
            10,
            SIDE_BUY,
            TYPE_ADD
        );

        std::span<const std::byte> truncated(
            frame.data(),
            31
        );

        auto event = decode_event(truncated);

        assert(!event.has_value());
    }
    {
        std::array<std::byte, 33> frame{};

        auto event = decode_event(frame);

        assert(!event.has_value());
    }
    
    return 0;
}