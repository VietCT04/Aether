#pragma once

#include "core/event.h"

#include <cstddef>
#include <optional>
#include <span>

std::optional<Event> decode_event(
    std::span<const std::byte> frame
);