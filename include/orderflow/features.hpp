#pragma once
#include "order_book.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace orderflow {

struct MicrostructureFeatures {
    std::string symbol;
    uint64_t    timestamp     = 0;

    double best_bid    = 0.0;
    double best_ask    = 0.0;
    double mid         = 0.0;
    double spread      = 0.0;          // ask - bid
    double spread_bps  = 0.0;          // spread / mid * 10000

    double bid_depth_l1 = 0.0;        // shares at best bid
    double ask_depth_l1 = 0.0;        // shares at best ask
    double bid_depth_5  = 0.0;        // total shares top-5 bid levels
    double ask_depth_5  = 0.0;        // total shares top-5 ask levels

    // Order-flow imbalance:  (bid_depth - ask_depth) / (bid_depth + ask_depth)
    // Range [-1, 1]. Positive => buy-side pressure.
    double ofi_l1 = 0.0;
    double ofi_5  = 0.0;

    // Queue imbalance at top of book
    double queue_imbalance = 0.0;
};

MicrostructureFeatures compute_features(const BookSnapshot& snap);

// Compute OFI for a vector of snapshots and return OFI series
std::vector<double> ofi_series(const std::vector<BookSnapshot>& snaps);

} // namespace orderflow
