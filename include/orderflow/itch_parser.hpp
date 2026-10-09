#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

namespace orderflow {

// NASDAQ ITCH 5.0 message types
enum class MsgType : char {
    SystemEvent        = 'S',
    StockDirectory     = 'R',
    StockTradingAction = 'H',
    RegSHORestriction  = 'Y',
    MarketParticipant  = 'L',
    MWCBDecline        = 'V',
    MWCBStatus         = 'W',
    IPOQuoting         = 'K',
    AddOrder           = 'A',
    AddOrderMPID       = 'F',
    OrderExecuted      = 'E',
    OrderExecutedPrice = 'C',
    OrderCancel        = 'X',
    OrderDelete        = 'D',
    OrderReplace       = 'U',
    Trade              = 'P',
    CrossTrade         = 'Q',
    BrokenTrade        = 'B',
    NOII               = 'I',
    Unknown            = '\0'
};

#pragma pack(push, 1)

struct AddOrderMsg {
    char     msg_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    char     side;
    uint32_t shares;
    char     stock[8];
    uint32_t price;   // price * 10000
};

struct OrderExecutedMsg {
    char     msg_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct OrderCancelMsg {
    char     msg_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t cancelled_shares;
};

struct OrderDeleteMsg {
    char     msg_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
};

struct OrderReplaceMsg {
    char     msg_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t orig_order_ref;
    uint64_t new_order_ref;
    uint32_t shares;
    uint32_t price;
};

#pragma pack(pop)

// Parsed, host-endian representation
struct Order {
    uint64_t    order_ref;
    char        side;       // 'B' or 'S'
    uint32_t    shares;
    uint32_t    price;      // integer price * 10000
    std::string symbol;
};

struct ParsedMsg {
    MsgType  type      = MsgType::Unknown;
    uint64_t timestamp = 0;   // nanoseconds from midnight

    // Filled depending on type
    Order    order{};         // AddOrder / AddOrderMPID
    uint64_t order_ref  = 0;
    uint64_t new_ref    = 0;
    uint32_t shares     = 0;
    uint32_t price      = 0;
    uint64_t match_num  = 0;
};

// Byte-swap helpers
inline uint16_t be16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}
inline uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}
inline uint64_t be64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}
inline uint64_t be48(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 6; i++) v = (v << 8) | p[i];
    return v;
}

// Parse a single raw message buffer (without leading 2-byte length).
// Returns false if the buffer is too short or the type is unrecognised.
bool parse_message(const uint8_t* buf, uint16_t len, ParsedMsg& out);

} // namespace orderflow
