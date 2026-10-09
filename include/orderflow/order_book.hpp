#pragma once
#include "itch_parser.hpp"
#include <map>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

namespace orderflow {

struct PriceLevel {
    uint32_t price  = 0;
    uint64_t shares = 0;   // total visible quantity
    uint64_t count  = 0;   // number of resting orders
};

struct BookSnapshot {
    std::string symbol;
    uint64_t    timestamp     = 0;
    std::vector<PriceLevel> bids;  // descending price
    std::vector<PriceLevel> asks;  // ascending price

    double best_bid()  const { return bids.empty() ? 0.0 : bids.front().price / 10000.0; }
    double best_ask()  const { return asks.empty() ? 0.0 : asks.front().price / 10000.0; }
    double mid()       const {
        if (bids.empty() || asks.empty()) return 0.0;
        return (bids.front().price + asks.front().price) / 20000.0;
    }
    double spread()    const {
        if (bids.empty() || asks.empty()) return 0.0;
        return (asks.front().price - bids.front().price) / 10000.0;
    }
};

// Tracks one symbol's full limit order book (L3).
class LimitOrderBook {
public:
    explicit LimitOrderBook(std::string sym) : symbol_(std::move(sym)) {}

    void add(const Order& o);
    void execute(uint64_t order_ref, uint32_t shares);
    void cancel(uint64_t order_ref, uint32_t cancelled_shares);
    void remove(uint64_t order_ref);
    void replace(uint64_t old_ref, uint64_t new_ref, uint32_t shares, uint32_t price);

    BookSnapshot snapshot(uint64_t ts, int depth = 10) const;

    const std::string& symbol() const { return symbol_; }

    // Counters
    uint64_t add_count()     const { return add_count_; }
    uint64_t cancel_count()  const { return cancel_count_; }
    uint64_t execute_count() const { return execute_count_; }
    uint64_t delete_count()  const { return delete_count_; }
    uint64_t replace_count() const { return replace_count_; }

    // Invariant: total bid shares == sum of bid price levels
    bool check_invariants() const;

private:
    struct OrderEntry {
        uint64_t order_ref;
        char     side;
        uint32_t shares;
        uint32_t price;
    };

    std::string symbol_;

    // order_ref -> OrderEntry
    std::unordered_map<uint64_t, OrderEntry> orders_;

    // price -> PriceLevel (for bids: descending key, asks: ascending key)
    std::map<uint32_t, PriceLevel, std::greater<uint32_t>> bids_;
    std::map<uint32_t, PriceLevel>                          asks_;

    uint64_t add_count_     = 0;
    uint64_t cancel_count_  = 0;
    uint64_t execute_count_ = 0;
    uint64_t delete_count_  = 0;
    uint64_t replace_count_ = 0;

    void deduct(const OrderEntry& e, uint32_t shares);
    void remove_entry(uint64_t order_ref);
};

// Manages books for all symbols seen in a feed.
class BookManager {
public:
    void apply(const ParsedMsg& msg);

    LimitOrderBook* book(const std::string& sym);
    const std::unordered_map<std::string, LimitOrderBook>& all() const { return books_; }

    uint64_t total_messages() const { return total_msgs_; }

private:
    std::unordered_map<std::string, LimitOrderBook> books_;
    std::unordered_map<uint64_t, std::string>       order_to_sym_;
    uint64_t total_msgs_ = 0;
};

} // namespace orderflow
