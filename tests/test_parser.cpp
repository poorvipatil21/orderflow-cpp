#include <gtest/gtest.h>
#include "orderflow/itch_parser.hpp"
#include <cstring>
#include <vector>

using namespace orderflow;

// Build a raw AddOrder message in big-endian ITCH 5.0 format
static std::vector<uint8_t> make_add_order(
        uint64_t order_ref, char side, uint32_t shares,
        const char* symbol8, uint32_t price) {
    // Layout: type(1) locate(2) tracking(2) ts(6) ref(8) side(1) shares(4) stock(8) price(4) = 36
    std::vector<uint8_t> buf(36, 0);
    buf[0] = 'A';
    // locate = 1, tracking = 0
    buf[1] = 0; buf[2] = 1;
    buf[3] = 0; buf[4] = 0;
    // timestamp = 0
    // order_ref (big-endian 8 bytes at offset 11)
    for (int i = 7; i >= 0; i--) {
        buf[11 + (7 - i)] = (order_ref >> (i * 8)) & 0xFF;
    }
    buf[19] = static_cast<uint8_t>(side);
    // shares at 20
    buf[20] = (shares >> 24) & 0xFF;
    buf[21] = (shares >> 16) & 0xFF;
    buf[22] = (shares >>  8) & 0xFF;
    buf[23] =  shares        & 0xFF;
    // stock at 24
    memcpy(&buf[24], symbol8, 8);
    // price at 32
    buf[32] = (price >> 24) & 0xFF;
    buf[33] = (price >> 16) & 0xFF;
    buf[34] = (price >>  8) & 0xFF;
    buf[35] =  price        & 0xFF;
    return buf;
}

TEST(Parser, AddOrder) {
    char sym8[8] = {'A','A','P','L',' ',' ',' ',' '};
    auto buf = make_add_order(999, 'B', 200, sym8, 1500000);

    ParsedMsg msg;
    ASSERT_TRUE(parse_message(buf.data(), static_cast<uint16_t>(buf.size()), msg));
    EXPECT_EQ(msg.type, MsgType::AddOrder);
    EXPECT_EQ(msg.order.order_ref, 999u);
    EXPECT_EQ(msg.order.side, 'B');
    EXPECT_EQ(msg.order.shares, 200u);
    EXPECT_EQ(msg.order.symbol, "AAPL");
    EXPECT_EQ(msg.order.price, 1500000u);
}

TEST(Parser, TooShortReturnsTrue) {
    // Unknown type with very short buffer -- should not crash
    uint8_t buf[3] = {'Z', 0, 0};
    ParsedMsg msg;
    // Unknown type: parse returns true (not a book msg but not malformed)
    // msg.type is set to the raw byte ('Z'), not MsgType::Unknown
    EXPECT_TRUE(parse_message(buf, 3, msg));
    EXPECT_NE(msg.type, MsgType::AddOrder);  // just verify it didn't crash
}

TEST(Parser, AddOrderTooShort) {
    char sym8[8] = {'A','A','P','L',' ',' ',' ',' '};
    auto buf = make_add_order(1, 'B', 100, sym8, 100);
    buf.resize(20);  // truncate
    ParsedMsg msg;
    EXPECT_FALSE(parse_message(buf.data(), 20, msg));
}

TEST(Parser, PriceConversion) {
    // price = 1500000 => $150.0000
    char sym8[8] = {'S','P','Y',' ',' ',' ',' ',' '};
    auto buf = make_add_order(1, 'S', 500, sym8, 4000100);
    ParsedMsg msg;
    ASSERT_TRUE(parse_message(buf.data(), static_cast<uint16_t>(buf.size()), msg));
    EXPECT_EQ(msg.order.price, 4000100u);
    EXPECT_NEAR(msg.order.price / 10000.0, 400.01, 1e-9);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
