# Aether Wire Protocol v0.1

Aether v0.1 uses a fixed-size 32-byte binary frame.

The wire format is independent of the in-memory C++ `Event` layout.

## Frame Layout

| Offset | Size | Field |
|---|---:|---|
| 0 | 8 | sequence |
| 8 | 8 | order_id |
| 16 | 8 | price |
| 24 | 4 | quantity |
| 28 | 1 | side |
| 29 | 1 | event_type |
| 30 | 2 | reserved |

Total frame size: 32 bytes.

## Byte Order

All multi-byte integer fields use big-endian byte order.

## Side Encoding

- `0` = Buy
- `1` = Sell

Any other value is invalid.

## Event Type Encoding

- `0` = Add
- `1` = Cancel
- `2` = Trade

Any other value is invalid.

## Reserved Bytes

Bytes 30 and 31 must both be zero in protocol version 0.1.

Frames with non-zero reserved bytes are invalid.

## Quantity Validation

- Add: quantity must be greater than zero.
- Trade: quantity must be greater than zero.
- Cancel: quantity may be zero.

## Decoder Contract

The decoder accepts exactly one 32-byte frame.

Valid frame:

```text
32 wire bytes
↓
validation
↓
canonical Event
```
Invalid frame:

```text
32 wire bytes
↓
validation fails
↓
std::nullopt
```
Frames shorter or longer than 32 bytes are invalid.