#include "orderflow/order_book.hpp"
#include <stdexcept>
#include <cassert>

using namespace orderflow;

// ---- LimitOrderBook --------------------------------------------------------

void LimitOrderBook::add(const Order& o) {
    if (orders_.count(o.order_ref)) return;   // duplicate protection

    OrderEntry e{o.order_ref, o.side, o.shares, o.price};
    orders_[o.order_ref] = e;

    if (o.side == 'B') {
        auto& lvl = bids_[o.price];
        lvl.price   = o.price;
        lvl.shares += o.shares;
        lvl.count++;
    } else {
        auto& lvl = asks_[o.price];
        lvl.price   = o.price;
        lvl.shares += o.shares;
        lvl.count++;
    }
    ++add_count_;
}

void LimitOrderBook::deduct(const OrderEntry& e, uint32_t shares) {
    if (e.side == 'B') {
        auto it = bids_.find(e.price);
        if (it != bids_.end()) {
            it->second.shares -= std::min<uint64_t>(shares, it->second.shares);
            if (it->second.shares == 0) bids_.erase(it);
        }
    } else {
        auto it = asks_.find(e.price);
        if (it != asks_.end()) {
            it->second.shares -= std::min<uint64_t>(shares, it->second.shares);
            if (it->second.shares == 0) asks_.erase(it);
        }
    }
}

void LimitOrderBook::execute(uint64_t order_ref, uint32_t shares) {
    auto it = orders_.find(order_ref);
    if (it == orders_.end()) return;

    OrderEntry& e = it->second;
    deduct(e, shares);

    if (shares >= e.shares) {
        orders_.erase(it);
    } else {
        e.shares -= shares;
        // Restore the level count (deduct may have removed the level if shares==0 transiently)
        if (e.side == 'B') {
            auto& lvl = bids_[e.price];
            lvl.price   = e.price;
            lvl.shares += 0;  // already deducted correctly
            // Reinsert if gone
        } else {
            auto& lvl = asks_[e.price];
            lvl.price   = e.price;
        }
    }
    ++execute_count_;
}

void LimitOrderBook::cancel(uint64_t order_ref, uint32_t cancelled_shares) {
    auto it = orders_.find(order_ref);
    if (it == orders_.end()) return;

    OrderEntry& e = it->second;
    uint32_t actual = std::min(cancelled_shares, e.shares);
    deduct(e, actual);
    e.shares -= actual;

    if (e.shares == 0) orders_.erase(it);
    ++cancel_count_;
}

void LimitOrderBook::remove(uint64_t order_ref) {
    auto it = orders_.find(order_ref);
    if (it == orders_.end()) return;

    deduct(it->second, it->second.shares);
    // Decrement count
    if (it->second.side == 'B') {
        auto lvl_it = bids_.find(it->second.price);
        if (lvl_it != bids_.end()) {
            if (lvl_it->second.count > 0) lvl_it->second.count--;
            if (lvl_it->second.shares == 0) bids_.erase(lvl_it);
        }
    } else {
        auto lvl_it = asks_.find(it->second.price);
        if (lvl_it != asks_.end()) {
            if (lvl_it->second.count > 0) lvl_it->second.count--;
            if (lvl_it->second.shares == 0) asks_.erase(lvl_it);
        }
    }
    orders_.erase(it);
    ++delete_count_;
}

void LimitOrderBook::replace(uint64_t old_ref, uint64_t new_ref,
                              uint32_t shares, uint32_t price) {
    auto it = orders_.find(old_ref);
    if (it == orders_.end()) return;

    char side = it->second.side;
    remove(old_ref);

    Order o;
    o.order_ref = new_ref;
    o.side      = side;
    o.shares    = shares;
    o.price     = price;
    o.symbol    = symbol_;
    add(o);
    ++replace_count_;
}

BookSnapshot LimitOrderBook::snapshot(uint64_t ts, int depth) const {
    BookSnapshot snap;
    snap.symbol    = symbol_;
    snap.timestamp = ts;

    int n = 0;
    for (auto& [price, lvl] : bids_) {
        if (n++ >= depth) break;
        snap.bids.push_back(lvl);
    }
    n = 0;
    for (auto& [price, lvl] : asks_) {
        if (n++ >= depth) break;
        snap.asks.push_back(lvl);
    }
    return snap;
}

bool LimitOrderBook::check_invariants() const {
    uint64_t bid_total = 0;
    for (auto& [p, lvl] : bids_) bid_total += lvl.shares;
    uint64_t ask_total = 0;
    for (auto& [p, lvl] : asks_) ask_total += lvl.shares;

    uint64_t order_bid = 0, order_ask = 0;
    for (auto& [ref, e] : orders_) {
        if (e.side == 'B') order_bid += e.shares;
        else                order_ask += e.shares;
    }
    return bid_total == order_bid && ask_total == order_ask;
}

// ---- BookManager -----------------------------------------------------------

void BookManager::apply(const ParsedMsg& msg) {
    ++total_msgs_;

    switch (msg.type) {
        case MsgType::AddOrder: {
            const std::string& sym = msg.order.symbol;
            if (sym.empty()) break;
            auto [it, _] = books_.try_emplace(sym, sym);
            it->second.add(msg.order);
            order_to_sym_[msg.order.order_ref] = sym;
            break;
        }
        case MsgType::OrderExecuted:
        case MsgType::OrderExecutedPrice: {
            auto sit = order_to_sym_.find(msg.order_ref);
            if (sit == order_to_sym_.end()) break;
            auto bit = books_.find(sit->second);
            if (bit == books_.end()) break;
            bit->second.execute(msg.order_ref, msg.shares);
            break;
        }
        case MsgType::OrderCancel: {
            auto sit = order_to_sym_.find(msg.order_ref);
            if (sit == order_to_sym_.end()) break;
            auto bit = books_.find(sit->second);
            if (bit == books_.end()) break;
            bit->second.cancel(msg.order_ref, msg.shares);
            break;
        }
        case MsgType::OrderDelete: {
            auto sit = order_to_sym_.find(msg.order_ref);
            if (sit == order_to_sym_.end()) break;
            auto bit = books_.find(sit->second);
            if (bit == books_.end()) break;
            bit->second.remove(msg.order_ref);
            order_to_sym_.erase(sit);
            break;
        }
        case MsgType::OrderReplace: {
            auto sit = order_to_sym_.find(msg.order_ref);
            if (sit == order_to_sym_.end()) break;
            std::string sym = sit->second;
            auto bit = books_.find(sym);
            if (bit == books_.end()) break;
            bit->second.replace(msg.order_ref, msg.new_ref, msg.shares, msg.price);
            order_to_sym_.erase(sit);
            order_to_sym_[msg.new_ref] = sym;
            break;
        }
        default:
            break;
    }
}

LimitOrderBook* BookManager::book(const std::string& sym) {
    auto it = books_.find(sym);
    return it == books_.end() ? nullptr : &it->second;
}
