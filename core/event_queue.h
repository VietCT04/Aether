#pragma once

#include "core/spsc_queue.h"

#include <cstddef>

inline constexpr std::size_t EVENT_QUEUE_CAPACITY = 65536;

using EventQueue = SPSCQueue<EVENT_QUEUE_CAPACITY>;