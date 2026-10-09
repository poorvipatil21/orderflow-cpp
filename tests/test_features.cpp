#include <gtest/gtest.h>
#include "orderflow/order_book.hpp"
#include "orderflow/features.hpp"

using namespace orderflow;

static Order make_order(uint64_t ref, char side, uint32_t shares, uint32_t price) {
    Order o;
    o.order_ref = ref;
    o.side      = side;
    o.shares    = shares;
    o.price     = price;
    o.symbol    = "TEST";
    return o;
}

TEST(Features, OFIBuyPressure) {
    LimitOrderBook book("TEST");
    book.add(make_order(1, 'B', 500, 1000000));
    book.add(make_order(2, 'S', 100, 1000100));

    auto snap = book.snapshot(0);
    auto feat = compute_features(snap);

    // More bid depth => positive OFI
    EXPECT_GT(feat.ofi_l1, 0.0);
    EXPECT_LE(feat.ofi_l1, 1.0);
}

TEST(Features, OFISellPressure) {
    LimitOrderBook book("TEST");
    book.add(make_order(1, 'B', 100, 1000000));
    book.add(make_order(2, 'S', 500, 1000100));

    auto snap = book.snapshot(0);
    auto feat = compute_features(snap);

    EXPECT_LT(feat.ofi_l1, 0.0);
    EXPECT_GE(feat.ofi_l1, -1.0);
}

TEST(Features, SpreadBps) {
    LimitOrderBook book("TEST");
    book.add(make_order(1, 'B', 100, 4000000));  // $400.00
    book.add(make_order(2, 'S', 100, 4000100));  // $400.01

    auto snap = book.snapshot(0);
    auto feat = compute_features(snap);

    EXPECT_NEAR(feat.spread, 0.01, 1e-9);
    // spread_bps = 0.01 / 400.005 * 10000 ~= 0.25
    EXPECT_NEAR(feat.spread_bps, 0.25, 0.01);
}

TEST(Features, EmptyBook) {
    LimitOrderBook book("EMPTY");
    auto snap = book.snapshot(0);
    auto feat = compute_features(snap);

    EXPECT_EQ(feat.best_bid, 0.0);
    EXPECT_EQ(feat.best_ask, 0.0);
    EXPECT_EQ(feat.ofi_l1, 0.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
