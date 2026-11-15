#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace quant {

// ============================================================================
// Ariel Silahian (Packt, 2024) - "C++ High Performance for Financial Systems"
// Integrated Chapters:
//   * Ch 2 & Ch 3: Lock-Free SPSC Messaging Hub, O(1) Circular-Array LOB,
//                  OMS/EMS Lifecycle, and Institutional Pre-Trade RMS
//   * Ch 4:        Intelligent Order Router (IOR) via Q-Learning Execution Opt.
//   * Ch 6:        Cache-Line Alignment, False Sharing Prevention, Locality,
//                  Loop Unrolling/Tiling, System Warmup & Branchless Kernels
//   * Ch 7:        Algorithmic Game Theory (Nash/Inventory-Skewed Market Making)
// ============================================================================

inline constexpr std::size_t HFT_CACHE_LINE_BYTES = 64;

enum class OrderSide : uint8_t {
    Buy = 0,
    Sell = 1
};

struct alignas(HFT_CACHE_LINE_BYTES) MarketTickMessage {
    uint64_t seq_id = 0;
    uint64_t order_id = 0;
    int64_t qty = 0;
    double price = 0.0;
    OrderSide side = OrderSide::Buy;
    uint8_t msg_type = 0; // 0 = Add, 1 = Update, 2 = Delete
    int64_t timestamp_ns = 0;
};

// ----------------------------------------------------------------------------
// Lock-Free Single-Producer / Single-Consumer Ring Buffer (Ch 2 & Ch 3)
// Eliminates mutex contention and separates read/write cursors across 64B cache lines.
// ----------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                  "SpscRingBuffer Capacity must be a power of two >= 2");

public:
    SpscRingBuffer() = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity - 1;
    }

    bool try_push(const T& item) noexcept {
        const std::size_t w = write_idx_.load(std::memory_order_relaxed);
        const std::size_t next_w = (w + 1) & kMask;
        if (next_w == read_idx_.load(std::memory_order_acquire)) [[unlikely]] {
            return false; // Queue full -> back-pressure (Ch 5)
        }
        buffer_[w] = item;
        write_idx_.store(next_w, std::memory_order_release);
        return true;
    }

    bool try_push(T&& item) noexcept {
        const std::size_t w = write_idx_.load(std::memory_order_relaxed);
        const std::size_t next_w = (w + 1) & kMask;
        if (next_w == read_idx_.load(std::memory_order_acquire)) [[unlikely]] {
            return false;
        }
        buffer_[w] = std::move(item);
        write_idx_.store(next_w, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) noexcept {
        const std::size_t r = read_idx_.load(std::memory_order_relaxed);
        if (r == write_idx_.load(std::memory_order_acquire)) [[unlikely]] {
            return false;
        }
        out = std::move(buffer_[r]);
        read_idx_.store((r + 1) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t w = write_idx_.load(std::memory_order_acquire);
        const std::size_t r = read_idx_.load(std::memory_order_acquire);
        return (w - r) & kMask;
    }

    [[nodiscard]] bool empty() const noexcept {
        return write_idx_.load(std::memory_order_acquire) ==
               read_idx_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(HFT_CACHE_LINE_BYTES) std::atomic<std::size_t> write_idx_{0};
    alignas(HFT_CACHE_LINE_BYTES) std::atomic<std::size_t> read_idx_{0};
    alignas(HFT_CACHE_LINE_BYTES) std::array<T, Capacity> buffer_{};
};

// ----------------------------------------------------------------------------
// Pre-Allocated O(1) Circular-Array Limit Order Book (Chapter 3)
// ----------------------------------------------------------------------------
struct LOBLevelSnapshot {
    double price = 0.0;
    int64_t quantity = 0;
    uint32_t order_count = 0;
    bool valid = false;
};

struct LOBMicrostructureMetrics {
    double best_bid = 0.0;
    double best_ask = 0.0;
    int64_t best_bid_qty = 0;
    int64_t best_ask_qty = 0;
    double mid_price = 0.0;
    double micro_price = 0.0;
    double spread_abs = 0.0;
    double spread_bps = 0.0;
    double l1_imbalance = 0.0;    // (bid_qty - ask_qty) / (bid_qty + ask_qty)
    double top5_imbalance = 0.0;  // Aggregate top-5 level imbalance
    std::size_t active_orders = 0;
};

class CircularArrayLOB {
public:
    CircularArrayLOB(double min_price, double max_price, double tick_size,
                     std::size_t expected_max_orders = 16384)
        : min_price_(min_price),
          tick_size_(std::max(1e-6, tick_size)),
          inv_tick_(1.0 / std::max(1e-6, tick_size)) {
        const double span = std::max(tick_size_, max_price - min_price);
        num_levels_ = static_cast<std::size_t>(std::ceil(span * inv_tick_)) + 2;
        bids_.resize(num_levels_);
        asks_.resize(num_levels_);

        // Pre-allocate open-addressing flat hash map to guarantee zero hot-path mallocs
        std::size_t table_cap = 1024;
        std::size_t target = std::max<std::size_t>(256, expected_max_orders) * 4;
        while (table_cap < target) table_cap <<= 1;
        order_table_.assign(table_cap, FlatOrderEntry{});
        table_mask_ = table_cap - 1;
    }

    bool add_order(uint64_t order_id, OrderSide side, double price, int64_t qty) noexcept {
        if (order_id == 0 || qty <= 0) [[unlikely]] return false;
        const std::size_t lvl = price_to_index(price);
        if (lvl >= num_levels_) [[unlikely]] return false;

        std::size_t slot_idx = 0;
        if (!insert_flat_order(order_id, lvl, qty, side, slot_idx)) [[unlikely]] {
            return false;
        }

        if (side == OrderSide::Buy) {
            bids_[lvl].total_qty += qty;
            bids_[lvl].order_count += 1;
            if (!has_bid_ || lvl > best_bid_idx_) {
                best_bid_idx_ = lvl;
                has_bid_ = true;
            }
            if (!has_bid_low_ || lvl < min_bid_idx_) {
                min_bid_idx_ = lvl;
                has_bid_low_ = true;
            }
        } else {
            asks_[lvl].total_qty += qty;
            asks_[lvl].order_count += 1;
            if (!has_ask_ || lvl < best_ask_idx_) {
                best_ask_idx_ = lvl;
                has_ask_ = true;
            }
            if (!has_ask_high_ || lvl > max_ask_idx_) {
                max_ask_idx_ = lvl;
                has_ask_high_ = true;
            }
        }
        ++active_orders_;
        return true;
    }

    bool update_order(uint64_t order_id, int64_t new_qty) noexcept {
        if (new_qty <= 0) {
            return delete_order(order_id);
        }
        FlatOrderEntry* entry = find_flat_order(order_id);
        if (!entry) [[unlikely]] return false;

        const int64_t delta = new_qty - entry->qty;
        entry->qty = new_qty;
        if (entry->side == OrderSide::Buy) {
            bids_[entry->level_idx].total_qty += delta;
        } else {
            asks_[entry->level_idx].total_qty += delta;
        }
        return true;
    }

    bool delete_order(uint64_t order_id) noexcept {
        FlatOrderEntry* entry = find_flat_order(order_id);
        if (!entry) [[unlikely]] return false;

        const std::size_t lvl = entry->level_idx;
        const OrderSide side = entry->side;
        const int64_t qty = entry->qty;

        entry->state = SlotState::Tombstone;
        entry->order_id = 0;
        --active_orders_;

        if (side == OrderSide::Buy) {
            auto& bl = bids_[lvl];
            bl.total_qty = std::max<int64_t>(0, bl.total_qty - qty);
            if (bl.order_count > 0) --bl.order_count;
            if (bl.order_count == 0 && has_bid_ && lvl == best_bid_idx_) {
                scan_next_best_bid();
            }
        } else {
            auto& al = asks_[lvl];
            al.total_qty = std::max<int64_t>(0, al.total_qty - qty);
            if (al.order_count > 0) --al.order_count;
            if (al.order_count == 0 && has_ask_ && lvl == best_ask_idx_) {
                scan_next_best_ask();
            }
        }
        return true;
    }

    [[nodiscard]] LOBLevelSnapshot get_best_bid() const noexcept {
        if (!has_bid_) return LOBLevelSnapshot{};
        return LOBLevelSnapshot{
            index_to_price(best_bid_idx_),
            bids_[best_bid_idx_].total_qty,
            bids_[best_bid_idx_].order_count,
            bids_[best_bid_idx_].order_count > 0
        };
    }

    [[nodiscard]] LOBLevelSnapshot get_best_offer() const noexcept {
        if (!has_ask_) return LOBLevelSnapshot{};
        return LOBLevelSnapshot{
            index_to_price(best_ask_idx_),
            asks_[best_ask_idx_].total_qty,
            asks_[best_ask_idx_].order_count,
            asks_[best_ask_idx_].order_count > 0
        };
    }

    [[nodiscard]] std::vector<LOBLevelSnapshot> top_levels(OrderSide side, std::size_t depth = 5) const {
        std::vector<LOBLevelSnapshot> out;
        out.reserve(depth);
        if (side == OrderSide::Buy) {
            if (!has_bid_) return out;
            for (std::size_t i = best_bid_idx_ + 1; i-- > min_bid_idx_ && out.size() < depth;) {
                if (bids_[i].order_count > 0 && bids_[i].total_qty > 0) {
                    out.push_back({index_to_price(i), bids_[i].total_qty, bids_[i].order_count, true});
                }
                if (i == 0) break;
            }
        } else {
            if (!has_ask_) return out;
            for (std::size_t i = best_ask_idx_; i <= max_ask_idx_ && i < num_levels_ && out.size() < depth; ++i) {
                if (asks_[i].order_count > 0 && asks_[i].total_qty > 0) {
                    out.push_back({index_to_price(i), asks_[i].total_qty, asks_[i].order_count, true});
                }
            }
        }
        return out;
    }

    [[nodiscard]] LOBMicrostructureMetrics analytics() const {
        LOBMicrostructureMetrics m;
        m.active_orders = active_orders_;
        auto bb = get_best_bid();
        auto ba = get_best_offer();
        if (!bb.valid || !ba.valid) return m;

        m.best_bid = bb.price;
        m.best_bid_qty = bb.quantity;
        m.best_ask = ba.price;
        m.best_ask_qty = ba.quantity;
        m.mid_price = 0.5 * (bb.price + ba.price);
        m.spread_abs = std::max(0.0, ba.price - bb.price);
        m.spread_bps = (m.mid_price > 0.0) ? (m.spread_abs / m.mid_price) * 10000.0 : 0.0;

        const double denom_l1 = static_cast<double>(bb.quantity + ba.quantity);
        if (denom_l1 > 0.0) {
            m.micro_price = (bb.price * static_cast<double>(ba.quantity) +
                             ba.price * static_cast<double>(bb.quantity)) / denom_l1;
            m.l1_imbalance = static_cast<double>(bb.quantity - ba.quantity) / denom_l1;
        } else {
            m.micro_price = m.mid_price;
        }

        auto top_b = top_levels(OrderSide::Buy, 5);
        auto top_a = top_levels(OrderSide::Sell, 5);
        int64_t sum_b = 0, sum_a = 0;
        for (const auto& lv : top_b) sum_b += lv.quantity;
        for (const auto& lv : top_a) sum_a += lv.quantity;
        if (sum_b + sum_a > 0) {
            m.top5_imbalance = static_cast<double>(sum_b - sum_a) / static_cast<double>(sum_b + sum_a);
        }
        return m;
    }

    [[nodiscard]] std::size_t active_order_count() const noexcept { return active_orders_; }
    [[nodiscard]] std::size_t level_capacity() const noexcept { return num_levels_; }
    [[nodiscard]] double tick_size() const noexcept { return tick_size_; }

private:
    struct alignas(16) PriceLevelSlot {
        int64_t total_qty = 0;
        uint32_t order_count = 0;
    };

    enum class SlotState : uint8_t { Empty = 0, Occupied = 1, Tombstone = 2 };

    struct FlatOrderEntry {
        uint64_t order_id = 0;
        std::size_t level_idx = 0;
        int64_t qty = 0;
        OrderSide side = OrderSide::Buy;
        SlotState state = SlotState::Empty;
    };

    [[nodiscard]] std::size_t price_to_index(double price) const noexcept {
        if (price <= min_price_) return 0;
        const long long ticks = std::llround((price - min_price_) * inv_tick_);
        if (ticks < 0) return 0;
        return static_cast<std::size_t>(ticks);
    }

    [[nodiscard]] double index_to_price(std::size_t idx) const noexcept {
        return min_price_ + static_cast<double>(idx) * tick_size_;
    }

    [[nodiscard]] static uint64_t mix_hash(uint64_t k) noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    }

    bool insert_flat_order(uint64_t order_id, std::size_t lvl, int64_t qty,
                           OrderSide side, std::size_t& out_slot) noexcept {
        std::size_t pos = static_cast<std::size_t>(mix_hash(order_id)) & table_mask_;
        std::size_t first_tombstone = std::numeric_limits<std::size_t>::max();
        for (std::size_t probe = 0; probe <= table_mask_; ++probe) {
            auto& slot = order_table_[pos];
            if (slot.state == SlotState::Empty) {
                const std::size_t target = (first_tombstone != std::numeric_limits<std::size_t>::max())
                                               ? first_tombstone : pos;
                order_table_[target] = FlatOrderEntry{order_id, lvl, qty, side, SlotState::Occupied};
                out_slot = target;
                return true;
            }
            if (slot.state == SlotState::Occupied && slot.order_id == order_id) {
                return false; // Duplicate ID
            }
            if (slot.state == SlotState::Tombstone && first_tombstone == std::numeric_limits<std::size_t>::max()) {
                first_tombstone = pos;
            }
            pos = (pos + 1) & table_mask_;
        }
        return false;
    }

    FlatOrderEntry* find_flat_order(uint64_t order_id) noexcept {
        std::size_t pos = static_cast<std::size_t>(mix_hash(order_id)) & table_mask_;
        for (std::size_t probe = 0; probe <= table_mask_; ++probe) {
            auto& slot = order_table_[pos];
            if (slot.state == SlotState::Empty) return nullptr;
            if (slot.state == SlotState::Occupied && slot.order_id == order_id) return &slot;
            pos = (pos + 1) & table_mask_;
        }
        return nullptr;
    }

    void scan_next_best_bid() noexcept {
        const std::size_t lo = has_bid_low_ ? min_bid_idx_ : 0;
        for (std::size_t i = best_bid_idx_; i > lo;) {
            --i;
            if (bids_[i].order_count > 0 && bids_[i].total_qty > 0) {
                best_bid_idx_ = i;
                has_bid_ = true;
                return;
            }
        }
        if (bids_[lo].order_count > 0 && bids_[lo].total_qty > 0 && lo <= best_bid_idx_) {
            best_bid_idx_ = lo;
            has_bid_ = true;
            return;
        }
        has_bid_ = false;
        best_bid_idx_ = 0;
    }

    void scan_next_best_ask() noexcept {
        const std::size_t hi = has_ask_high_ ? std::min(max_ask_idx_, num_levels_ - 1) : (num_levels_ - 1);
        for (std::size_t i = best_ask_idx_ + 1; i <= hi; ++i) {
            if (asks_[i].order_count > 0 && asks_[i].total_qty > 0) {
                best_ask_idx_ = i;
                has_ask_ = true;
                return;
            }
        }
        has_ask_ = false;
        best_ask_idx_ = num_levels_ > 0 ? num_levels_ - 1 : 0;
    }

    double min_price_;
    double tick_size_;
    double inv_tick_;
    std::size_t num_levels_ = 0;

    std::vector<PriceLevelSlot> bids_;
    std::vector<PriceLevelSlot> asks_;
    std::vector<FlatOrderEntry> order_table_;
    std::size_t table_mask_ = 0;

    std::size_t best_bid_idx_ = 0;
    std::size_t min_bid_idx_ = 0;
    bool has_bid_ = false;
    bool has_bid_low_ = false;

    std::size_t best_ask_idx_ = 0;
    std::size_t max_ask_idx_ = 0;
    bool has_ask_ = false;
    bool has_ask_high_ = false;

    std::size_t active_orders_ = 0;
};

// ----------------------------------------------------------------------------
// Reference std::map O(log N) Limit Order Book (Chapter 3 Baseline)
// ----------------------------------------------------------------------------
class MapLimitOrderBook {
public:
    explicit MapLimitOrderBook(double tick_size = 0.01)
        : tick_size_(std::max(1e-6, tick_size)) {}

    bool add_order(uint64_t order_id, OrderSide side, double price, int64_t qty) {
        if (order_id == 0 || qty <= 0 || orders_.contains(order_id)) return false;
        const int64_t ticks = std::llround(price / tick_size_);
        orders_.emplace(order_id, Entry{side, ticks, qty});
        if (side == OrderSide::Buy) {
            auto& lvl = bids_[ticks];
            lvl.qty += qty;
            lvl.count += 1;
        } else {
            auto& lvl = asks_[ticks];
            lvl.qty += qty;
            lvl.count += 1;
        }
        return true;
    }

    bool update_order(uint64_t order_id, int64_t new_qty) {
        if (new_qty <= 0) return delete_order(order_id);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) return false;
        const int64_t delta = new_qty - it->second.qty;
        it->second.qty = new_qty;
        if (it->second.side == OrderSide::Buy) {
            bids_[it->second.ticks].qty += delta;
        } else {
            asks_[it->second.ticks].qty += delta;
        }
        return true;
    }

    bool delete_order(uint64_t order_id) {
        auto it = orders_.find(order_id);
        if (it == orders_.end()) return false;
        const Entry e = it->second;
        orders_.erase(it);
        if (e.side == OrderSide::Buy) {
            auto bit = bids_.find(e.ticks);
            if (bit != bids_.end()) {
                bit->second.qty -= e.qty;
                if (--bit->second.count == 0) bids_.erase(bit);
            }
        } else {
            auto ait = asks_.find(e.ticks);
            if (ait != asks_.end()) {
                ait->second.qty -= e.qty;
                if (--ait->second.count == 0) asks_.erase(ait);
            }
        }
        return true;
    }

    [[nodiscard]] LOBLevelSnapshot get_best_bid() const {
        if (bids_.empty()) return LOBLevelSnapshot{};
        const auto& kv = *bids_.begin();
        return {static_cast<double>(kv.first) * tick_size_, kv.second.qty, kv.second.count, true};
    }

    [[nodiscard]] LOBLevelSnapshot get_best_offer() const {
        if (asks_.empty()) return LOBLevelSnapshot{};
        const auto& kv = *asks_.begin();
        return {static_cast<double>(kv.first) * tick_size_, kv.second.qty, kv.second.count, true};
    }

private:
    struct LevelAgg { int64_t qty = 0; uint32_t count = 0; };
    struct Entry { OrderSide side; int64_t ticks; int64_t qty; };

    double tick_size_;
    std::map<int64_t, LevelAgg, std::greater<int64_t>> bids_;
    std::map<int64_t, LevelAgg> asks_;
    std::unordered_map<uint64_t, Entry> orders_;
};

// ----------------------------------------------------------------------------
// Pre-Trade Institutional Risk Management System (RMS - Chapter 2 & Chapter 3)
// ----------------------------------------------------------------------------
struct PreTradeRiskConfig {
    int64_t max_order_qty = 25000;
    double max_order_notional = 500000.0;
    int64_t max_net_position = 50000;
    int64_t max_gross_position = 100000;
    double max_daily_drawdown = 25000.0;
    double max_price_collar_pct = 5.0; // Fat-finger protection vs mid
};

enum class RiskRejectReason : uint8_t {
    Approved = 0,
    KillSwitchEngaged,
    InvalidQuantityOrPrice,
    MaxOrderQtyExceeded,
    MaxOrderNotionalExceeded,
    NetPositionLimitExceeded,
    GrossPositionLimitExceeded,
    FatFingerCollarExceeded
};

struct RiskCheckResult {
    bool approved = false;
    RiskRejectReason reason = RiskRejectReason::Approved;
    std::string message;
};

class PreTradeRiskEngine {
public:
    explicit PreTradeRiskEngine(PreTradeRiskConfig cfg = {}) : cfg_(cfg) {}

    void set_pnl(double daily_pnl) noexcept {
        daily_pnl_ = daily_pnl;
        if (daily_pnl_ <= -std::abs(cfg_.max_daily_drawdown)) {
            kill_switch_ = true;
        }
    }

    void reset_kill_switch() noexcept { kill_switch_ = false; }

    void set_positions(int64_t net_pos, int64_t gross_pos) noexcept {
        net_pos_ = net_pos;
        gross_pos_ = gross_pos;
    }

    RiskCheckResult validate_order(OrderSide side, double price, int64_t qty,
                                   double reference_mid) noexcept {
        ++total_checked_;
        if (kill_switch_ || daily_pnl_ <= -std::abs(cfg_.max_daily_drawdown)) [[unlikely]] {
            kill_switch_ = true;
            ++total_rejected_;
            return {false, RiskRejectReason::KillSwitchEngaged,
                    std::format("KILL-SWITCH: Daily PnL {:.2f} breached -{:.2f} limit",
                                daily_pnl_, cfg_.max_daily_drawdown)};
        }
        if (qty <= 0 || price <= 0.0 || !std::isfinite(price)) [[unlikely]] {
            ++total_rejected_;
            return {false, RiskRejectReason::InvalidQuantityOrPrice,
                    "REJECT: Non-positive order price or quantity"};
        }
        if (qty > cfg_.max_order_qty) [[unlikely]] {
            ++total_rejected_;
            return {false, RiskRejectReason::MaxOrderQtyExceeded,
                    std::format("REJECT: Qty {} > MaxOrderQty {}", qty, cfg_.max_order_qty)};
        }
        const double notional = price * static_cast<double>(qty);
        if (notional > cfg_.max_order_notional) [[unlikely]] {
            ++total_rejected_;
            return {false, RiskRejectReason::MaxOrderNotionalExceeded,
                    std::format("REJECT: Notional {:.0f} > MaxNotional {:.0f}",
                                notional, cfg_.max_order_notional)};
        }
        const int64_t signed_qty = (side == OrderSide::Buy) ? qty : -qty;
        const int64_t proj_net = net_pos_ + signed_qty;
        if (std::abs(proj_net) > cfg_.max_net_position) [[unlikely]] {
            ++total_rejected_;
            return {false, RiskRejectReason::NetPositionLimitExceeded,
                    std::format("REJECT: Projected net pos {} exceeds +/-{}",
                                proj_net, cfg_.max_net_position)};
        }
        const int64_t proj_gross = gross_pos_ + qty;
        if (proj_gross > cfg_.max_gross_position) [[unlikely]] {
            ++total_rejected_;
            return {false, RiskRejectReason::GrossPositionLimitExceeded,
                    std::format("REJECT: Projected gross pos {} > {}",
                                proj_gross, cfg_.max_gross_position)};
        }
        if (reference_mid > 0.0) {
            const double dev_pct = std::abs(price - reference_mid) / reference_mid * 100.0;
            if (dev_pct > cfg_.max_price_collar_pct) [[unlikely]] {
                ++total_rejected_;
                return {false, RiskRejectReason::FatFingerCollarExceeded,
                        std::format("REJECT: Price dev {:.2f}% > collar {:.2f}%",
                                    dev_pct, cfg_.max_price_collar_pct)};
            }
        }
        ++total_approved_;
        return {true, RiskRejectReason::Approved, "APPROVED"};
    }

    void record_fill(OrderSide side, int64_t qty) noexcept {
        net_pos_ += (side == OrderSide::Buy) ? qty : -qty;
        gross_pos_ += qty;
    }

    [[nodiscard]] int64_t net_position() const noexcept { return net_pos_; }
    [[nodiscard]] int64_t gross_position() const noexcept { return gross_pos_; }
    [[nodiscard]] uint64_t checked_count() const noexcept { return total_checked_; }
    [[nodiscard]] uint64_t approved_count() const noexcept { return total_approved_; }
    [[nodiscard]] uint64_t rejected_count() const noexcept { return total_rejected_; }

private:
    PreTradeRiskConfig cfg_;
    int64_t net_pos_ = 0;
    int64_t gross_pos_ = 0;
    double daily_pnl_ = 0.0;
    bool kill_switch_ = false;
    uint64_t total_checked_ = 0;
    uint64_t total_approved_ = 0;
    uint64_t total_rejected_ = 0;
};

// ----------------------------------------------------------------------------
// ML-Driven Intelligent Order Router (IOR - Chapter 4 Execution Optimization)
// Learns state-action Q-table over market spread/vol states -> venue + slicing actions
// to minimize Implementation Shortfall (slippage + fees + adverse impact).
// ----------------------------------------------------------------------------
struct VenueProfile {
    std::string name;
    double maker_taker_fee_bps = 1.5;
    double base_slippage_bps = 2.0;
    double size_impact_coeff = 6.0;
    double fill_probability = 0.95;
    double reject_probability = 0.01;
    double latency_us = 18.0;
};

struct IORSimulationSummary {
    double arrival_price = 0.0;
    int64_t total_target_shares = 0;
    int64_t ior_filled_shares = 0;
    int64_t naive_filled_shares = 0;
    double ior_vwap = 0.0;
    double naive_vwap = 0.0;
    double ior_shortfall_bps = 0.0;
    double naive_shortfall_bps = 0.0;
    double shortfall_savings_bps = 0.0;
    double ior_fill_rate_pct = 0.0;
    double avg_latency_us = 0.0;
    std::string dominant_venue;
    std::array<int, 3> venue_allocations{0, 0, 0};
};

class IntelligentOrderRouter {
public:
    IntelligentOrderRouter() {
        venues_ = {
            VenueProfile{"LIT_PRIMARY",   1.2, 1.8, 7.5, 0.98, 0.005, 14.0},
            VenueProfile{"MIDPOINT_DARK", 0.3, 0.4, 2.2, 0.68, 0.020, 26.0},
            VenueProfile{"ALT_ECN_FAST",  0.8, 1.1, 4.8, 0.88, 0.012, 19.0}
        };
        // Initialize tabular Q-values mildly optimistic for faster exploration
        for (auto& s : q_table_) s.fill(-2.0);
    }

    // Actions: 0 = LIT Full Slice, 1 = DARK Passive Mid, 2 = ALT ECN Split, 3 = Smart Split (50% Dark + 50% Fast ECN)
    static constexpr int kNumStates = 4;   // 2 spread regimes x 2 volatility regimes
    static constexpr int kNumActions = 4;

    [[nodiscard]] static int encode_state(double spread_bps, double realized_vol) noexcept {
        const int wide_spread = (spread_bps > 12.0) ? 1 : 0;
        const int high_vol = (realized_vol > 0.25) ? 1 : 0;
        return (wide_spread << 1) | high_vol;
    }

    IORSimulationSummary simulate_parent_order(double arrival_price, double annualized_vol,
                                               double spread_bps, int64_t parent_shares,
                                               int training_episodes = 240,
                                               uint64_t seed = 42) {
        if (arrival_price <= 0.0) arrival_price = 100.0;
        if (parent_shares <= 0) parent_shares = 10000;

        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        std::normal_distribution<double> eps(0.0, 1.0);

        const double alpha = 0.18;
        const double gamma_disc = 0.85;
        double epsilon = 0.45;
        const double daily_vol = std::clamp(annualized_vol / std::sqrt(252.0), 0.004, 0.06);

        // Online RL warmup episodes over stylized spread/vol dynamics
        for (int ep = 0; ep < training_episodes; ++ep) {
            const double ep_spread = spread_bps * (0.6 + 0.9 * u01(rng));
            const double ep_vol = annualized_vol * (0.7 + 0.7 * u01(rng));
            int s = encode_state(ep_spread, ep_vol);

            for (int step = 0; step < 5; ++step) {
                int act = 0;
                if (u01(rng) < epsilon) {
                    act = static_cast<int>(rng() % kNumActions);
                } else {
                    act = best_action(s);
                }
                const auto res = execute_slice_cost(act, ep_spread, ep_vol, 0.20, rng);
                const int s_next = encode_state(ep_spread * (0.9 + 0.25 * u01(rng)), ep_vol);
                const double reward = -res.cost_bps; // Minimize shortfall + unfill penalty
                const double best_next = *std::max_element(q_table_[s_next].begin(), q_table_[s_next].end());
                q_table_[s][act] += alpha * (reward + gamma_disc * best_next - q_table_[s][act]);
                s = s_next;
            }
            epsilon = std::max(0.05, epsilon * 0.985);
        }

        // Evaluation run: Compare Naive Aggressive Single-Venue LIT Sweep vs Learned IOR
        const int eval_slices = 10;
        const int64_t slice_qty = std::max<int64_t>(1, parent_shares / eval_slices);

        double cur_mid = arrival_price;
        double naive_cost_sum = 0.0;
        int64_t naive_filled = 0;

        double ior_cost_sum = 0.0;
        int64_t ior_filled = 0;
        double lat_sum = 0.0;
        std::array<int, 3> venue_counts{0, 0, 0};

        for (int t = 0; t < eval_slices; ++t) {
            const int64_t this_qty = (t == eval_slices - 1)
                ? (parent_shares - slice_qty * (eval_slices - 1))
                : slice_qty;
            const double part_ratio = static_cast<double>(this_qty) / static_cast<double>(parent_shares);
            const double cur_spread = spread_bps * (0.85 + 0.4 * u01(rng));
            const int state = encode_state(cur_spread, annualized_vol);
            const int best_act = best_action(state);

            // Naive always sweeps LIT_PRIMARY (action 0) at market with full urgency impact
            const auto naive_fill = execute_slice_cost(0, cur_spread, annualized_vol, part_ratio * 1.4, rng);
            const double naive_exec_px = cur_mid * (1.0 + naive_fill.impact_bps * 1e-4);
            const int64_t n_sh = naive_fill.rejected ? 0 : static_cast<int64_t>(std::llround(this_qty * naive_fill.fill_frac));
            naive_cost_sum += naive_exec_px * static_cast<double>(n_sh);
            naive_filled += n_sh;

            // IOR executes learned optimal routing policy
            const auto ior_fill = execute_slice_cost(best_act, cur_spread, annualized_vol, part_ratio, rng);
            const double ior_exec_px = cur_mid * (1.0 + ior_fill.impact_bps * 1e-4);
            const int64_t i_sh = ior_fill.rejected ? 0 : static_cast<int64_t>(std::llround(this_qty * ior_fill.fill_frac));
            ior_cost_sum += ior_exec_px * static_cast<double>(i_sh);
            ior_filled += i_sh;
            lat_sum += ior_fill.latency_us;

            if (best_act == 0) venue_counts[0]++;
            else if (best_act == 1) venue_counts[1]++;
            else if (best_act == 2) venue_counts[2]++;
            else {
                venue_counts[1]++;
                venue_counts[2]++;
            }

            // Evolve mid-price with slight positive temporary market pressure + noise
            cur_mid *= std::exp(0.0001 + (daily_vol * 0.15) * eps(rng));
        }

        const double cleanup_px = cur_mid * (1.0 + (spread_bps * 0.75 + 6.0) * 1e-4);
        const int64_t naive_unfilled = std::max<int64_t>(0, parent_shares - naive_filled);
        const int64_t ior_unfilled = std::max<int64_t>(0, parent_shares - ior_filled);
        const double total_target_notional = arrival_price * static_cast<double>(parent_shares);
        const double naive_total_paid = naive_cost_sum + static_cast<double>(naive_unfilled) * cleanup_px;
        const double ior_total_paid = ior_cost_sum + static_cast<double>(ior_unfilled) * cleanup_px;

        IORSimulationSummary out;
        out.arrival_price = arrival_price;
        out.total_target_shares = parent_shares;
        out.ior_filled_shares = ior_filled;
        out.naive_filled_shares = naive_filled;
        out.ior_vwap = (ior_filled > 0) ? (ior_cost_sum / static_cast<double>(ior_filled)) : arrival_price;
        out.naive_vwap = (naive_filled > 0) ? (naive_cost_sum / static_cast<double>(naive_filled)) : arrival_price;
        // True Perold Implementation Shortfall including horizon-end cleanup cost for unfilled shares
        out.ior_shortfall_bps = ((ior_total_paid - total_target_notional) / total_target_notional) * 10000.0;
        out.naive_shortfall_bps = ((naive_total_paid - total_target_notional) / total_target_notional) * 10000.0;
        out.shortfall_savings_bps = out.naive_shortfall_bps - out.ior_shortfall_bps;
        out.ior_fill_rate_pct = 100.0 * static_cast<double>(ior_filled) / static_cast<double>(parent_shares);
        out.avg_latency_us = lat_sum / static_cast<double>(eval_slices);
        out.venue_allocations = venue_counts;

        int max_idx = 0;
        for (int i = 1; i < 3; ++i) {
            if (venue_counts[i] > venue_counts[max_idx]) max_idx = i;
        }
        out.dominant_venue = venues_[static_cast<std::size_t>(max_idx)].name;
        return out;
    }

private:
    struct SliceExecResult {
        double impact_bps = 0.0;
        double cost_bps = 0.0;
        double fill_frac = 1.0;
        double latency_us = 15.0;
        bool rejected = false;
    };

    [[nodiscard]] int best_action(int state) const noexcept {
        const auto& row = q_table_[static_cast<std::size_t>(state)];
        return static_cast<int>(std::distance(row.begin(), std::max_element(row.begin(), row.end())));
    }

    SliceExecResult execute_slice_cost(int action, double spread_bps, double vol,
                                       double participation, std::mt19937_64& rng) const {
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        SliceExecResult r;
        const auto eval_venue = [&](const VenueProfile& v, double slice_share, double dark_discount) {
            const bool rej = (u01(rng) < v.reject_probability);
            const double fill = rej ? 0.0 : ((u01(rng) < v.fill_probability) ? 1.0 : 0.45);
            const double half_spread = 0.5 * spread_bps * dark_discount;
            const double temp_impact = v.base_slippage_bps + v.size_impact_coeff * std::sqrt(slice_share * participation) * (1.0 + vol);
            const double unfill_pen = (1.0 - fill) * (spread_bps * 0.45 + 3.0);
            SliceExecResult s;
            s.rejected = rej;
            s.fill_frac = fill;
            s.impact_bps = half_spread + temp_impact + v.maker_taker_fee_bps;
            s.cost_bps = s.impact_bps + unfill_pen;
            s.latency_us = v.latency_us;
            return s;
        };

        if (action == 0) return eval_venue(venues_[0], 1.0, 1.0);
        if (action == 1) return eval_venue(venues_[1], 1.0, 0.15); // Near midpoint
        if (action == 2) return eval_venue(venues_[2], 1.0, 0.65);

        // Action 3: 50% Midpoint Dark + 50% Fast ECN split
        const auto d = eval_venue(venues_[1], 0.5, 0.15);
        const auto e = eval_venue(venues_[2], 0.5, 0.65);
        r.rejected = d.rejected && e.rejected;
        r.fill_frac = 0.5 * d.fill_frac + 0.5 * e.fill_frac;
        r.impact_bps = 0.5 * d.impact_bps + 0.5 * e.impact_bps;
        r.cost_bps = 0.5 * d.cost_bps + 0.5 * e.cost_bps;
        r.latency_us = std::max(d.latency_us, e.latency_us);
        return r;
    }

    std::vector<VenueProfile> venues_;
    std::array<std::array<double, kNumActions>, kNumStates> q_table_{};
};

// ----------------------------------------------------------------------------
// Algorithmic Game Theory Market Maker (Chapter 7: Nash / Avellaneda-Stoikov)
// Combines reservation price inventory skew + adverse-selection OBI defense.
// ----------------------------------------------------------------------------
struct GameTheoryQuoteResult {
    double micro_price = 0.0;
    double reservation_price = 0.0;
    double optimal_bid = 0.0;
    double optimal_ask = 0.0;
    double optimal_half_spread_bps = 0.0;
    double inventory_skew_bps = 0.0;
    double simulated_mm_pnl = 0.0;
    int simulated_fills = 0;
    int final_inventory = 0;
};

class GameTheoreticQuoter {
public:
    static GameTheoryQuoteResult compute_nash_quotes(double mid_price,
                                                     double micro_price,
                                                     double order_book_imbalance,
                                                     int current_inventory,
                                                     double annualized_vol,
                                                     double risk_aversion_gamma = 0.12,
                                                     double liquidity_kappa = 1.8,
                                                     double horizon_days = 1.0 / 252.0) {
        GameTheoryQuoteResult res;
        if (mid_price <= 0.0) return res;

        const double micro = (micro_price > 0.0) ? micro_price : mid_price;
        const double sigma_px = std::max(1e-6, mid_price * std::max(0.05, annualized_vol));
        const double gamma = std::max(1e-4, risk_aversion_gamma);
        const double kappa = std::max(1e-3, liquidity_kappa);

        // Reservation price shifts away from accumulated inventory
        const double inv_skew = static_cast<double>(current_inventory) * gamma * (sigma_px * sigma_px) * horizon_days * 1e-2;
        const double obi_shift = order_book_imbalance * (0.25 * sigma_px * std::sqrt(horizon_days));
        res.micro_price = micro;
        res.reservation_price = micro - inv_skew + obi_shift;

        // Optimal Nash-equilibrium half-spread + adverse selection bluff defense
        const double base_delta = 0.5 * gamma * (sigma_px * sigma_px) * horizon_days * 1e-2
                                + (1.0 / gamma) * std::log(1.0 + gamma / kappa) * (mid_price * 0.00025);
        const double adverse_selection_guard = std::abs(order_book_imbalance) * mid_price * 0.00035;
        const double half_spread = std::max(mid_price * 0.0002, base_delta + adverse_selection_guard);

        res.optimal_bid = std::max(0.01, res.reservation_price - half_spread);
        res.optimal_ask = std::max(res.optimal_bid + 0.01, res.reservation_price + half_spread);
        res.optimal_half_spread_bps = (half_spread / mid_price) * 10000.0;
        res.inventory_skew_bps = ((res.reservation_price - micro) / mid_price) * 10000.0;
        return res;
    }

    static GameTheoryQuoteResult run_market_making_session(double start_price,
                                                           double annualized_vol,
                                                           int steps = 200,
                                                           uint64_t seed = 101) {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> z(0.0, 1.0);
        std::uniform_real_distribution<double> u01(0.0, 1.0);

        double mid = std::max(1.0, start_price);
        double cash = 0.0;
        int inv = 0;
        int fills = 0;
        const double step_vol = std::clamp(annualized_vol / std::sqrt(252.0 * 12.0), 0.0005, 0.02);

        GameTheoryQuoteResult last_quote;
        for (int t = 0; t < steps; ++t) {
            const double obi = std::clamp(0.45 * z(rng), -0.95, 0.95);
            const double micro = mid * (1.0 + obi * step_vol * 0.25);
            last_quote = compute_nash_quotes(mid, micro, obi, inv, annualized_vol);

            // Flow arrival intensity decays exponentially with quote distance from mid
            const double bid_dist_bps = std::max(0.0, (mid - last_quote.optimal_bid) / mid * 10000.0);
            const double ask_dist_bps = std::max(0.0, (last_quote.optimal_ask - mid) / mid * 10000.0);
            const double p_bid_fill = std::clamp(0.48 * std::exp(-0.04 * bid_dist_bps) * (1.0 - 0.3 * obi), 0.02, 0.85);
            const double p_ask_fill = std::clamp(0.48 * std::exp(-0.04 * ask_dist_bps) * (1.0 + 0.3 * obi), 0.02, 0.85);

            if (u01(rng) < p_bid_fill && inv < 40) {
                cash -= last_quote.optimal_bid;
                ++inv;
                ++fills;
            }
            if (u01(rng) < p_ask_fill && inv > -40) {
                cash += last_quote.optimal_ask;
                --inv;
                ++fills;
            }
            mid *= std::exp(step_vol * z(rng));
        }
        last_quote.simulated_mm_pnl = cash + static_cast<double>(inv) * mid;
        last_quote.simulated_fills = fills;
        last_quote.final_inventory = inv;
        return last_quote;
    }
};

// ----------------------------------------------------------------------------
// Chapter 6 + Chapter 3 Empirical CPU & Low-Latency Benchmark Suite
// Measures genuine on-machine timings (never synthetic constants).
// ----------------------------------------------------------------------------
struct LowLatencyBenchmarkReport {
    double lob_map_ms = 0.0;
    double lob_circular_ms = 0.0;
    double lob_speedup = 1.0;

    double false_shared_ms = 0.0;
    double cache_padded_ms = 0.0;
    double false_sharing_speedup = 1.0;

    double col_major_ms = 0.0;
    double row_major_tiled_ms = 0.0;
    double locality_speedup = 1.0;

    double cold_pass_ms = 0.0;
    double warmed_pass_ms = 0.0;

    double branchy_unsorted_ms = 0.0;
    double branchless_ms = 0.0;
    double branchless_speedup = 1.0;
};

class LowLatencyBenchmarkSuite {
public:
    static LowLatencyBenchmarkReport run_on_machine(std::size_t lob_ops = 25000,
                                                    std::size_t mat_dim = 224) {
        LowLatencyBenchmarkReport rep;
        using Clock = std::chrono::steady_clock;

        // 1. LOB Data Structure Benchmark: std::map O(log N) vs CircularArrayLOB O(1)
        {
            const std::size_t n = std::clamp<std::size_t>(lob_ops, 2000, 100000);
            MapLimitOrderBook map_lob(0.01);
            CircularArrayLOB circ_lob(50.0, 250.0, 0.01, n + 1024);

            volatile double sink_map = 0.0;
            auto t0 = Clock::now();
            for (std::size_t i = 1; i <= n; ++i) {
                const OrderSide s = (i & 1U) ? OrderSide::Buy : OrderSide::Sell;
                const double px = (s == OrderSide::Buy)
                    ? (99.0 + static_cast<double>(i % 200) * 0.01)
                    : (101.0 + static_cast<double>(i % 200) * 0.01);
                map_lob.add_order(i, s, px, 100 + static_cast<int64_t>(i % 50));
                if ((i & 3U) == 0 && i > 4) map_lob.update_order(i - 2, 250);
                if ((i & 7U) == 0 && i > 8) map_lob.delete_order(i - 6);
                if ((i & 15U) == 0) {
                    sink_map += map_lob.get_best_bid().price + map_lob.get_best_offer().price;
                }
            }
            auto t1 = Clock::now();
            rep.lob_map_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            volatile double sink_circ = 0.0;
            auto t2 = Clock::now();
            for (std::size_t i = 1; i <= n; ++i) {
                const OrderSide s = (i & 1U) ? OrderSide::Buy : OrderSide::Sell;
                const double px = (s == OrderSide::Buy)
                    ? (99.0 + static_cast<double>(i % 200) * 0.01)
                    : (101.0 + static_cast<double>(i % 200) * 0.01);
                circ_lob.add_order(i, s, px, 100 + static_cast<int64_t>(i % 50));
                if ((i & 3U) == 0 && i > 4) circ_lob.update_order(i - 2, 250);
                if ((i & 7U) == 0 && i > 8) circ_lob.delete_order(i - 6);
                if ((i & 15U) == 0) {
                    sink_circ += circ_lob.get_best_bid().price + circ_lob.get_best_offer().price;
                }
            }
            auto t3 = Clock::now();
            rep.lob_circular_ms = std::max(0.001, std::chrono::duration<double, std::milli>(t3 - t2).count());
            rep.lob_speedup = rep.lob_map_ms / rep.lob_circular_ms;
            (void)sink_map;
            (void)sink_circ;
        }

        // 2. Avoiding False Sharing (Ch 6): Two threads incrementing shared vs 64B-padded atomics
        {
            struct Unpadded {
                std::atomic<uint64_t> a{0};
                std::atomic<uint64_t> b{0};
            } unpadded;

            struct Padded {
                alignas(HFT_CACHE_LINE_BYTES) std::atomic<uint64_t> a{0};
                alignas(HFT_CACHE_LINE_BYTES) std::atomic<uint64_t> b{0};
            } padded;

            constexpr uint64_t kIters = 1200000ULL;

            auto t0 = Clock::now();
            {
                std::thread t_a([&]() {
                    for (uint64_t i = 0; i < kIters; ++i) {
                        unpadded.a.fetch_add(1, std::memory_order_relaxed);
                    }
                });
                std::thread t_b([&]() {
                    for (uint64_t i = 0; i < kIters; ++i) {
                        unpadded.b.fetch_add(1, std::memory_order_relaxed);
                    }
                });
                t_a.join();
                t_b.join();
            }
            auto t1 = Clock::now();
            rep.false_shared_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            auto t2 = Clock::now();
            {
                std::thread t_a([&]() {
                    for (uint64_t i = 0; i < kIters; ++i) {
                        padded.a.fetch_add(1, std::memory_order_relaxed);
                    }
                });
                std::thread t_b([&]() {
                    for (uint64_t i = 0; i < kIters; ++i) {
                        padded.b.fetch_add(1, std::memory_order_relaxed);
                    }
                });
                t_a.join();
                t_b.join();
            }
            auto t3 = Clock::now();
            rep.cache_padded_ms = std::max(0.001, std::chrono::duration<double, std::milli>(t3 - t2).count());
            rep.false_sharing_speedup = rep.false_shared_ms / rep.cache_padded_ms;
        }

        // 3. Data Locality + 4x Loop Unrolling / Tiling (Ch 6) + Warmup Cold vs Primed
        {
            const std::size_t n = std::clamp<std::size_t>(mat_dim, 64, 512);
            std::vector<double> mat(n * n, 1.0001);
            for (std::size_t i = 0; i < mat.size(); ++i) mat[i] = 1.0 + 0.001 * static_cast<double>(i & 63U);

            volatile double sink = 0.0;
            // Cold pass (before L1/L2 residency)
            auto tc0 = Clock::now();
            double sum_cold = 0.0;
            for (std::size_t r = 0; r < n; ++r) {
                for (std::size_t c = 0; c < n; ++c) {
                    sum_cold += mat[r * n + c];
                }
            }
            auto tc1 = Clock::now();
            rep.cold_pass_ms = std::chrono::duration<double, std::milli>(tc1 - tc0).count();
            sink += sum_cold;

            // Column-major strided loop across multiple repeats
            constexpr int kReps = 16;
            auto tcol0 = Clock::now();
            double col_acc = 0.0;
            for (int rep_i = 0; rep_i < kReps; ++rep_i) {
                for (std::size_t c = 0; c < n; ++c) {
                    for (std::size_t r = 0; r < n; ++r) {
                        col_acc += mat[r * n + c];
                    }
                }
            }
            auto tcol1 = Clock::now();
            rep.col_major_ms = std::chrono::duration<double, std::milli>(tcol1 - tcol0).count();
            sink += col_acc;

            // Row-major sequential + 4x unrolled & tiled pass (warmed cache)
            auto trow0 = Clock::now();
            double row_acc = 0.0;
            for (int rep_i = 0; rep_i < kReps; ++rep_i) {
                for (std::size_t r = 0; r < n; ++r) {
                    const double* row_ptr = &mat[r * n];
                    double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0;
                    std::size_t c = 0;
                    for (; c + 3 < n; c += 4) {
                        s0 += row_ptr[c];
                        s1 += row_ptr[c + 1];
                        s2 += row_ptr[c + 2];
                        s3 += row_ptr[c + 3];
                    }
                    for (; c < n; ++c) s0 += row_ptr[c];
                    row_acc += (s0 + s1 + s2 + s3);
                }
            }
            auto trow1 = Clock::now();
            rep.row_major_tiled_ms = std::max(0.001, std::chrono::duration<double, std::milli>(trow1 - trow0).count());
            rep.warmed_pass_ms = rep.row_major_tiled_ms / static_cast<double>(kReps);
            rep.locality_speedup = rep.col_major_ms / rep.row_major_tiled_ms;
            sink += row_acc;
            (void)sink;
        }

        // 4. Branch Prediction vs Branchless Accumulation (Ch 6)
        {
            constexpr std::size_t kLen = 200000;
            std::vector<int> vals(kLen);
            std::mt19937 rng(7);
            std::uniform_int_distribution<int> dist(0, 255);
            for (auto& v : vals) v = dist(rng);

            volatile int64_t sink_br = 0;
            auto t0 = Clock::now();
            int64_t acc1 = 0;
            for (int r = 0; r < 12; ++r) {
                for (std::size_t i = 0; i < kLen; ++i) {
                    if (vals[i] >= 128) acc1 += vals[i];
                }
            }
            auto t1 = Clock::now();
            rep.branchy_unsorted_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            sink_br += acc1;

            auto t2 = Clock::now();
            int64_t acc2 = 0;
            for (int r = 0; r < 12; ++r) {
                for (std::size_t i = 0; i < kLen; ++i) {
                    const int mask = -(vals[i] >= 128);
                    acc2 += (vals[i] & mask);
                }
            }
            auto t3 = Clock::now();
            rep.branchless_ms = std::max(0.001, std::chrono::duration<double, std::milli>(t3 - t2).count());
            rep.branchless_speedup = rep.branchy_unsorted_ms / rep.branchless_ms;
            sink_br += acc2;
            (void)sink_br;
        }

        return rep;
    }
};

} // namespace quant
