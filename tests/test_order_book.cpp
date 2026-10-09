#include <gtest/gtest.h>
#include "orderflow/order_book.hpp"

using namespace orderflow;

static Order make_order(uint64_t ref, char side, uint32_t shares, uint32_t price,
                        const std::string& sym = "AAPL") {
    Order o;
    o.order_ref = ref;
    o.side      = side;
    o.shares    = shares;
    o.price     = price;
    o.symbol    = sym;
    return o;
}

TEST(OrderBook, AddBidAsk) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));  // $150.0000
    book.add(make_order(2, 'S', 200, 1500100));  // $150.0100

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.bids.size(), 1u);
    ASSERT_EQ(snap.asks.size(), 1u);
    EXPECT_EQ(snap.bids[0].shares, 100u);
    EXPECT_EQ(snap.asks[0].shares, 200u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, AggregateSamePrice) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));
    book.add(make_order(2, 'B', 200, 1500000));

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0].shares, 300u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, Cancel) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));
    book.cancel(1, 50);

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0].shares, 50u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, CancelFull) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));
    book.cancel(1, 100);

    auto snap = book.snapshot(0);
    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, Execute) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'S', 200, 1500100));
    book.execute(1, 100);

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.asks.size(), 1u);
    EXPECT_EQ(snap.asks[0].shares, 100u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, Delete) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));
    book.add(make_order(2, 'B', 200, 1500000));
    book.remove(1);

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0].shares, 200u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, Replace) {
    LimitOrderBook book("AAPL");
    book.add(make_order(1, 'B', 100, 1500000));
    book.replace(1, 2, 150, 1499900);

    auto snap = book.snapshot(0);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0].price, 1499900u);
    EXPECT_EQ(snap.bids[0].shares, 150u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, MultiLevelDepth) {
    LimitOrderBook book("MSFT");
    // Three bid levels
    book.add(make_order(1, 'B', 100, 3000000));  // $300
    book.add(make_order(2, 'B', 200, 2999000));  // $299.90
    book.add(make_order(3, 'B', 300, 2998000));  // $299.80

    auto snap = book.snapshot(0, 10);
    ASSERT_EQ(snap.bids.size(), 3u);
    // Best bid first (descending)
    EXPECT_EQ(snap.bids[0].price, 3000000u);
    EXPECT_EQ(snap.bids[1].price, 2999000u);
    EXPECT_EQ(snap.bids[2].price, 2998000u);
    EXPECT_TRUE(book.check_invariants());
}

TEST(OrderBook, SnapshotSpread) {
    LimitOrderBook book("SPY");
    book.add(make_order(1, 'B', 500, 4000000));  // $400.00
    book.add(make_order(2, 'S', 500, 4000100));  // $400.01

    auto snap = book.snapshot(0);
    EXPECT_NEAR(snap.spread(), 0.01, 1e-9);
    EXPECT_NEAR(snap.mid(), 400.005, 1e-9);
}

TEST(BookManager, ApplyAddAndDelete) {
    BookManager mgr;

    ParsedMsg add_msg;
    add_msg.type            = MsgType::AddOrder;
    add_msg.order.order_ref = 42;
    add_msg.order.side      = 'B';
    add_msg.order.shares    = 100;
    add_msg.order.price     = 1000000;
    add_msg.order.symbol    = "AAPL";
    mgr.apply(add_msg);

    ParsedMsg del_msg;
    del_msg.type      = MsgType::OrderDelete;
    del_msg.order_ref = 42;
    mgr.apply(del_msg);

    auto* book = mgr.book("AAPL");
    ASSERT_NE(book, nullptr);
    auto snap = book->snapshot(0);
    EXPECT_TRUE(snap.bids.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
