#pragma once

#include <cstddef>
#include <cstdint>

namespace aether::wire {

constexpr std::size_t FRAME_SIZE = 32;

constexpr std::size_t SEQUENCE_OFFSET = 0;
constexpr std::size_t ORDER_ID_OFFSET = 8;
constexpr std::size_t PRICE_OFFSET = 16;
constexpr std::size_t QUANTITY_OFFSET = 24;
constexpr std::size_t SIDE_OFFSET = 28;
constexpr std::size_t TYPE_OFFSET = 29;
constexpr std::size_t RESERVED_OFFSET = 30;

constexpr uint8_t SIDE_BUY = 0;
constexpr uint8_t SIDE_SELL = 1;

constexpr uint8_t TYPE_ADD = 0;
constexpr uint8_t TYPE_CANCEL = 1;
constexpr uint8_t TYPE_TRADE = 2;

}