#include "orderflow/features.hpp"
#include <cmath>
#include <numeric>

using namespace orderflow;

MicrostructureFeatures orderflow::compute_features(const BookSnapshot& snap) {
    MicrostructureFeatures f;
    f.symbol    = snap.symbol;
    f.timestamp = snap.timestamp;
    f.best_bid  = snap.best_bid();
    f.best_ask  = snap.best_ask();
    f.mid       = snap.mid();
    f.spread    = snap.spread();
    f.spread_bps = (f.mid > 0) ? (f.spread / f.mid * 10000.0) : 0.0;

    if (!snap.bids.empty()) {
        f.bid_depth_l1 = snap.bids.front().shares;
        f.bid_depth_5  = 0;
        int n = 0;
        for (auto& lvl : snap.bids) {
            if (n++ >= 5) break;
            f.bid_depth_5 += lvl.shares;
        }
    }
    if (!snap.asks.empty()) {
        f.ask_depth_l1 = snap.asks.front().shares;
        f.ask_depth_5  = 0;
        int n = 0;
        for (auto& lvl : snap.asks) {
            if (n++ >= 5) break;
            f.ask_depth_5 += lvl.shares;
        }
    }

    // OFI level-1
    double tot1 = f.bid_depth_l1 + f.ask_depth_l1;
    f.ofi_l1 = (tot1 > 0) ? (f.bid_depth_l1 - f.ask_depth_l1) / tot1 : 0.0;

    // OFI top-5
    double tot5 = f.bid_depth_5 + f.ask_depth_5;
    f.ofi_5 = (tot5 > 0) ? (f.bid_depth_5 - f.ask_depth_5) / tot5 : 0.0;

    // Queue imbalance (same as OFI_l1 here, could be weighted differently)
    f.queue_imbalance = f.ofi_l1;

    return f;
}

std::vector<double> orderflow::ofi_series(const std::vector<BookSnapshot>& snaps) {
    std::vector<double> out;
    out.reserve(snaps.size());
    for (auto& snap : snaps)
        out.push_back(compute_features(snap).ofi_l1);
    return out;
}
