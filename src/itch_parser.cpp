#include "orderflow/itch_parser.hpp"
#include <cstring>

using namespace orderflow;

static std::string parse_symbol(const char* p) {
    // ITCH symbol is 8 chars, right-padded with spaces
    std::string s(p, 8);
    auto pos = s.find_last_not_of(' ');
    return (pos == std::string::npos) ? "" : s.substr(0, pos + 1);
}

bool orderflow::parse_message(const uint8_t* buf, uint16_t len, ParsedMsg& out) {
    if (len == 0) return false;

    out.type = static_cast<MsgType>(buf[0]);

    // Most messages: locate=bytes 1-2, tracking=bytes 3-4, ts=bytes 5-10
    auto ts_std = [&]() -> uint64_t {
        if (len < 11) return 0;
        return be48(buf + 5);
    };

    switch (out.type) {
        case MsgType::AddOrder: {
            // S R T T T T T T order_ref(8) side shares(4) stock(8) price(4)
            // Total: 1+2+2+6+8+1+4+8+4 = 36
            if (len < 36) return false;
            out.timestamp         = be48(buf + 5);
            out.order.order_ref   = be64(buf + 11);
            out.order.side        = static_cast<char>(buf[19]);
            out.order.shares      = be32(buf + 20);
            out.order.symbol      = parse_symbol(reinterpret_cast<const char*>(buf + 24));
            out.order.price       = be32(buf + 32);
            break;
        }
        case MsgType::AddOrderMPID: {
            // Same as AddOrder but +4 bytes MPID at end = 40
            if (len < 40) return false;
            out.timestamp         = be48(buf + 5);
            out.order.order_ref   = be64(buf + 11);
            out.order.side        = static_cast<char>(buf[19]);
            out.order.shares      = be32(buf + 20);
            out.order.symbol      = parse_symbol(reinterpret_cast<const char*>(buf + 24));
            out.order.price       = be32(buf + 32);
            out.type              = MsgType::AddOrder; // treat same
            break;
        }
        case MsgType::OrderExecuted: {
            // 1+2+2+6+8+4+8 = 31
            if (len < 31) return false;
            out.timestamp  = be48(buf + 5);
            out.order_ref  = be64(buf + 11);
            out.shares     = be32(buf + 19);
            out.match_num  = be64(buf + 23);
            break;
        }
        case MsgType::OrderExecutedPrice: {
            // 1+2+2+6+8+4+8+1+4 = 36
            if (len < 36) return false;
            out.timestamp  = be48(buf + 5);
            out.order_ref  = be64(buf + 11);
            out.shares     = be32(buf + 19);
            out.match_num  = be64(buf + 23);
            break;
        }
        case MsgType::OrderCancel: {
            // 1+2+2+6+8+4 = 23
            if (len < 23) return false;
            out.timestamp  = be48(buf + 5);
            out.order_ref  = be64(buf + 11);
            out.shares     = be32(buf + 19);
            break;
        }
        case MsgType::OrderDelete: {
            // 1+2+2+6+8 = 19
            if (len < 19) return false;
            out.timestamp  = be48(buf + 5);
            out.order_ref  = be64(buf + 11);
            break;
        }
        case MsgType::OrderReplace: {
            // 1+2+2+6+8+8+4+4 = 35
            if (len < 35) return false;
            out.timestamp  = be48(buf + 5);
            out.order_ref  = be64(buf + 11);   // orig_order_ref
            out.new_ref    = be64(buf + 19);   // new_order_ref
            out.shares     = be32(buf + 27);
            out.price      = be32(buf + 31);
            break;
        }
        default:
            // Not a book-relevant message; we still return true so the
            // caller can count total messages without treating them as errors.
            out.timestamp = ts_std();
            break;
    }
    return true;
}
