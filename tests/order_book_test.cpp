#include "../market/order_book.h"
#include <cassert>

int main() {
    {
        OrderBook book;

        Event e{
            .sequence = 1,
            .order_id = 1,
            .price = 100,
            .quantity = 10,
            .side = Side::Buy,
            .type = EventType::Add
        };

        assert(book.add(e));
        assert(book.best_bid().has_value());
        assert(book.best_bid().value() == 100);
    }   
    {
        OrderBook book;

        Event e1{
            .sequence = 1,
            .order_id = 1,
            .price = 100,
            .quantity = 6,
            .side = Side::Buy,
            .type = EventType::Add
        };

        Event e2{
            .sequence = 2,
            .order_id = 2,
            .price = 100,
            .quantity = 4,
            .side = Side::Buy,
            .type = EventType::Add
        };

        assert(book.add(e1));
        assert(book.add(e2));
        assert(book.best_bid().value() == 100);

        Event c1{
            .sequence = 3,
            .order_id = 1,
            .price = 0,
            .quantity = 0,
            .side = Side::Buy,
            .type = EventType::Cancel
        };

        assert(book.cancel(c1));
        assert(book.best_bid().has_value());
        assert(book.best_bid().value() == 100);

        Event c2{
            .sequence = 4,
            .order_id = 2,
            .price = 0,
            .quantity = 0,
            .side = Side::Buy,
            .type = EventType::Cancel
        };

        assert(book.cancel(c2));
        assert(!book.best_bid().has_value());
    }
    {
        OrderBook book;

        Event b1{
            .sequence = 1,
            .order_id = 1,
            .price = 100,
            .quantity = 10,
            .side = Side::Buy,
            .type = EventType::Add
        };

        Event b2{
            .sequence = 2,
            .order_id = 2,
            .price = 101,
            .quantity = 5,
            .side = Side::Buy,
            .type = EventType::Add
        };

        Event a1{
            .sequence = 3,
            .order_id = 3,
            .price = 104,
            .quantity = 8,
            .side = Side::Sell,
            .type = EventType::Add
        };

        Event a2{
            .sequence = 4,
            .order_id = 4,
            .price = 103,
            .quantity = 7,
            .side = Side::Sell,
            .type = EventType::Add
        };

        assert(book.add(b1));
        assert(book.add(b2));
        assert(book.add(a1));
        assert(book.add(a2));

        assert(book.best_bid().value() == 101);
        assert(book.best_ask().value() == 103);
        Event c2{
            .sequence = 5,
            .order_id = 2,
            .price = 0,
            .quantity = 0,
            .side = Side::Buy,
            .type = EventType::Cancel
        };

        assert(book.cancel(c2));
        assert(book.best_bid().value() == 100);

        Event c4{
            .sequence = 6,
            .order_id = 4,
            .price = 0,
            .quantity = 0,
            .side = Side::Sell,
            .type = EventType::Cancel
        };

        assert(book.cancel(c4));
        assert(book.best_ask().value() == 104);
    }
    // partial + full trade
    {
        OrderBook book;

        Event add{
            .sequence = 1,
            .order_id = 1,
            .price = 100,
            .quantity = 10,
            .side = Side::Buy,
            .type = EventType::Add
        };

        assert(book.add(add));

        Event trade1{
            .sequence = 2,
            .order_id = 1,
            .price = 0,
            .quantity = 4,
            .side = Side::Buy,
            .type = EventType::Trade
        };

        assert(book.trade(trade1));
        assert(book.best_bid().value() == 100);

        Event trade2{
            .sequence = 3,
            .order_id = 1,
            .price = 0,
            .quantity = 6,
            .side = Side::Buy,
            .type = EventType::Trade
        };

        assert(book.trade(trade2));
        assert(!book.best_bid().has_value());

        Event add_again{
            .sequence = 4,
            .order_id = 1,
            .price = 101,
            .quantity = 5,
            .side = Side::Buy,
            .type = EventType::Add
        };

        assert(book.add(add_again));
    }
    // invalid operations
    {
        OrderBook book;

        Event add{
            .sequence = 1,
            .order_id = 1,
            .price = 100,
            .quantity = 10,
            .side = Side::Buy,
            .type = EventType::Add
        };

        assert(book.add(add));
        assert(!book.add(add));

        Event unknown_cancel{
            .sequence = 2,
            .order_id = 999,
            .price = 0,
            .quantity = 0,
            .side = Side::Buy,
            .type = EventType::Cancel
        };

        Event unknown_trade{
            .sequence = 3,
            .order_id = 999,
            .price = 0,
            .quantity = 1,
            .side = Side::Buy,
            .type = EventType::Trade
        };

        assert(!book.cancel(unknown_cancel));
        assert(!book.trade(unknown_trade));

        Event oversized_trade{
            .sequence = 4,
            .order_id = 1,
            .price = 0,
            .quantity = 11,
            .side = Side::Buy,
            .type = EventType::Trade
        };

        assert(!book.trade(oversized_trade));
        assert(book.best_bid().value() == 100);
    }
}