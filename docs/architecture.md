# Aether v0.1 Architecture

## Overview

Aether is a high-performance C++ event-processing engine for ordered market-data workloads, designed to explore predictable latency, deterministic state processing, and systems-level performance.

The v0.1 processing model is:

```text
event ingestion
      ↓
event representation
      ↓
event transport
      ↓
market state
```

The first stateful subsystem is the OrderBook.

## Event Model

```cpp
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
```

`Event` represents an input message. Persistent order state is stored separately.

## OrderBook v0.1

The OrderBook supports:

* ADD
* CANCEL
* TRADE
* active-order lookup by `order_id`
* aggregated bid price levels
* aggregated ask price levels
* best bid
* best ask

### State

```text
                    OrderBook
                       │
          ┌────────────┴────────────┐
          │                         │
     active orders              price levels
          │                     /         \
 order_id → Order            bids         asks
                           descending    ascending
```

Candidate standard-library structures:

```cpp
std::unordered_map<uint64_t, Order> orders_;
std::map<int64_t, uint64_t, std::greater<int64_t>> bids_;
std::map<int64_t, uint64_t> asks_;
```

Price-level values represent aggregate remaining quantity.

### Public API

```cpp
class OrderBook {
public:
    bool add(const Event& event);
    bool cancel(const Event& event);
    bool trade(const Event& event);

    std::optional<int64_t> best_bid() const;
    std::optional<int64_t> best_ask() const;
};
```

A successful operation returns `true`. Invalid operations return `false` and must leave OrderBook state unchanged.

## Current Non-Goals

Aether v0.1 does not include:

* trading strategies
* distributed clustering
* write-ahead logging
* kernel bypass
* custom memory allocators
* complex scheduling
