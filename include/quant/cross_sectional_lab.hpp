#pragma once

#include "alpha_screener.hpp"
#include "forecast_engine.hpp"
#include "i18n.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
#include "theme_engine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace quant {

// ============================================================================
// Classification & Correlation Metric Helpers (Pillar 3 & 4)
// ============================================================================
struct ClassificationMetrics {
    double accuracy{0.0};
    double zero_rule{0.0};
    double lift{0.0};
    double f1_macro{0.0};
    double mcc{0.0}; // Gorodkin Multi-Class Matthews Correlation Coefficient
    double dir_hit_rate_ex_neutral{0.5};
    size_t non_neutral_samples{0};
    size_t n_samples{0};
};

struct SignalCorrelationStats {
    double pearson_r{0.0};
    double spearman_ic{0.0};
    double t_stat{0.0};
    double p_value{1.0};
    size_t n{0};
};

// ============================================================================
// Diagnostic Suite Results (Pillar 3: Look-Ahead, Signal Quality, Overfit, Sentiment Var)
// ============================================================================
struct CausalityAuditResult {
    bool passed{false};
    double max_feature_diff{0.0};
    double max_forecast_diff{0.0};
    size_t test_bar_index{0};
    std::string detail;
};

struct DiagnosticCheckSummary {
    CausalityAuditResult lookahead_audit;
    bool synthetic_overfit_passed{false};
    double synthetic_overfit_dir_acc{0.0};
    bool temporal_sentiment_var_passed{false};
    double temporal_sentiment_std{0.0};
    SignalCorrelationStats signal_quality;
};

// ============================================================================
// Cross-Sectional Portfolio Backtest & Fake-Alpha Structures (Pillars 2, 3, 4)
// ============================================================================
struct CrossSectionalConfig {
    std::string region{"US"}; // US, BIST, XETRA, CUSTOM
    std::string experiment_name;
    int horizon_days{5};               // Forward return evaluation horizon (trading days)
    int rebalance_days{5};             // Portfolio rebalance cadence (trading days)
    int top_n{4};                      // Top-N long portfolio size
    double top_pct{0.25};              // Top-K% quantile slice (0.20 = quintile, 0.25 = quartile)
    double neutral_band_ret{0.0025};   // +/-0.25% threshold separating Up/Flat/Down classes
    double tc_bps{10.0};               // 10 bps one-way transaction cost * portfolio turnover
    bool use_next_open_execution{true}; // Realistic Open[t+1] execution when OHLC is present
    bool exclude_etfs_warrants{true};  // Exclude ETFs / index proxies / leveraged products
    bool use_live_sentiment{true};     // Blend live asset news sentiment & sticky shock signal
    size_t warmup_bars{65};
    int monte_carlo_trials{24};
    std::string reports_root{"reports"};
};

struct DailyCrossSectionalIC {
    int64_t day_key{0};
    std::string date_str;
    double ic{0.0};
    double ic_rolling_20{0.0};
    int n_stocks{0};
    double q_spread{0.0};
    double cumulative_spread{0.0};
    double pred_vol_corr{0.0};
    double abs_pred_vol_corr{0.0};
    double high_vol_pool_ic{0.0};
};

struct StrategyPerformanceRow {
    std::string strategy_name;
    bool is_model{true};
    double total_return{0.0};      // Decimal (e.g. 0.145 = +14.5%)
    double annualized_return{0.0}; // Decimal
    double sharpe_ratio{0.0};
    double sortino_ratio{0.0};
    double max_drawdown{0.0};      // Negative decimal (e.g. -0.12 = -12.0%)
    double daily_win_rate{0.0};    // Decimal (0.52 = 52%)
    double total_turnover{0.0};    // Cumulative one-way turnover
    double excess_vs_bnh{0.0};     // Decimal difference vs Buy & Hold
};

struct StockPredictabilityRow {
    std::string ticker;
    size_t n_samples{0};
    double ic{0.0};
    double directional_accuracy{0.0};
    double dir_hit_rate_ex_neutral{0.5};
    double zero_rule{0.0};
    double lift{0.0};
    double mcc{0.0};
    double f1_macro{0.0};
    double mse{0.0};
    double r2{0.0};
    int topn_picks{0};
    double topn_contrib_pct{0.0};
    double ret_when_picked_ann{0.0};
    double ret_when_not_picked_ann{0.0};
    double timing_spread_pct{0.0};
};

struct FoldPerformanceRow {
    int fold{0};
    std::string val_start;
    std::string val_end;
    size_t val_samples{0};
    double ic{0.0};
    double accuracy{0.0};
    double directional_accuracy{0.0};
    double lift{0.0};
    double mcc{0.0};
    double f1_macro{0.0};
    double q_spread{0.0};
};

struct SkillDiagnosticsSummary {
    double model_topn_sharpe{0.0};
    double vol_topn_sharpe{0.0};
    double rand_vol_topk_sharpe{0.0};
    double rand_model_topk_sharpe{0.0};
    double shuffled_signal_sharpe{0.0};

    double excess_sharpe_vs_vol{0.0};
    double excess_sharpe_vs_rand_vol{0.0};
    double excess_sharpe_vs_shuffled{0.0};

    double avg_pred_vol_corr{0.0};
    double avg_abs_pred_vol_corr{0.0};
    double high_vol_pool_ic{0.0};

    double mean_ret_picked_ann{0.0};
    double mean_ret_not_picked_ann{0.0};
    double timing_alpha_pct{0.0};

    double sharpe_ex_top3_stocks{0.0};
    double sharpe_ex_best5_days{0.0};
    std::string verdict;
};

struct TradeLogRow {
    std::string date;
    std::string ticker;
    std::string action; // BUY / SELL / REBALANCE
    double price{0.0};
    double predicted_return{0.0};
    double actual_return{0.0};
    double pnl_pct{0.0};
};

struct HoldingsLogRow {
    std::string date;
    std::string top_picks;
    double portfolio_value{1.0};
    double daily_return{0.0};
    double benchmark_value{1.0};
};

struct CrossSectionalReport {
    CrossSectionalConfig config;
    bool valid{false};
    bool is_live_data{false};
    int universe_size{0};
    int excluded_etf_count{0};
    int evaluated_days{0};
    size_t total_observations{0};
    std::string start_date;
    std::string end_date;
    std::string exported_dir;

    // Cross-sectional & global predictive quality
    double mean_cs_ic{0.0};
    double ic_std{0.0};
    double ic_ir{0.0};
    double pct_positive_ic{0.0};
    double mean_q_spread{0.0};
    double cumulative_q_spread{0.0};
    double global_wf_ic{0.0};
    double global_mse{0.0};
    double global_r2{0.0};
    ClassificationMetrics global_clf;

    SkillDiagnosticsSummary skill;
    DiagnosticCheckSummary diagnostics;

    std::vector<DailyCrossSectionalIC> daily_ic_series;
    std::vector<StrategyPerformanceRow> strategies;
    std::vector<StockPredictabilityRow> per_stock;
    std::vector<FoldPerformanceRow> per_fold;
    std::vector<TradeLogRow> topn_trades;
    std::vector<HoldingsLogRow> topn_holdings;
};

class CrossSectionalBacktestEngine {
public:
    // Filters out ETFs, Index Proxies, Warrants and Leveraged Products from Equity Universes
    static bool is_clean_equity(const RealTimeAssetData& asset) {
        if (asset.historical_prices.size() < 75) return false;
        if (asset.instrument_type == "ETF" || asset.instrument_type == "INDEX" ||
            asset.instrument_type == "MUTUALFUND") {
            return false;
        }
        std::string sym_u = asset.symbol;
        std::string name_u = asset.long_name;
        std::transform(sym_u.begin(), sym_u.end(), sym_u.begin(), ::toupper);
        std::transform(name_u.begin(), name_u.end(), name_u.begin(), ::toupper);

        static const std::vector<std::string> bad_syms = {
            "SPY", "QQQ", "IWM", "DIA", "VTI", "VOO", "TQQQ", "SQQQ", "SOXL", "SOXS",
            "SPXL", "SPXU", "UVXY", "VXX", "XU100.IS", "^GSPC", "^GDAXI"
        };
        for (const auto& bs : bad_syms) {
            if (sym_u == bs) return false;
        }
        if (sym_u.ends_with(".W") || sym_u.starts_with("^")) return false;
        if (name_u.find(" ETF") != std::string::npos ||
            name_u.find("WARRANT") != std::string::npos ||
            name_u.find("2X ") != std::string::npos ||
            name_u.find("3X ") != std::string::npos ||
            name_u.find("PROSHARES") != std::string::npos ||
            name_u.find("DIREXION") != std::string::npos ||
            name_u.find("INDEX TRUST") != std::string::npos) {
            return false;
        }
        return true;
    }

    static std::string format_epoch_date(int64_t ts, int64_t day_key = 0) {
        int64_t sec = ts;
        if (sec <= 0 && day_key > 10000) {
            sec = day_key * 86400LL;
        }
        if (sec <= 0) {
            // Synthetic monotonic index -> valid ISO date starting 2024-01-01
            std::time_t base = 1704067200LL + day_key * 86400LL;
            std::tm tm_buf{};
#ifdef _WIN32
            gmtime_s(&tm_buf, &base);
#else
            gmtime_r(&base, &tm_buf);
#endif
            return std::format("{:04d}-{:02d}-{:02d}", tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
        }
        std::time_t tt = static_cast<std::time_t>(sec);
        std::tm tm_buf{};
#ifdef _WIN32
        gmtime_s(&tm_buf, &tt);
#else
        gmtime_r(&tt, &tm_buf);
#endif
        return std::format("{:04d}-{:02d}-{:02d}", tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
    }

    // Average-tie Spearman rank correlation
    static double spearman_rank_ic(const std::vector<double>& x, const std::vector<double>& y) {
        const size_t n = std::min(x.size(), y.size());
        if (n < 3) return 0.0;
        auto rank_vec = [&](const std::vector<double>& v) {
            std::vector<size_t> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return v[a] < v[b]; });
            std::vector<double> r(n, 0.0);
            size_t i = 0;
            while (i < n) {
                size_t j = i + 1;
                while (j < n && std::abs(v[idx[j]] - v[idx[i]]) <= 1e-12) ++j;
                double avg_rank = 0.5 * static_cast<double>(i + j - 1) + 1.0;
                for (size_t k = i; k < j; ++k) r[idx[k]] = avg_rank;
                i = j;
            }
            return r;
        };
        auto rx = rank_vec(x);
        auto ry = rank_vec(y);
        return pearson_corr(rx, ry);
    }

    static double pearson_corr(const std::vector<double>& x, const std::vector<double>& y) {
        const size_t n = std::min(x.size(), y.size());
        if (n < 3) return 0.0;
        double sx = 0.0, sy = 0.0;
        for (size_t i = 0; i < n; ++i) {
            sx += x[i];
            sy += y[i];
        }
        const double mx = sx / static_cast<double>(n);
        const double my = sy / static_cast<double>(n);
        double num = 0.0, vx = 0.0, vy = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double dx = x[i] - mx;
            double dy = y[i] - my;
            num += dx * dy;
            vx += dx * dx;
            vy += dy * dy;
        }
        if (vx <= 1e-18 || vy <= 1e-18) return 0.0;
        return std::clamp(num / std::sqrt(vx * vy), -1.0, 1.0);
    }

    // Two-sided t-test p-value approximation accurate for both small & large N
    static SignalCorrelationStats evaluate_signal_quality(const std::vector<double>& signals,
                                                          const std::vector<double>& fwd_returns) {
        SignalCorrelationStats st;
        st.n = std::min(signals.size(), fwd_returns.size());
        if (st.n < 4) return st;

        st.pearson_r = pearson_corr(signals, fwd_returns);
        st.spearman_ic = spearman_rank_ic(signals, fwd_returns);
        const double r2 = std::min(0.999999, st.pearson_r * st.pearson_r);
        const double df = static_cast<double>(st.n - 2);
        st.t_stat = st.pearson_r * std::sqrt(df / std::max(1e-9, 1.0 - r2));

        // Numerical integration of Student-t density on [0, |t|] via Simpson's rule (exact to ~1e-8)
        const double abs_t = std::abs(st.t_stat);
        const double coeff = std::exp(std::lgamma(0.5 * (df + 1.0)) - std::lgamma(0.5 * df)) / std::sqrt(df * 3.14159265358979323846);
        const double hi = std::min(abs_t, 20.0);
        constexpr int kSteps = 200;
        const double h = hi / kSteps;
        auto pdf = [&](double u) { return coeff * std::pow(1.0 + (u * u) / df, -0.5 * (df + 1.0)); };
        double integ = pdf(0.0) + pdf(hi);
        for (int i = 1; i < kSteps; ++i) {
            integ += ((i % 2 == 0) ? 2.0 : 4.0) * pdf(i * h);
        }
        integ *= (h / 3.0);
        const double one_sided = std::max(0.0, 0.5 - integ);
        st.p_value = std::clamp(2.0 * one_sided, 0.0, 1.0);
        return st;
    }

    // Computes Accuracy, Zero-Rule, Lift, Macro-F1 & Gorodkin Multi-Class MCC for classes {-1, 0, +1}
    static ClassificationMetrics compute_classification_metrics(const std::vector<int>& y_true,
                                                                const std::vector<int>& y_pred,
                                                                const std::vector<double>* actual_ret = nullptr,
                                                                const std::vector<double>* pred_ret = nullptr) {
        ClassificationMetrics cm;
        const size_t n = std::min(y_true.size(), y_pred.size());
        cm.n_samples = n;
        if (n == 0) return cm;

        auto to_idx = [](int cls) -> int { return (cls < 0) ? 0 : ((cls == 0) ? 1 : 2); };
        double C[3][3]{};
        size_t correct = 0;
        size_t class_counts[3]{0, 0, 0};

        for (size_t i = 0; i < n; ++i) {
            int ti = to_idx(y_true[i]);
            int pi = to_idx(y_pred[i]);
            C[ti][pi] += 1.0;
            class_counts[static_cast<size_t>(ti)] += 1;
            if (ti == pi) ++correct;
        }
        cm.accuracy = static_cast<double>(correct) / static_cast<double>(n);
        size_t max_cls = *std::max_element(std::begin(class_counts), std::end(class_counts));
        cm.zero_rule = static_cast<double>(max_cls) / static_cast<double>(n);
        cm.lift = cm.accuracy - cm.zero_rule;

        // Macro F1 across classes present in y_true or y_pred
        double f1_sum = 0.0;
        int f1_present = 0;
        double t_k[3]{0, 0, 0}, p_k[3]{0, 0, 0};
        for (int k = 0; k < 3; ++k) {
            for (int j = 0; j < 3; ++j) {
                t_k[k] += C[k][j];
                p_k[k] += C[j][k];
            }
            if (t_k[k] > 0.0 || p_k[k] > 0.0) {
                double tp = C[k][k];
                double fp = p_k[k] - tp;
                double fn = t_k[k] - tp;
                double denom = 2.0 * tp + fp + fn;
                f1_sum += (denom > 0.0) ? (2.0 * tp / denom) : 0.0;
                ++f1_present;
            }
        }
        cm.f1_macro = (f1_present > 0) ? (f1_sum / static_cast<double>(f1_present)) : 0.0;

        // Gorodkin multi-class MCC
        const double s = static_cast<double>(n);
        const double c = static_cast<double>(correct);
        double sum_pk_tk = 0.0, sum_pk2 = 0.0, sum_tk2 = 0.0;
        for (int k = 0; k < 3; ++k) {
            sum_pk_tk += p_k[k] * t_k[k];
            sum_pk2 += p_k[k] * p_k[k];
            sum_tk2 += t_k[k] * t_k[k];
        }
        const double mcc_den = std::sqrt(std::max(0.0, (s * s - sum_pk2) * (s * s - sum_tk2)));
        cm.mcc = (mcc_den > 1e-12) ? ((c * s - sum_pk_tk) / mcc_den) : 0.0;

        // Directional hit rate excluding neutral predictions & zero returns
        if (actual_ret != nullptr && pred_ret != nullptr) {
            size_t hits = 0, tot = 0;
            const size_t m = std::min(n, std::min(actual_ret->size(), pred_ret->size()));
            for (size_t i = 0; i < m; ++i) {
                if (std::abs((*pred_ret)[i]) > 1e-7 && std::abs((*actual_ret)[i]) > 1e-7) {
                    if (((*pred_ret)[i] > 0.0) == ((*actual_ret)[i] > 0.0)) ++hits;
                    ++tot;
                }
            }
            cm.non_neutral_samples = tot;
            cm.dir_hit_rate_ex_neutral = (tot > 0) ? (static_cast<double>(hits) / static_cast<double>(tot)) : 0.5;
        }
        return cm;
    }

    // Pillar 3: Strict Look-Ahead Leakage & Feature Causality Auditor (mutates t+1..N-1 & asserts exact invariance at bar t)
    static CausalityAuditResult verify_feature_causality(const RealTimeAssetData& raw_asset,
                                                         int horizon = 5,
                                                         size_t lookahead_gap = 12) {
        CausalityAuditResult res;
        RealTimeAssetData data = raw_asset;
        if (data.historical_prices.size() < 80) {
            // Build deterministic realistic 100-bar OHLCV if caller passed too short series
            data = make_synthetic_asset_("AUDIT_SYN", 110, 42, 0.0004, 0.015);
        }
        const size_t n = data.historical_prices.size();
        const size_t gap = std::min<size_t>(lookahead_gap, n / 4);
        const size_t t = n - 1 - gap;
        res.test_bar_index = t;

        RealTimeAssetData mutated = data;
        for (size_t j = t + 1; j < n; ++j) {
            const double mult = (j % 2 == 0) ? 2.8 : 0.32;
            mutated.historical_prices[j] *= mult;
            if (j < mutated.historical_opens.size()) mutated.historical_opens[j] *= mult;
            if (j < mutated.historical_highs.size()) mutated.historical_highs[j] *= mult * 1.1;
            if (j < mutated.historical_lows.size()) mutated.historical_lows[j] *= mult * 0.9;
            if (j < mutated.historical_volumes.size()) mutated.historical_volumes[j] *= 25.0;
        }
        mutated.current_price = mutated.historical_prices.back();

        auto f_orig = QuantForecastEngine::inspect_causal_features_at(data, t, horizon);
        auto f_mut  = QuantForecastEngine::inspect_causal_features_at(mutated, t, horizon);
        if (!f_orig.valid || !f_mut.valid) {
            res.passed = false;
            res.detail = "Invalid bar range for causality audit";
            return res;
        }

        double max_fd = 0.0;
        auto track = [&](double a, double b) { max_fd = std::max(max_fd, std::abs(a - b)); };
        track(f_orig.rev5, f_mut.rev5);
        track(f_orig.rev10, f_mut.rev10);
        track(f_orig.mom20, f_mut.mom20);
        track(f_orig.mom60, f_mut.mom60);
        track(f_orig.rsi_rev, f_mut.rsi_rev);
        track(f_orig.er20, f_mut.er20);
        track(f_orig.flow10, f_mut.flow10);
        track(f_orig.clv3, f_mut.clv3);
        track(f_orig.vsp, f_mut.vsp);
        track(f_orig.sig_d, f_mut.sig_d);
        track(f_orig.composite, f_mut.composite);

        auto prefix_orig = slice_prefix_causal_(data, t);
        auto prefix_mut  = slice_prefix_causal_(mutated, t);
        auto fc_orig = QuantForecastEngine::forecast_for_trading_days(prefix_orig, horizon);
        auto fc_mut  = QuantForecastEngine::forecast_for_trading_days(prefix_mut, horizon);
        double max_fc = std::max(std::abs(fc_orig.expected_return_pct - fc_mut.expected_return_pct),
                                 std::abs(fc_orig.quant_score - fc_mut.quant_score));

        res.max_feature_diff = max_fd;
        res.max_forecast_diff = max_fc;
        res.passed = (max_fd < 1e-12 && max_fc < 1e-12);
        res.detail = std::format("bar_t={} | Δfeat={:.2e} | Δforecast={:.2e}", t, max_fd, max_fc);
        return res;
    }

    // Pillar 3: Synthetic Overfit & Temporal Sentiment Variance Pre-Deployment Sanity Suite
    static DiagnosticCheckSummary run_diagnostic_sanity_checks(const RealTimeAssetData* sample_asset = nullptr) {
        DiagnosticCheckSummary out;

        // 1. Look-Ahead Trailing Mutation Audit
        RealTimeAssetData syn_audit = sample_asset ? *sample_asset : make_synthetic_asset_("CAUSAL_CHK", 120, 101, 0.0005, 0.016);
        out.lookahead_audit = verify_feature_causality(syn_audit, 5, 15);

        // 2. Synthetic Overfit Sanity (deterministically learnable oscillating series -> model must achieve positive hit rate)
        auto learnable = make_oscillating_synthetic_asset_("OVERFIT_CHK", 160, 0.05);
        size_t hits = 0, total = 0;
        std::vector<double> sigs, rets;
        for (size_t t = 65; t + 5 < learnable.historical_prices.size(); t += 2) {
            auto sub = slice_prefix_causal_(learnable, t);
            auto fc = QuantForecastEngine::forecast_for_trading_days(sub, 5);
            double fwd = (learnable.historical_prices[t + 5] / learnable.historical_prices[t]) - 1.0;
            sigs.push_back(fc.expected_return_pct / 100.0);
            rets.push_back(fwd);
            if (((fc.expected_return_pct > 0.0) == (fwd > 0.0))) ++hits;
            ++total;
        }
        out.synthetic_overfit_dir_acc = (total > 0) ? (static_cast<double>(hits) / static_cast<double>(total)) : 0.0;
        out.synthetic_overfit_passed = (out.synthetic_overfit_dir_acc >= 0.58);
        out.signal_quality = evaluate_signal_quality(sigs, rets);

        // 3. Temporal Sentiment Variance Check (must distinguish scandal regime vs neutral vs optimistic days)
        int64_t day0 = 20100LL * 86400LL;
        std::vector<double> daily_scores;
        std::vector<StickyHeadlineObs> stream = {
            {day0 + 3600, 0.65, 1.0},
            {day0 + 86400 + 3600, -0.82, 1.0},
            {day0 + 2 * 86400 + 3600, 0.00, 1.0}, // neutral weekend fluff
            {day0 + 5 * 86400 + 3600, 0.45, 1.0}
        };
        for (int k = 1; k <= static_cast<int>(stream.size()); ++k) {
            std::vector<StickyHeadlineObs> pref(stream.begin(), stream.begin() + k);
            auto st = NewsSentimentEngine::compute_sticky_sentiment(pref, pref.back().publish_time + 3600);
            daily_scores.push_back(st.sticky_score);
        }
        double mean_s = std::accumulate(daily_scores.begin(), daily_scores.end(), 0.0) / static_cast<double>(daily_scores.size());
        double var_s = 0.0;
        for (double v : daily_scores) var_s += (v - mean_s) * (v - mean_s);
        out.temporal_sentiment_std = std::sqrt(var_s / static_cast<double>(daily_scores.size()));
        // Net sticky score on neutral weekend after +0.65 (d0) & -0.82 (d1) shock stays strongly negative
        out.temporal_sentiment_var_passed = (out.temporal_sentiment_std > 0.15 && daily_scores[2] < -0.25);
        return out;
    }

    // ============================================================================
    // Main Cross-Sectional Multi-Asset Backtest + Fake-Alpha + Predictability Runner
    // ============================================================================
    static CrossSectionalReport run_backtest(const std::vector<RealTimeAssetData>& raw_assets,
                                             CrossSectionalConfig cfg,
                                             const std::unordered_map<std::string, double>* asset_sentiments = nullptr) {
        CrossSectionalReport rep;
        rep.config = cfg;

        // 1. Clean Equity Universe Hygiene (strip ETFs, warrants, short series)
        std::vector<RealTimeAssetData> universe;
        universe.reserve(raw_assets.size());
        for (const auto& a : raw_assets) {
            if (cfg.exclude_etfs_warrants && !is_clean_equity(a)) {
                ++rep.excluded_etf_count;
                continue;
            }
            if (a.historical_prices.size() < cfg.warmup_bars + static_cast<size_t>(std::max(1, cfg.horizon_days)) + 5) {
                continue;
            }
            universe.push_back(a);
            if (a.is_live) rep.is_live_data = true;
        }
        rep.universe_size = static_cast<int>(universe.size());
        if (universe.size() < 4) return rep;

        // Clamp top_n sensibly relative to universe size
        cfg.top_n = std::clamp(cfg.top_n, 1, std::max(1, static_cast<int>(universe.size()) / 2));
        cfg.horizon_days = std::clamp(cfg.horizon_days, 1, 21);
        cfg.rebalance_days = std::clamp(cfg.rebalance_days, 1, 21);
        rep.config = cfg;

        // 2. Date Alignment Index: day_key -> bar_index for each asset
        const size_t n_assets = universe.size();
        std::vector<std::map<int64_t, size_t>> day_maps(n_assets);
        std::set<int64_t> all_day_keys;
        std::unordered_map<int64_t, std::string> day_str_map;

        for (size_t a = 0; a < n_assets; ++a) {
            const auto& d = universe[a];
            for (size_t i = 0; i < d.historical_prices.size(); ++i) {
                int64_t dk = (i < d.day_keys.size() && d.day_keys[i] > 0)
                    ? d.day_keys[i]
                    : ((i < d.timestamps.size() && d.timestamps[i] > 0) ? (d.timestamps[i] / 86400LL) : static_cast<int64_t>(i));
                day_maps[a][dk] = i;
                all_day_keys.insert(dk);
                if (!day_str_map.contains(dk)) {
                    int64_t ts = (i < d.timestamps.size()) ? d.timestamps[i] : 0;
                    day_str_map[dk] = format_epoch_date(ts, dk);
                }
            }
        }

        // 3. Per-Date Strictly Causal Panel Construction
        struct PanelObs {
            size_t asset_idx{0};
            double pred_ret{0.0};
            double quant_score{0.0};
            double vol_20d{0.20};
            double exec_price{0.0};
            double fwd_ret_h{0.0};
            double step_1d_ret{0.0};
        };
        struct DateSlice {
            int64_t day_key{0};
            std::string date_str;
            std::vector<PanelObs> obs;
        };

        std::vector<DateSlice> panel;
        const size_t H = static_cast<size_t>(cfg.horizon_days);
        const size_t min_active = std::max<size_t>(4, (n_assets * 2) / 3);
        const size_t win_len = 260;

        for (int64_t dk : all_day_keys) {
            DateSlice slice;
            slice.day_key = dk;
            slice.date_str = day_str_map[dk];

            for (size_t a = 0; a < n_assets; ++a) {
                auto it = day_maps[a].find(dk);
                if (it == day_maps[a].end()) continue;
                const size_t idx = it->second;
                const auto& d = universe[a];
                if (idx < cfg.warmup_bars) continue;

                const bool has_open = (cfg.use_next_open_execution &&
                                       d.historical_opens.size() == d.historical_prices.size());
                const size_t entry_offset = has_open ? 1 : 0;
                if (idx + entry_offset + H >= d.historical_prices.size()) continue;
                if (idx + entry_offset + 1 >= d.historical_prices.size()) continue;

                const double p_entry = has_open ? d.historical_opens[idx + 1] : d.historical_prices[idx];
                const double p_exit_h = has_open ? d.historical_opens[idx + 1 + H] : d.historical_prices[idx + H];
                const double p_exit_1 = has_open ? d.historical_opens[idx + 2] : d.historical_prices[idx + 1];
                if (p_entry <= 0.0 || p_exit_h <= 0.0 || p_exit_1 <= 0.0) continue;

                // Strictly causal trailing window ending at idx
                auto sub = slice_window_causal_(d, idx, win_len);
                double vol_20d = compute_trailing_vol_(d.historical_prices, idx, 20);
                if (vol_20d > 0.0) sub.annualized_volatility = vol_20d;

                double sent = 0.0;
                bool has_news = false;
                if (cfg.use_live_sentiment && asset_sentiments != nullptr) {
                    auto sit = asset_sentiments->find(d.symbol);
                    if (sit != asset_sentiments->end()) {
                        sent = sit->second;
                        has_news = (std::abs(sent) > 1e-6);
                    }
                }
                auto fc = QuantForecastEngine::forecast_for_trading_days(sub, cfg.horizon_days, sent, has_news);

                PanelObs po;
                po.asset_idx = a;
                po.pred_ret = fc.expected_return_pct / 100.0;
                po.quant_score = fc.quant_score;
                po.vol_20d = (vol_20d > 0.0) ? vol_20d : sub.annualized_volatility;
                po.exec_price = p_entry;
                po.fwd_ret_h = (p_exit_h / p_entry) - 1.0;
                po.step_1d_ret = (p_exit_1 / p_entry) - 1.0;
                slice.obs.push_back(po);
            }
            if (slice.obs.size() >= min_active) {
                panel.push_back(std::move(slice));
            }
        }

        if (panel.size() < 20) return rep;
        rep.evaluated_days = static_cast<int>(panel.size());
        rep.start_date = panel.front().date_str;
        rep.end_date = panel.back().date_str;

        // 4. Daily Cross-Sectional IC, Rolling-20, Quantile Spread & Fake-Alpha Bias Correlations
        double cum_spread = 0.0;
        double sum_pv_corr = 0.0, sum_abspv_corr = 0.0, sum_hv_ic = 0.0;
        int n_hv_days = 0;

        std::vector<double> global_pred, global_ret;
        std::vector<int> global_ytrue_cls, global_ypred_cls;
        std::vector<std::vector<double>> stock_preds(n_assets), stock_rets(n_assets);
        std::vector<std::vector<int>> stock_ytrue(n_assets), stock_ypred(n_assets);

        auto to_cls = [&](double r) -> int {
            if (r > cfg.neutral_band_ret) return 1;
            if (r < -cfg.neutral_band_ret) return -1;
            return 0;
        };

        for (size_t d_i = 0; d_i < panel.size(); ++d_i) {
            const auto& sl = panel[d_i];
            std::vector<double> preds, rets, vols, abs_preds;
            preds.reserve(sl.obs.size());
            rets.reserve(sl.obs.size());
            vols.reserve(sl.obs.size());
            abs_preds.reserve(sl.obs.size());

            for (const auto& o : sl.obs) {
                preds.push_back(o.pred_ret);
                rets.push_back(o.fwd_ret_h);
                vols.push_back(o.vol_20d);
                abs_preds.push_back(std::abs(o.pred_ret));

                global_pred.push_back(o.pred_ret);
                global_ret.push_back(o.fwd_ret_h);
                global_ytrue_cls.push_back(to_cls(o.fwd_ret_h));
                global_ypred_cls.push_back(to_cls(o.pred_ret));

                stock_preds[o.asset_idx].push_back(o.pred_ret);
                stock_rets[o.asset_idx].push_back(o.fwd_ret_h);
                stock_ytrue[o.asset_idx].push_back(to_cls(o.fwd_ret_h));
                stock_ypred[o.asset_idx].push_back(to_cls(o.pred_ret));
            }

            DailyCrossSectionalIC dic;
            dic.day_key = sl.day_key;
            dic.date_str = sl.date_str;
            dic.n_stocks = static_cast<int>(sl.obs.size());
            dic.ic = spearman_rank_ic(preds, rets);

            // Top-Bottom Quantile Spread (Q5 - Q1)
            std::vector<size_t> rank_ord(sl.obs.size());
            std::iota(rank_ord.begin(), rank_ord.end(), 0);
            std::sort(rank_ord.begin(), rank_ord.end(), [&](size_t a, size_t b) {
                return sl.obs[a].pred_ret > sl.obs[b].pred_ret;
            });
            const size_t q_cnt = std::max<size_t>(1, static_cast<size_t>(std::llround(sl.obs.size() * cfg.top_pct)));
            double top_sum = 0.0, bot_sum = 0.0;
            for (size_t k = 0; k < q_cnt; ++k) {
                top_sum += sl.obs[rank_ord[k]].fwd_ret_h;
                bot_sum += sl.obs[rank_ord[sl.obs.size() - 1 - k]].fwd_ret_h;
            }
            dic.q_spread = (top_sum - bot_sum) / static_cast<double>(q_cnt);
            cum_spread += dic.q_spread;
            dic.cumulative_spread = cum_spread;

            dic.pred_vol_corr = spearman_rank_ic(preds, vols);
            dic.abs_pred_vol_corr = spearman_rank_ic(abs_preds, vols);
            sum_pv_corr += dic.pred_vol_corr;
            sum_abspv_corr += dic.abs_pred_vol_corr;

            // High-Vol Pool IC (stocks with vol >= median vol)
            std::vector<double> vols_sorted = vols;
            std::sort(vols_sorted.begin(), vols_sorted.end());
            double med_vol = vols_sorted[vols_sorted.size() / 2];
            std::vector<double> hv_p, hv_r;
            for (size_t k = 0; k < sl.obs.size(); ++k) {
                if (vols[k] >= med_vol) {
                    hv_p.push_back(preds[k]);
                    hv_r.push_back(rets[k]);
                }
            }
            if (hv_p.size() >= 3) {
                dic.high_vol_pool_ic = spearman_rank_ic(hv_p, hv_r);
                sum_hv_ic += dic.high_vol_pool_ic;
                ++n_hv_days;
            }

            // Rolling 20-day mean IC
            rep.daily_ic_series.push_back(dic);
            size_t r_start = (rep.daily_ic_series.size() > 20) ? (rep.daily_ic_series.size() - 20) : 0;
            double r_sum = 0.0;
            for (size_t j = r_start; j < rep.daily_ic_series.size(); ++j) r_sum += rep.daily_ic_series[j].ic;
            rep.daily_ic_series.back().ic_rolling_20 = r_sum / static_cast<double>(rep.daily_ic_series.size() - r_start);
        }

        rep.total_observations = global_pred.size();
        const double n_days_d = static_cast<double>(rep.daily_ic_series.size());
        double ic_sum = 0.0, q_sum = 0.0;
        int pos_ic_days = 0;
        for (const auto& d : rep.daily_ic_series) {
            ic_sum += d.ic;
            q_sum += d.q_spread;
            if (d.ic > 0.0) ++pos_ic_days;
        }
        rep.mean_cs_ic = ic_sum / n_days_d;
        rep.mean_q_spread = q_sum / n_days_d;
        rep.cumulative_q_spread = cum_spread;
        rep.pct_positive_ic = (static_cast<double>(pos_ic_days) / n_days_d) * 100.0;

        double ic_var = 0.0;
        for (const auto& d : rep.daily_ic_series) {
            double diff = d.ic - rep.mean_cs_ic;
            ic_var += diff * diff;
        }
        rep.ic_std = std::sqrt(ic_var / std::max(1.0, n_days_d - 1.0));
        rep.ic_ir = (rep.ic_std > 1e-9) ? (rep.mean_cs_ic / rep.ic_std) : 0.0;

        rep.global_wf_ic = spearman_rank_ic(global_pred, global_ret);
        rep.global_clf = compute_classification_metrics(global_ytrue_cls, global_ypred_cls, &global_ret, &global_pred);
        {
            double sse = 0.0, sst = 0.0;
            double mean_y = std::accumulate(global_ret.begin(), global_ret.end(), 0.0) / static_cast<double>(global_ret.size());
            for (size_t i = 0; i < global_ret.size(); ++i) {
                double e = global_ret[i] - global_pred[i];
                double d = global_ret[i] - mean_y;
                sse += e * e;
                sst += d * d;
            }
            rep.global_mse = sse / static_cast<double>(global_ret.size());
            rep.global_r2 = (sst > 1e-12) ? (1.0 - sse / sst) : 0.0;
        }

        rep.skill.avg_pred_vol_corr = sum_pv_corr / n_days_d;
        rep.skill.avg_abs_pred_vol_corr = sum_abspv_corr / n_days_d;
        rep.skill.high_vol_pool_ic = (n_hv_days > 0) ? (sum_hv_ic / static_cast<double>(n_hv_days)) : 0.0;

        // 5. Simulate All Portfolio Strategies + Monte Carlo Fake-Alpha Controls (with Drifted Weights + Turnover Cost)
        auto sim_strategy = [&](const std::string& name,
                                bool is_model,
                                auto weight_fn,
                                bool record_logs,
                                std::vector<double>* out_daily_rets = nullptr,
                                std::vector<std::vector<double>>* out_asset_contribs = nullptr,
                                std::vector<int>* out_pick_counts = nullptr,
                                std::vector<double>* out_picked_rets = nullptr,
                                std::vector<double>* out_not_picked_rets = nullptr) -> StrategyPerformanceRow {
            StrategyPerformanceRow row;
            row.strategy_name = name;
            row.is_model = is_model;

            std::unordered_map<size_t, double> cur_weights;
            double nav = 1.0;
            double peak_nav = 1.0;
            double max_dd = 0.0;
            double turnover_sum = 0.0;
            int win_days = 0;
            std::vector<double> daily_rets;
            daily_rets.reserve(panel.size());
            if (out_asset_contribs) out_asset_contribs->assign(n_assets, std::vector<double>(panel.size(), 0.0));
            if (out_pick_counts) out_pick_counts->assign(n_assets, 0);

            std::set<size_t> prev_selected;
            double bench_nav = 1.0;

            for (size_t d_i = 0; d_i < panel.size(); ++d_i) {
                const auto& sl = panel[d_i];
                const bool do_rebal = (d_i == 0) || (d_i % static_cast<size_t>(cfg.rebalance_days) == 0);
                double tc_drag = 0.0;

                if (do_rebal || cur_weights.empty()) {
                    auto target = weight_fn(sl, d_i);
                    double turn = 0.0;
                    std::set<size_t> all_keys_w;
                    for (const auto& [k, _] : cur_weights) all_keys_w.insert(k);
                    for (const auto& [k, _] : target) all_keys_w.insert(k);
                    for (size_t k : all_keys_w) {
                        double w_old = cur_weights.contains(k) ? cur_weights[k] : 0.0;
                        double w_new = target.contains(k) ? target[k] : 0.0;
                        turn += std::abs(w_new - w_old);
                    }
                    double one_way = 0.5 * turn;
                    turnover_sum += one_way;
                    tc_drag = one_way * (cfg.tc_bps / 10000.0);

                    if (record_logs) {
                        std::set<size_t> new_selected;
                        for (const auto& [k, w] : target) {
                            if (w > 0.0) new_selected.insert(k);
                        }
                        for (const auto& o : sl.obs) {
                            bool in_new = new_selected.contains(o.asset_idx);
                            bool in_old = prev_selected.contains(o.asset_idx);
                            if (in_new && !in_old) {
                                rep.topn_trades.push_back({sl.date_str, universe[o.asset_idx].symbol, "BUY",
                                                           o.exec_price, o.pred_ret, o.fwd_ret_h, o.step_1d_ret * 100.0});
                            } else if (!in_new && in_old) {
                                rep.topn_trades.push_back({sl.date_str, universe[o.asset_idx].symbol, "SELL",
                                                           o.exec_price, o.pred_ret, o.fwd_ret_h, o.step_1d_ret * 100.0});
                            }
                        }
                        prev_selected = std::move(new_selected);
                    }
                    cur_weights = std::move(target);
                }

                // Mark-to-market daily return & weight drift
                double gross_ret = 0.0;
                double bench_day_ret = 0.0;
                std::unordered_map<size_t, double> next_weights;
                double denom_sum = 0.0;

                for (const auto& o : sl.obs) {
                    bench_day_ret += o.step_1d_ret / static_cast<double>(sl.obs.size());
                    auto wit = cur_weights.find(o.asset_idx);
                    if (wit != cur_weights.end() && std::abs(wit->second) > 1e-12) {
                        double c = wit->second * o.step_1d_ret;
                        gross_ret += c;
                        if (out_asset_contribs) (*out_asset_contribs)[o.asset_idx][d_i] = c;
                        if (out_pick_counts && wit->second > 0.0) (*out_pick_counts)[o.asset_idx] += 1;
                        if (out_picked_rets && wit->second > 0.0) out_picked_rets->push_back(o.step_1d_ret);

                        double grown = wit->second * (1.0 + o.step_1d_ret);
                        next_weights[o.asset_idx] = grown;
                        denom_sum += std::abs(grown);
                    } else if (out_not_picked_rets) {
                        out_not_picked_rets->push_back(o.step_1d_ret);
                    }
                }

                if (denom_sum > 1e-9) {
                    for (auto& [k, w] : next_weights) w /= denom_sum;
                }
                cur_weights = std::move(next_weights);

                const double net_ret = gross_ret - tc_drag;
                nav *= std::max(0.01, 1.0 + net_ret);
                bench_nav *= std::max(0.01, 1.0 + bench_day_ret);
                peak_nav = std::max(peak_nav, nav);
                max_dd = std::min(max_dd, (nav / peak_nav) - 1.0);
                if (net_ret > 0.0) ++win_days;
                daily_rets.push_back(net_ret);

                if (record_logs) {
                    std::string picks_str;
                    int p_cnt = 0;
                    for (const auto& [k, w] : cur_weights) {
                        if (w > 0.0 && k < universe.size()) {
                            if (p_cnt++ > 0) picks_str += "|";
                            picks_str += universe[k].symbol;
                        }
                    }
                    rep.topn_holdings.push_back({sl.date_str, picks_str, nav, net_ret, bench_nav});
                }
            }

            if (out_daily_rets) *out_daily_rets = daily_rets;
            const double nd = static_cast<double>(daily_rets.size());
            row.total_return = nav - 1.0;
            row.annualized_return = std::pow(std::max(1e-4, nav), 252.0 / std::max(1.0, nd)) - 1.0;
            row.max_drawdown = max_dd;
            row.daily_win_rate = (nd > 0.0) ? (static_cast<double>(win_days) / nd) : 0.0;
            row.total_turnover = turnover_sum;

            double mean_r = std::accumulate(daily_rets.begin(), daily_rets.end(), 0.0) / std::max(1.0, nd);
            double var_r = 0.0, dvar_r = 0.0;
            for (double r : daily_rets) {
                var_r += (r - mean_r) * (r - mean_r);
                if (r < 0.0) dvar_r += r * r;
            }
            double sd_r = std::sqrt(var_r / std::max(1.0, nd - 1.0));
            double dsd_r = std::sqrt(dvar_r / std::max(1.0, nd));
            row.sharpe_ratio = (sd_r > 1e-12) ? ((mean_r / sd_r) * std::sqrt(252.0)) : 0.0;
            row.sortino_ratio = (dsd_r > 1e-12) ? ((mean_r / dsd_r) * std::sqrt(252.0)) : 0.0;
            return row;
        };

        std::vector<double> model_topn_rets, picked_rets, not_picked_rets;
        std::vector<std::vector<double>> topn_asset_contribs;
        std::vector<int> topn_pick_counts;

        // S1: Model Top-N
        auto s_model_topn = sim_strategy(
            std::format("QuantForecast Top-{}", cfg.top_n), true,
            [&](const DateSlice& sl, size_t) {
                std::vector<size_t> ord(sl.obs.size());
                std::iota(ord.begin(), ord.end(), 0);
                std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                    return sl.obs[a].pred_ret > sl.obs[b].pred_ret;
                });
                size_t k = std::min<size_t>(static_cast<size_t>(cfg.top_n), ord.size());
                std::unordered_map<size_t, double> w;
                for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                return w;
            },
            true, &model_topn_rets, &topn_asset_contribs, &topn_pick_counts, &picked_rets, &not_picked_rets);

        // S2: Model Top-Pct Long Only
        auto s_model_pct = sim_strategy(
            std::format("QuantForecast Long Top-{:.0f}%", cfg.top_pct * 100.0), true,
            [&](const DateSlice& sl, size_t) {
                std::vector<size_t> ord(sl.obs.size());
                std::iota(ord.begin(), ord.end(), 0);
                std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                    return sl.obs[a].pred_ret > sl.obs[b].pred_ret;
                });
                size_t k = std::max<size_t>(1, static_cast<size_t>(std::llround(sl.obs.size() * cfg.top_pct)));
                std::unordered_map<size_t, double> w;
                for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                return w;
            },
            false);

        // S3: Model Long/Short Market-Neutral
        auto s_model_ls = sim_strategy(
            "QuantForecast Long/Short Neutral", true,
            [&](const DateSlice& sl, size_t) {
                std::vector<size_t> ord(sl.obs.size());
                std::iota(ord.begin(), ord.end(), 0);
                std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                    return sl.obs[a].pred_ret > sl.obs[b].pred_ret;
                });
                size_t k = std::max<size_t>(1, static_cast<size_t>(std::llround(sl.obs.size() * cfg.top_pct)));
                std::unordered_map<size_t, double> w;
                for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 0.5 / static_cast<double>(k);
                for (size_t i = 0; i < k; ++i) w[sl.obs[ord[ord.size() - 1 - i]].asset_idx] = -0.5 / static_cast<double>(k);
                return w;
            },
            false);

        // S4: Fake-Alpha Control — Vol Top-N (no model, picks highest trailing 20d vol stocks!)
        auto s_vol_topn = sim_strategy(
            std::format("Control: Vol Top-{}", cfg.top_n), false,
            [&](const DateSlice& sl, size_t) {
                std::vector<size_t> ord(sl.obs.size());
                std::iota(ord.begin(), ord.end(), 0);
                std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                    return sl.obs[a].vol_20d > sl.obs[b].vol_20d;
                });
                size_t k = std::min<size_t>(static_cast<size_t>(cfg.top_n), ord.size());
                std::unordered_map<size_t, double> w;
                for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                return w;
            },
            false);

        // S5 & S6 & S7: Monte Carlo Random & Shuffled-Signal Controls
        auto avg_mc_strategy = [&](const std::string& name, auto mc_weight_factory) {
            StrategyPerformanceRow agg;
            agg.strategy_name = name;
            agg.is_model = false;
            const int trials = std::max(4, cfg.monte_carlo_trials);
            for (int t = 0; t < trials; ++t) {
                std::mt19937_64 rng(202600ULL + static_cast<uint64_t>(t) * 7919ULL);
                auto res_t = sim_strategy(name, false, mc_weight_factory(rng), false);
                agg.total_return += res_t.total_return / trials;
                agg.annualized_return += res_t.annualized_return / trials;
                agg.sharpe_ratio += res_t.sharpe_ratio / trials;
                agg.sortino_ratio += res_t.sortino_ratio / trials;
                agg.max_drawdown += res_t.max_drawdown / trials;
                agg.daily_win_rate += res_t.daily_win_rate / trials;
                agg.total_turnover += res_t.total_turnover / trials;
            }
            return agg;
        };

        auto s_rand_vol = avg_mc_strategy(
            std::format("Control: Rand-from-VolTopK (Avg {})", std::max(4, cfg.monte_carlo_trials)),
            [&](std::mt19937_64& rng) {
                return [&](const DateSlice& sl, size_t) {
                    std::vector<size_t> ord(sl.obs.size());
                    std::iota(ord.begin(), ord.end(), 0);
                    std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                        return sl.obs[a].vol_20d > sl.obs[b].vol_20d;
                    });
                    size_t pool = std::min<size_t>(std::max<size_t>(static_cast<size_t>(cfg.top_n) * 2, 2), ord.size());
                    std::shuffle(ord.begin(), ord.begin() + static_cast<std::ptrdiff_t>(pool), rng);
                    size_t k = std::min<size_t>(static_cast<size_t>(cfg.top_n), pool);
                    std::unordered_map<size_t, double> w;
                    for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                    return w;
                };
            });

        auto s_rand_mod = avg_mc_strategy(
            std::format("Control: Rand-from-ModelTopK (Avg {})", std::max(4, cfg.monte_carlo_trials)),
            [&](std::mt19937_64& rng) {
                return [&](const DateSlice& sl, size_t) {
                    std::vector<size_t> ord(sl.obs.size());
                    std::iota(ord.begin(), ord.end(), 0);
                    std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) {
                        return sl.obs[a].pred_ret > sl.obs[b].pred_ret;
                    });
                    size_t pool = std::min<size_t>(std::max<size_t>(static_cast<size_t>(cfg.top_n) * 2, 2), ord.size());
                    std::shuffle(ord.begin(), ord.begin() + static_cast<std::ptrdiff_t>(pool), rng);
                    size_t k = std::min<size_t>(static_cast<size_t>(cfg.top_n), pool);
                    std::unordered_map<size_t, double> w;
                    for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                    return w;
                };
            });

        auto s_shuffled_sig = avg_mc_strategy(
            std::format("Control: Shuffled Signal (Avg {})", std::max(4, cfg.monte_carlo_trials)),
            [&](std::mt19937_64& rng) {
                return [&](const DateSlice& sl, size_t) {
                    std::vector<size_t> ord(sl.obs.size());
                    std::iota(ord.begin(), ord.end(), 0);
                    std::shuffle(ord.begin(), ord.end(), rng);
                    size_t k = std::min<size_t>(static_cast<size_t>(cfg.top_n), ord.size());
                    std::unordered_map<size_t, double> w;
                    for (size_t i = 0; i < k; ++i) w[sl.obs[ord[i]].asset_idx] = 1.0 / static_cast<double>(k);
                    return w;
                };
            });

        // S8 & S9: Daily Rebalanced & Buy-and-Hold Benchmarks
        auto s_daily_rebal = sim_strategy(
            "Baseline: Daily Rebalanced", false,
            [&](const DateSlice& sl, size_t) {
                std::unordered_map<size_t, double> w;
                for (const auto& o : sl.obs) w[o.asset_idx] = 1.0 / static_cast<double>(sl.obs.size());
                return w;
            },
            false);

        // Passive Buy & Hold (rebalance only day 0, let weights drift without paying turnover)
        StrategyPerformanceRow s_bnh;
        {
            CrossSectionalConfig bnh_cfg = cfg;
            bnh_cfg.rebalance_days = 1000000;
            double bnh_nav = 1.0, bnh_peak = 1.0, bnh_dd = 0.0;
            int bnh_wins = 0;
            std::vector<double> bnh_rets;
            std::unordered_map<size_t, double> w_bnh;
            for (size_t d_i = 0; d_i < panel.size(); ++d_i) {
                const auto& sl = panel[d_i];
                if (d_i == 0) {
                    for (const auto& o : sl.obs) w_bnh[o.asset_idx] = 1.0 / static_cast<double>(sl.obs.size());
                }
                double dr = 0.0, dsum = 0.0;
                std::unordered_map<size_t, double> nw;
                for (const auto& o : sl.obs) {
                    double w = w_bnh.contains(o.asset_idx) ? w_bnh[o.asset_idx] : 0.0;
                    dr += w * o.step_1d_ret;
                    double gw = w * (1.0 + o.step_1d_ret);
                    nw[o.asset_idx] = gw;
                    dsum += gw;
                }
                if (dsum > 1e-9) {
                    for (auto& [k, v] : nw) v /= dsum;
                }
                w_bnh = std::move(nw);
                bnh_nav *= std::max(0.01, 1.0 + dr);
                bnh_peak = std::max(bnh_peak, bnh_nav);
                bnh_dd = std::min(bnh_dd, (bnh_nav / bnh_peak) - 1.0);
                if (dr > 0.0) ++bnh_wins;
                bnh_rets.push_back(dr);
            }
            const double nd = static_cast<double>(bnh_rets.size());
            s_bnh.strategy_name = "Baseline: Buy & Hold";
            s_bnh.is_model = false;
            s_bnh.total_return = bnh_nav - 1.0;
            s_bnh.annualized_return = std::pow(std::max(1e-4, bnh_nav), 252.0 / std::max(1.0, nd)) - 1.0;
            s_bnh.max_drawdown = bnh_dd;
            s_bnh.daily_win_rate = (nd > 0.0) ? (static_cast<double>(bnh_wins) / nd) : 0.0;
            double m = std::accumulate(bnh_rets.begin(), bnh_rets.end(), 0.0) / std::max(1.0, nd);
            double v = 0.0, dv = 0.0;
            for (double r : bnh_rets) {
                v += (r - m) * (r - m);
                if (r < 0.0) dv += r * r;
            }
            double sd = std::sqrt(v / std::max(1.0, nd - 1.0));
            double dsd = std::sqrt(dv / std::max(1.0, nd));
            s_bnh.sharpe_ratio = (sd > 1e-12) ? ((m / sd) * std::sqrt(252.0)) : 0.0;
            s_bnh.sortino_ratio = (dsd > 1e-12) ? ((m / dsd) * std::sqrt(252.0)) : 0.0;
        }

        rep.strategies = {
            s_model_topn, s_model_pct, s_model_ls,
            s_vol_topn, s_rand_vol, s_rand_mod, s_shuffled_sig,
            s_daily_rebal, s_bnh
        };
        for (auto& st : rep.strategies) {
            st.excess_vs_bnh = st.total_return - s_bnh.total_return;
        }

        // 6. Fake-Alpha Skill & Timing / Robustness Diagnostics
        rep.skill.model_topn_sharpe = s_model_topn.sharpe_ratio;
        rep.skill.vol_topn_sharpe = s_vol_topn.sharpe_ratio;
        rep.skill.rand_vol_topk_sharpe = s_rand_vol.sharpe_ratio;
        rep.skill.rand_model_topk_sharpe = s_rand_mod.sharpe_ratio;
        rep.skill.shuffled_signal_sharpe = s_shuffled_sig.sharpe_ratio;

        rep.skill.excess_sharpe_vs_vol = s_model_topn.sharpe_ratio - s_vol_topn.sharpe_ratio;
        rep.skill.excess_sharpe_vs_rand_vol = s_model_topn.sharpe_ratio - s_rand_vol.sharpe_ratio;
        rep.skill.excess_sharpe_vs_shuffled = s_model_topn.sharpe_ratio - s_shuffled_sig.sharpe_ratio;

        if (!picked_rets.empty()) {
            double mp = std::accumulate(picked_rets.begin(), picked_rets.end(), 0.0) / static_cast<double>(picked_rets.size());
            rep.skill.mean_ret_picked_ann = mp * 252.0 * 100.0;
        }
        if (!not_picked_rets.empty()) {
            double mnp = std::accumulate(not_picked_rets.begin(), not_picked_rets.end(), 0.0) / static_cast<double>(not_picked_rets.size());
            rep.skill.mean_ret_not_picked_ann = mnp * 252.0 * 100.0;
        }
        rep.skill.timing_alpha_pct = rep.skill.mean_ret_picked_ann - rep.skill.mean_ret_not_picked_ann;

        // Per-Stock Predictability Matrix & Attribution (Pillar 4)
        std::vector<double> asset_cum_contrib(n_assets, 0.0);
        double total_topn_contrib = 0.0;
        for (size_t a = 0; a < n_assets; ++a) {
            for (double c : topn_asset_contribs[a]) asset_cum_contrib[a] += c;
            total_topn_contrib += asset_cum_contrib[a];
        }
        for (size_t a = 0; a < n_assets; ++a) {
            StockPredictabilityRow sr;
            sr.ticker = universe[a].symbol;
            sr.n_samples = stock_rets[a].size();
            if (sr.n_samples < 5) continue;

            sr.ic = spearman_rank_ic(stock_preds[a], stock_rets[a]);
            auto cm = compute_classification_metrics(stock_ytrue[a], stock_ypred[a], &stock_rets[a], &stock_preds[a]);
            sr.directional_accuracy = cm.accuracy;
            sr.dir_hit_rate_ex_neutral = cm.dir_hit_rate_ex_neutral;
            sr.zero_rule = cm.zero_rule;
            sr.lift = cm.lift;
            sr.mcc = cm.mcc;
            sr.f1_macro = cm.f1_macro;

            double sse = 0.0, sst = 0.0;
            double ym = std::accumulate(stock_rets[a].begin(), stock_rets[a].end(), 0.0) / static_cast<double>(sr.n_samples);
            for (size_t i = 0; i < sr.n_samples; ++i) {
                double e = stock_rets[a][i] - stock_preds[a][i];
                double d = stock_rets[a][i] - ym;
                sse += e * e;
                sst += d * d;
            }
            sr.mse = sse / static_cast<double>(sr.n_samples);
            sr.r2 = (sst > 1e-12) ? (1.0 - sse / sst) : 0.0;
            sr.topn_picks = topn_pick_counts[a];
            sr.topn_contrib_pct = (std::abs(total_topn_contrib) > 1e-9)
                ? (asset_cum_contrib[a] / std::abs(total_topn_contrib)) * 100.0
                : (asset_cum_contrib[a] * 100.0);

            // Stock-level timing spread (days picked vs days not picked)
            double sp = 0.0, snp = 0.0;
            size_t cp = 0, cnp = 0;
            for (size_t d_i = 0; d_i < panel.size(); ++d_i) {
                for (const auto& o : panel[d_i].obs) {
                    if (o.asset_idx != a) continue;
                    if (std::abs(topn_asset_contribs[a][d_i]) > 1e-12) {
                        sp += o.step_1d_ret;
                        ++cp;
                    } else {
                        snp += o.step_1d_ret;
                        ++cnp;
                    }
                }
            }
            sr.ret_when_picked_ann = (cp > 0) ? ((sp / cp) * 252.0 * 100.0) : 0.0;
            sr.ret_when_not_picked_ann = (cnp > 0) ? ((snp / cnp) * 252.0 * 100.0) : 0.0;
            sr.timing_spread_pct = sr.ret_when_picked_ann - sr.ret_when_not_picked_ann;
            rep.per_stock.push_back(sr);
        }
        std::sort(rep.per_stock.begin(), rep.per_stock.end(),
                  [](const StockPredictabilityRow& a, const StockPredictabilityRow& b) {
                      return a.ic > b.ic;
                  });

        // Robustness Test 1: Drop Top-3 contributor tickers from daily portfolio return
        std::vector<size_t> top_contrib_idx(n_assets);
        std::iota(top_contrib_idx.begin(), top_contrib_idx.end(), 0);
        std::sort(top_contrib_idx.begin(), top_contrib_idx.end(), [&](size_t a, size_t b) {
            return asset_cum_contrib[a] > asset_cum_contrib[b];
        });
        std::set<size_t> drop_top3;
        for (size_t i = 0; i < std::min<size_t>(3, n_assets); ++i) drop_top3.insert(top_contrib_idx[i]);

        std::vector<double> rets_ex_top3 = model_topn_rets;
        for (size_t d_i = 0; d_i < rets_ex_top3.size(); ++d_i) {
            for (size_t a : drop_top3) rets_ex_top3[d_i] -= topn_asset_contribs[a][d_i];
        }
        rep.skill.sharpe_ex_top3_stocks = compute_sharpe_(rets_ex_top3);

        // Robustness Test 2: Zero out best 5 single days
        std::vector<double> rets_ex_best5 = model_topn_rets;
        std::vector<size_t> day_ord(rets_ex_best5.size());
        std::iota(day_ord.begin(), day_ord.end(), 0);
        std::sort(day_ord.begin(), day_ord.end(), [&](size_t a, size_t b) {
            return rets_ex_best5[a] > rets_ex_best5[b];
        });
        for (size_t k = 0; k < std::min<size_t>(5, day_ord.size()); ++k) {
            rets_ex_best5[day_ord[k]] = 0.0;
        }
        rep.skill.sharpe_ex_best5_days = compute_sharpe_(rets_ex_best5);

        if (rep.mean_cs_ic >= 0.02 && rep.skill.excess_sharpe_vs_vol > 0.15 && rep.skill.excess_sharpe_vs_shuffled > 0.15) {
            rep.skill.verdict = I18n::tr("GERÇEK SEÇİM ALFASI (Vol/Rastgele Üstü Skill)", "GENUINE SELECTION ALPHA (Beats Vol & Shuffled)", "ECHTES SELEKTIONS-ALPHA");
        } else if (std::abs(rep.skill.avg_pred_vol_corr) > 0.40 && rep.skill.excess_sharpe_vs_vol <= 0.10) {
            rep.skill.verdict = I18n::tr("SAHTE ALFA UYARISI: Volatilite Vekili / Vol Top-N ile Eşdeğer", "FAKE-ALPHA WARNING: Volatility Proxy / Matches Vol Top-N", "FAKE-ALPHA-WARNUNG: Volatilitäts-Proxy");
        } else if (rep.mean_cs_ic <= 0.0 && rep.skill.excess_sharpe_vs_shuffled <= 0.0) {
            rep.skill.verdict = I18n::tr("YETERSİZ YATAY KESİT YETENEĞİ (Rastgele ile Eşdeğer)", "NO CROSS-SECTIONAL SKILL (Matches Random)", "KEIN QUERSCHNITTS-SKILL");
        } else {
            rep.skill.verdict = I18n::tr("ILIMLI / REJİM BAĞIMLI ALFA (Seçicilik Doğrulandı)", "MODERATE / REGIME-DEPENDENT ALPHA", "MODERATES / REGIME-ABHÄNGIGES ALPHA");
        }

        // 7. Sequential Temporal Walk-Forward Folds (4-6 folds)
        const size_t n_folds = (panel.size() >= 180) ? 5 : 4;
        const size_t fold_len = std::max<size_t>(5, panel.size() / n_folds);
        for (size_t f = 0; f < n_folds; ++f) {
            const size_t f0 = f * fold_len;
            const size_t f1 = (f + 1 == n_folds) ? panel.size() : std::min(panel.size(), (f + 1) * fold_len);
            if (f0 >= f1) continue;

            FoldPerformanceRow fr;
            fr.fold = static_cast<int>(f + 1);
            fr.val_start = panel[f0].date_str;
            fr.val_end = panel[f1 - 1].date_str;
            double fic = 0.0, fq = 0.0;
            std::vector<int> fyt, fyp;
            std::vector<double> fpr, frt;
            for (size_t d_i = f0; d_i < f1; ++d_i) {
                fic += rep.daily_ic_series[d_i].ic;
                fq += rep.daily_ic_series[d_i].q_spread;
                for (const auto& o : panel[d_i].obs) {
                    fyt.push_back(to_cls(o.fwd_ret_h));
                    fyp.push_back(to_cls(o.pred_ret));
                    fpr.push_back(o.pred_ret);
                    frt.push_back(o.fwd_ret_h);
                }
            }
            const double fd_cnt = static_cast<double>(f1 - f0);
            fr.val_samples = frt.size();
            fr.ic = fic / fd_cnt;
            fr.q_spread = fq / fd_cnt;
            auto fcm = compute_classification_metrics(fyt, fyp, &frt, &fpr);
            fr.accuracy = fcm.accuracy;
            fr.directional_accuracy = fcm.dir_hit_rate_ex_neutral;
            fr.lift = fcm.lift;
            fr.mcc = fcm.mcc;
            fr.f1_macro = fcm.f1_macro;
            rep.per_fold.push_back(fr);
        }

        // 8. Pre-Deployment Diagnostic Suite
        rep.diagnostics = run_diagnostic_sanity_checks(&universe.front());
        rep.diagnostics.signal_quality = evaluate_signal_quality(global_pred, global_ret);

        rep.valid = true;
        return rep;
    }

    // Convenience runner fetching regional equities in parallel and executing full cross-sectional study
    static CrossSectionalReport run_regional_study(LiveMarketDataClient& client,
                                                   const std::string& region,
                                                   int horizon_days = 5,
                                                   int top_n = 4,
                                                   const std::string& range = "2y",
                                                   bool include_live_sentiment = true) {
        CrossSectionalConfig cfg;
        cfg.region = region;
        cfg.horizon_days = horizon_days;
        cfg.rebalance_days = std::max(1, horizon_days);
        cfg.top_n = top_n;
        cfg.use_live_sentiment = include_live_sentiment;

        auto tickers = QuantOpportunityScanner::get_universe_tickers(region, true);
        std::vector<RealTimeAssetData> assets(tickers.size());
        std::vector<double> sents(tickers.size(), 0.0);
        std::atomic<size_t> next_i{0};
        const size_t workers_n = std::min<size_t>(4, tickers.size());
        std::vector<std::future<void>> pool;

        for (size_t w = 0; w < workers_n; ++w) {
            pool.push_back(std::async(std::launch::async, [&]() {
                while (true) {
                    size_t i = next_i.fetch_add(1, std::memory_order_relaxed);
                    if (i >= tickers.size()) break;
                    assets[i] = client.fetch_asset(tickers[i], range);
                    if (include_live_sentiment && !assets[i].symbol.empty()) {
                        auto prof = NewsSentimentEngine::get_asset_sentiment(assets[i].symbol, assets[i].long_name, client);
                        sents[i] = prof.sentiment_score;
                    }
                }
            }));
        }
        for (auto& f : pool) f.get();

        std::unordered_map<std::string, double> sent_map;
        std::vector<RealTimeAssetData> valid_assets;
        bool any_live = false;
        for (const auto& a : assets) {
            if (a.is_live) any_live = true;
        }
        for (size_t i = 0; i < assets.size(); ++i) {
            if (assets[i].historical_prices.size() < 75) continue;
            if (any_live && !assets[i].is_live) continue;
            valid_assets.push_back(assets[i]);
            sent_map[assets[i].symbol] = sents[i];
        }

        if (cfg.experiment_name.empty()) {
            auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm_now{};
#ifdef _WIN32
            localtime_s(&tm_now, &now_t);
#else
            localtime_r(&now_t, &tm_now);
#endif
            cfg.experiment_name = std::format("bt_cpp_{}_h{}_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}",
                                              region, cfg.horizon_days,
                                              tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                                              tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        }

        auto rep = run_backtest(valid_assets, cfg, &sent_map);
        if (rep.valid) {
            export_report(rep, &valid_assets);
        }
        return rep;
    }

    // ============================================================================
    // Pillar 5 Data Contract: Export Standard CSV/JSON + Local OHLCV Cache to reports/
    // ============================================================================
    static std::string export_report(CrossSectionalReport& rep,
                                     const std::vector<RealTimeAssetData>* source_assets = nullptr) {
        namespace fs = std::filesystem;
        if (!rep.valid) return {};

        if (rep.config.experiment_name.empty()) {
            rep.config.experiment_name = std::format("bt_cpp_{}_h{}", rep.config.region, rep.config.horizon_days);
        }
        fs::path root = fs::path(rep.config.reports_root) / rep.config.experiment_name;
        std::error_code ec;
        fs::create_directories(root, ec);
        if (ec) return {};

        auto csv_escape = [](const std::string& s) -> std::string {
            if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
            std::string out = "\"";
            for (char c : s) {
                if (c == '"') out += "\"\"";
                else out.push_back(c);
            }
            out += "\"";
            return out;
        };

        // 1. config.json
        {
            std::ofstream f(root / "config.json");
            f << std::fixed << std::setprecision(4);
            f << "{\n"
              << "  \"experiment_name\": \"" << rep.config.experiment_name << "\",\n"
              << "  \"task\": \"regression\",\n"
              << "  \"region\": \"" << rep.config.region << "\",\n"
              << "  \"model_type\": \"QuantForecastEngine_Cpp20_EBayes\",\n"
              << "  \"use_sentiment\": " << (rep.config.use_live_sentiment ? "true" : "false") << ",\n"
              << "  \"sentiment_mode\": \"sticky_dual_stream\",\n"
              << "  \"horizon\": " << rep.config.horizon_days << ",\n"
              << "  \"rebalance_days\": " << rep.config.rebalance_days << ",\n"
              << "  \"top_n\": " << rep.config.top_n << ",\n"
              << "  \"top_pct\": " << rep.config.top_pct << ",\n"
              << "  \"tc_bps\": " << rep.config.tc_bps << ",\n"
              << "  \"execution\": \"" << (rep.config.use_next_open_execution ? "Open[t+1]" : "Close[t]") << "\",\n"
              << "  \"n_assets\": " << rep.universe_size << ",\n"
              << "  \"evaluated_days\": " << rep.evaluated_days << ",\n"
              << "  \"n_params\": 18,\n"
              << "  \"start_date\": \"" << rep.start_date << "\",\n"
              << "  \"end_date\": \"" << rep.end_date << "\"\n"
              << "}\n";
        }

        // 2. summary.csv (exact schema expected by dashboard/backend/data_loader.py)
        {
            std::ofstream f(root / "summary.csv");
            f << std::fixed << std::setprecision(6);
            f << "experiment_name,task,use_sentiment,wf_agg_ic,wf_agg_directional_accuracy,"
              << "wf_agg_mse,wf_agg_r2,wf_agg_accuracy,wf_agg_lift,wf_agg_mcc,wf_agg_f1_macro,"
              << "wf_std_accuracy,cross_sectional_ic,ic_ir,pct_positive_ic,quantile_spread,"
              << "cumulative_q_spread,evaluated_days,n_assets,model_topn_sharpe,vol_topn_sharpe,"
              << "rand_vol_topk_sharpe,shuffled_signal_sharpe,pred_vol_rank_corr,high_vol_pool_ic,"
              << "timing_alpha_pct,sharpe_ex_top3_stocks,sharpe_ex_best5_days,"
              << "lookahead_passed,synthetic_overfit_passed,sentiment_var_passed,signal_pvalue\n";
            f << csv_escape(rep.config.experiment_name) << ",regression,"
              << (rep.config.use_live_sentiment ? "True" : "False") << ","
              << rep.global_wf_ic << ","
              << rep.global_clf.dir_hit_rate_ex_neutral << ","
              << rep.global_mse << ","
              << rep.global_r2 << ","
              << rep.global_clf.accuracy << ","
              << rep.global_clf.lift << ","
              << rep.global_clf.mcc << ","
              << rep.global_clf.f1_macro << ","
              << rep.ic_std << ","
              << rep.mean_cs_ic << ","
              << rep.ic_ir << ","
              << rep.pct_positive_ic << ","
              << rep.mean_q_spread << ","
              << rep.cumulative_q_spread << ","
              << rep.evaluated_days << ","
              << rep.universe_size << ","
              << rep.skill.model_topn_sharpe << ","
              << rep.skill.vol_topn_sharpe << ","
              << rep.skill.rand_vol_topk_sharpe << ","
              << rep.skill.shuffled_signal_sharpe << ","
              << rep.skill.avg_pred_vol_corr << ","
              << rep.skill.high_vol_pool_ic << ","
              << rep.skill.timing_alpha_pct << ","
              << rep.skill.sharpe_ex_top3_stocks << ","
              << rep.skill.sharpe_ex_best5_days << ","
              << (rep.diagnostics.lookahead_audit.passed ? 1 : 0) << ","
              << (rep.diagnostics.synthetic_overfit_passed ? 1 : 0) << ","
              << (rep.diagnostics.temporal_sentiment_var_passed ? 1 : 0) << ","
              << rep.diagnostics.signal_quality.p_value << "\n";
        }

        // 3. per_fold_results.csv
        {
            std::ofstream f(root / "per_fold_results.csv");
            f << std::fixed << std::setprecision(6);
            f << "fold,val_start,val_end,val_samples,ic,accuracy,directional_accuracy,lift,mcc,f1_macro,q_spread\n";
            for (const auto& r : rep.per_fold) {
                f << r.fold << "," << r.val_start << "," << r.val_end << "," << r.val_samples << ","
                  << r.ic << "," << r.accuracy << "," << r.directional_accuracy << ","
                  << r.lift << "," << r.mcc << "," << r.f1_macro << "," << r.q_spread << "\n";
            }
        }

        // 4. per_stock_results.csv
        {
            std::ofstream f(root / "per_stock_results.csv");
            f << std::fixed << std::setprecision(6);
            f << "ticker,n_samples,ic,directional_accuracy,dir_hit_rate_ex_neutral,zero_rule,lift,mcc,f1_macro,mse,r2,topn_picks,topn_contrib_pct,ret_picked_ann,ret_not_picked_ann,timing_spread_pct\n";
            for (const auto& s : rep.per_stock) {
                f << csv_escape(s.ticker) << "," << s.n_samples << ","
                  << s.ic << "," << s.directional_accuracy << "," << s.dir_hit_rate_ex_neutral << ","
                  << s.zero_rule << "," << s.lift << "," << s.mcc << "," << s.f1_macro << ","
                  << s.mse << "," << s.r2 << "," << s.topn_picks << "," << s.topn_contrib_pct << ","
                  << s.ret_when_picked_ann << "," << s.ret_when_not_picked_ann << "," << s.timing_spread_pct << "\n";
            }
        }

        // 5. cross_sectional_ic.csv
        {
            std::ofstream f(root / "cross_sectional_ic.csv");
            f << std::fixed << std::setprecision(6);
            f << "Date,IC,IC_Rolling_20,N_Stocks,Q_Spread,Cumulative_Spread,Pred_Vol_Corr,High_Vol_Pool_IC\n";
            for (const auto& d : rep.daily_ic_series) {
                f << d.date_str << "," << d.ic << "," << d.ic_rolling_20 << "," << d.n_stocks << ","
                  << d.q_spread << "," << d.cumulative_spread << "," << d.pred_vol_corr << "," << d.high_vol_pool_ic << "\n";
            }
        }

        // 6. backtest_results.csv
        {
            std::ofstream f(root / "backtest_results.csv");
            f << std::fixed << std::setprecision(6);
            f << "Strategy,Total Return,Annualized Return,Sharpe Ratio,Sortino Ratio,Max Drawdown,Daily Win Rate,Total Turnover,Excess vs BnH\n";
            for (const auto& s : rep.strategies) {
                f << csv_escape(s.strategy_name) << "," << s.total_return << "," << s.annualized_return << ","
                  << s.sharpe_ratio << "," << s.sortino_ratio << "," << s.max_drawdown << ","
                  << s.daily_win_rate << "," << s.total_turnover << "," << s.excess_vs_bnh << "\n";
            }
        }

        // 7. Model Top-N Trades & Holdings (filename matches Strategy column 1:1 for TradeView)
        {
            std::ofstream f(root / std::format("QuantForecast Top-{}_trades.csv", rep.config.top_n));
            f << std::fixed << std::setprecision(6);
            f << "Date,Ticker,Action,Price,Predicted_Return,Actual_Return,PnL_Pct\n";
            for (const auto& tr : rep.topn_trades) {
                f << tr.date << "," << csv_escape(tr.ticker) << "," << tr.action << ","
                  << tr.price << "," << tr.predicted_return << "," << tr.actual_return << "," << tr.pnl_pct << "\n";
            }
        }
        {
            std::ofstream f(root / std::format("QuantForecast Top-{}_holdings.csv", rep.config.top_n));
            f << std::fixed << std::setprecision(6);
            f << "Date,Top_Picks,Portfolio_Value,Daily_Return,Benchmark_Value\n";
            for (const auto& h : rep.topn_holdings) {
                f << h.date << "," << csv_escape(h.top_picks) << "," << h.portfolio_value << ","
                  << h.daily_return << "," << h.benchmark_value << "\n";
            }
        }

        // 8. Export local OHLCV JSON cache so React TradeView renders instant candles (especially BIST / offline!)
        if (source_assets != nullptr && !source_assets->empty()) {
            fs::path ohlcv_dir = fs::path(rep.config.reports_root) / "ohlcv";
            fs::create_directories(ohlcv_dir, ec);
            for (const auto& a : *source_assets) {
                if (a.symbol.empty() || a.historical_prices.empty()) continue;
                std::ofstream jf(ohlcv_dir / (a.symbol + ".json"));
                jf << std::fixed << std::setprecision(4);
                jf << "[\n";
                const size_t n = a.historical_prices.size();
                for (size_t i = 0; i < n; ++i) {
                    int64_t ts = (i < a.timestamps.size()) ? a.timestamps[i] : 0;
                    int64_t dk = (i < a.day_keys.size()) ? a.day_keys[i] : static_cast<int64_t>(i);
                    std::string dt = format_epoch_date(ts, dk);
                    double cl = a.historical_prices[i];
                    double op = (i < a.historical_opens.size() && a.historical_opens[i] > 0.0) ? a.historical_opens[i] : cl;
                    double hi = (i < a.historical_highs.size() && a.historical_highs[i] > 0.0) ? a.historical_highs[i] : std::max(op, cl);
                    double lo = (i < a.historical_lows.size() && a.historical_lows[i] > 0.0) ? a.historical_lows[i] : std::min(op, cl);
                    long long vol = (i < a.historical_volumes.size()) ? static_cast<long long>(a.historical_volumes[i]) : 0LL;
                    jf << "  {\"time\":\"" << dt << "\",\"open\":" << op << ",\"high\":" << hi
                       << ",\"low\":" << lo << ",\"close\":" << cl << ",\"volume\":" << vol << "}";
                    if (i + 1 < n) jf << ",";
                    jf << "\n";
                }
                jf << "]\n";
            }
        }

        rep.exported_dir = root.generic_string();
        return rep.exported_dir;
    }

    // ============================================================================
    // CRT Terminal Report Renderer (TR / EN / DE i18n)
    // ============================================================================
    static void render_report(const CrossSectionalReport& rep) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();
        if (!rep.valid) {
            std::cout << pal.red_rgb << "\n  ❌ "
                      << I18n::tr("Yatay kesit portföy analizi için yeterli hizalı hisse senedi verisi bulunamadı!\n",
                                  "Insufficient aligned equity series for cross-sectional portfolio analysis!\n",
                                  "Nicht genügend ausgerichtete Aktiendaten für Querschnittsanalyse!\n")
                      << pal.reset_code;
            return;
        }

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::string hdr = I18n::tr(
            std::format("║  📊 {} YATAY KESİT PORTFÖY & SAHTE-ALFA / TAHMİN EDİLEBİLİRLİK RAPORU", rep.config.region),
            std::format("║  📊 {} CROSS-SECTIONAL PORTFOLIO, FAKE-ALPHA & PREDICTABILITY REPORT", rep.config.region),
            std::format("║  📊 {} QUERSCHNITTS-PORTFOLIO, FAKE-ALPHA & PROGNOSE-BERICHT", rep.config.region));
        int hpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));
        std::cout << hdr << std::string(static_cast<size_t>(hpad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n" << pal.reset_code;

        std::cout << std::format("   {:<26}: {} ({} hisse │ {} ETF/Index elendi)\n",
                                 I18n::tr("Evren & Tarih Aralığı", "Universe & Horizon Gap", "Universum & Zeitraum"),
                                 rep.start_date + " -> " + rep.end_date, rep.universe_size, rep.excluded_etf_count);
        std::cout << std::format("   {:<26}: {} gün │ {} gözlem │ İnfaz: {} │ Maliyet: {:.0f} bps\n",
                                 I18n::tr("Örneklem & İnfaz Modeli", "Sample & Execution", "Stichprobe & Ausführung"),
                                 rep.evaluated_days, rep.total_observations,
                                 rep.config.use_next_open_execution ? "Open[t+1]" : "Close[t]",
                                 rep.config.tc_bps);

        std::cout << "\n" << pal.glow_rgb << "   1. YATAY KESİT IC & SINIFLANDIRMA KALİTESİ (CS-IC / MCC / ZERO-RULE LIFT):\n" << pal.reset_code;
        std::cout << std::format("   • Mean Rank CS-IC : {}{:+.4f}{} │ IC-IR: {:+.3f} │ +IC Gün: %{:.1f} │ Mean Q-Spread: {:+.3f}%\n",
                                 (rep.mean_cs_ic >= 0.01 ? pal.green_rgb : pal.red_rgb), rep.mean_cs_ic, pal.reset_code,
                                 rep.ic_ir, rep.pct_positive_ic, rep.mean_q_spread * 100.0);
        std::cout << std::format("   • Yön İsabeti (Ex-Nötr) : %{:.1f} │ 3-Sınıf Acc: %{:.1f} (Zero-Rule: %{:.1f} │ Lift: {:+.1f}p)\n",
                                 rep.global_clf.dir_hit_rate_ex_neutral * 100.0,
                                 rep.global_clf.accuracy * 100.0,
                                 rep.global_clf.zero_rule * 100.0,
                                 rep.global_clf.lift * 100.0);
        std::cout << std::format("   • Gorodkin MCC    : {:+.4f} │ Macro F1: {:.4f} │ Cum Q-Spread: {:+.2f}%\n",
                                 rep.global_clf.mcc, rep.global_clf.f1_macro, rep.cumulative_q_spread * 100.0);

        std::cout << "\n" << pal.glow_rgb << "   2. PORTFÖY STRATEJİLERİ vs SAHTE-ALFA KONTROLLERİ (Top-N, L/S, Vol-Control, Random):\n" << pal.reset_code;
        std::cout << "   ┌──────────────────────────────────────┬──────────┬────────┬──────────┬─────────┐\n";
        std::cout << "   │ Strateji / Kontrol                   │ Top.Get. │ Sharpe │ Max DD   │ Turnov. │\n";
        std::cout << "   ├──────────────────────────────────────┼──────────┼────────┼──────────┼─────────┤\n";
        for (const auto& s : rep.strategies) {
            std::string col = s.is_model ? pal.glow_rgb : pal.dim_rgb;
            std::cout << std::format("   │ {}{:<36}{} │ {:+7.2f}% │ {:6.2f} │ {:+7.2f}% │ {:7.2f} │\n",
                                     col, s.strategy_name.substr(0, 36), pal.reset_code,
                                     s.total_return * 100.0, s.sharpe_ratio, s.max_drawdown * 100.0, s.total_turnover);
        }
        std::cout << "   └──────────────────────────────────────┴──────────┴────────┴──────────┴─────────┘\n";

        std::cout << "\n" << pal.glow_rgb << "   3. SAHTE-ALFA, ZAMANLAMA & DAYANIKLILIK TEŞHİSİ:\n" << pal.reset_code;
        std::cout << std::format("   • Sharpe Farkı vs Vol Top-N : {:+.2f} │ vs Shuffled-Signal: {:+.2f} │ vs Rand-Vol: {:+.2f}\n",
                                 rep.skill.excess_sharpe_vs_vol, rep.skill.excess_sharpe_vs_shuffled, rep.skill.excess_sharpe_vs_rand_vol);
        std::cout << std::format("   • Vol-Bias Corr(Pred,Vol)   : {:+.3f} │ |Pred| vs Vol: {:+.3f} │ High-Vol Pool IC: {:+.4f}\n",
                                 rep.skill.avg_pred_vol_corr, rep.skill.avg_abs_pred_vol_corr, rep.skill.high_vol_pool_ic);
        std::cout << std::format("   • Zamanlama Testi (Picked vs Not) : {:+.2f}% - {:+.2f}% = {}{:+.2f}%{} yıllık spread\n",
                                 rep.skill.mean_ret_picked_ann, rep.skill.mean_ret_not_picked_ann,
                                 (rep.skill.timing_alpha_pct >= 0 ? pal.green_rgb : pal.red_rgb), rep.skill.timing_alpha_pct, pal.reset_code);
        std::cout << std::format("   • Dayanıklılık (Top-3 Hisse Hariç Sharpe): {:.2f} │ (En İyi 5 Gün Hariç Sharpe): {:.2f}\n",
                                 rep.skill.sharpe_ex_top3_stocks, rep.skill.sharpe_ex_best5_days);
        std::cout << std::format("   • TEŞHİS KARARI : {}{}{}\n", pal.glow_rgb, rep.skill.verdict, pal.reset_code);

        std::cout << "\n" << pal.glow_rgb << "   4. SIZINTI & ÖN-DOĞRULAMA TESTLERİ (PRE-DEPLOYMENT SANITY):\n" << pal.reset_code;
        std::cout << std::format("   • Look-Ahead Nedensellik Mührü  : {} ({})\n",
                                 rep.diagnostics.lookahead_audit.passed ? (pal.green_rgb + "PASS ✔" + pal.reset_code) : (pal.red_rgb + "FAIL ✘" + pal.reset_code),
                                 rep.diagnostics.lookahead_audit.detail);
        std::cout << std::format("   • Synthetic Overfit Sanity      : {} (Yön: %{:.1f}) │ Temporal Sentiment Var: {} (σ={:.3f})\n",
                                 rep.diagnostics.synthetic_overfit_passed ? "PASS ✔" : "FAIL ✘",
                                 rep.diagnostics.synthetic_overfit_dir_acc * 100.0,
                                 rep.diagnostics.temporal_sentiment_var_passed ? "PASS ✔" : "FAIL ✘",
                                 rep.diagnostics.temporal_sentiment_std);
        std::cout << std::format("   • Signal Quality Pearson/p-val  : r={:+.4f} (Spearman={:+.4f}, t={:+.2f}, p={:.4f})\n",
                                 rep.diagnostics.signal_quality.pearson_r,
                                 rep.diagnostics.signal_quality.spearman_ic,
                                 rep.diagnostics.signal_quality.t_stat,
                                 rep.diagnostics.signal_quality.p_value);

        std::cout << "\n" << pal.glow_rgb << "   5. HİSSE BAZLI TAHMİN EDİLEBİLİRLİK (TOP/BOTTOM ÖZET):\n" << pal.reset_code;
        std::cout << "   ┌────────────┬──────────┬─────────┬──────────┬─────────┬────────┬──────────┐\n";
        std::cout << "   │ Sembol     │ Time-IC  │ DirHit% │ Lift     │ MCC     │ Picks  │ Katkı %  │\n";
        std::cout << "   ├────────────┼──────────┼─────────┼──────────┼─────────┼────────┼──────────┤\n";
        for (const auto& s : rep.per_stock) {
            std::cout << std::format("   │ {:<10} │ {:+8.4f} │ {:6.1f}% │ {:+7.1f}p │ {:+7.4f} │ {:6d} │ {:+7.1f}% │\n",
                                     s.ticker, s.ic, s.dir_hit_rate_ex_neutral * 100.0,
                                     s.lift * 100.0, s.mcc, s.topn_picks, s.topn_contrib_pct);
        }
        std::cout << "   └────────────┴──────────┴─────────┴──────────┴─────────┴────────┴──────────┘\n";

        if (!rep.exported_dir.empty()) {
            std::cout << "\n   " << pal.green_rgb << "📁 Rapor Dashboard için dışa aktarıldı: " << rep.exported_dir << pal.reset_code << "\n";
        }
        std::cout << pal.glow_rgb << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }

    // Helper for deterministic tests & fallbacks
    static RealTimeAssetData make_synthetic_asset_(const std::string& symbol,
                                                   size_t n_bars,
                                                   uint64_t seed,
                                                   double drift_daily,
                                                   double daily_vol) {
        RealTimeAssetData d;
        d.symbol = symbol;
        d.long_name = symbol + " Corp";
        d.currency_code = symbol.ends_with(".IS") ? "TRY" : "USD";
        d.currency_display = symbol.ends_with(".IS") ? "₺" : "$";
        d.instrument_type = "EQUITY";
        d.is_live = false;

        std::mt19937_64 rng(seed);
        std::normal_distribution<double> norm(0.0, daily_vol);
        double px = 100.0;
        const int64_t t0 = 1704067200LL; // 2024-01-01 UTC
        for (size_t i = 0; i < n_bars; ++i) {
            double r = drift_daily + norm(rng);
            double op = px;
            px = std::max(1.0, px * std::exp(r));
            double hi = std::max(op, px) * 1.008;
            double lo = std::min(op, px) * 0.992;
            d.timestamps.push_back(t0 + static_cast<int64_t>(i) * 86400LL);
            d.day_keys.push_back((t0 / 86400LL) + static_cast<int64_t>(i));
            d.historical_opens.push_back(op);
            d.historical_highs.push_back(hi);
            d.historical_lows.push_back(lo);
            d.historical_raw_closes.push_back(px);
            d.historical_prices.push_back(px);
            d.historical_volumes.push_back(1000000.0 + static_cast<double>((i * 7331) % 500000));
        }
        d.current_price = px;
        d.annualized_volatility = daily_vol * std::sqrt(252.0);
        return d;
    }

    static RealTimeAssetData make_oscillating_synthetic_asset_(const std::string& symbol,
                                                               size_t n_bars,
                                                               double amp = 0.04) {
        RealTimeAssetData d;
        d.symbol = symbol;
        d.instrument_type = "EQUITY";
        double px = 100.0;
        const int64_t t0 = 1704067200LL;
        for (size_t i = 0; i < n_bars; ++i) {
            double op = 100.0 * std::exp(amp * std::sin(static_cast<double>(i) * 0.45) + 0.0004 * static_cast<double>(i));
            px = op * (1.0 + 0.002 * std::cos(static_cast<double>(i) * 0.45));
            d.timestamps.push_back(t0 + static_cast<int64_t>(i) * 86400LL);
            d.day_keys.push_back((t0 / 86400LL) + static_cast<int64_t>(i));
            d.historical_opens.push_back(op);
            d.historical_highs.push_back(std::max(op, px) * 1.005);
            d.historical_lows.push_back(std::min(op, px) * 0.995);
            d.historical_raw_closes.push_back(px);
            d.historical_prices.push_back(px);
            d.historical_volumes.push_back(1500000.0);
        }
        d.current_price = px;
        d.annualized_volatility = 0.22;
        return d;
    }

private:
    static RealTimeAssetData slice_prefix_causal_(const RealTimeAssetData& d, size_t end_idx) {
        const size_t cnt = std::min(d.historical_prices.size(), end_idx + 1);
        RealTimeAssetData s;
        s.symbol = d.symbol;
        s.long_name = d.long_name;
        s.currency_code = d.currency_code;
        s.currency_display = d.currency_display;
        s.instrument_type = d.instrument_type;
        s.is_live = d.is_live;

        s.historical_prices.assign(d.historical_prices.begin(), d.historical_prices.begin() + static_cast<std::ptrdiff_t>(cnt));
        if (d.historical_highs.size() >= cnt)
            s.historical_highs.assign(d.historical_highs.begin(), d.historical_highs.begin() + static_cast<std::ptrdiff_t>(cnt));
        if (d.historical_lows.size() >= cnt)
            s.historical_lows.assign(d.historical_lows.begin(), d.historical_lows.begin() + static_cast<std::ptrdiff_t>(cnt));
        if (d.historical_volumes.size() >= cnt)
            s.historical_volumes.assign(d.historical_volumes.begin(), d.historical_volumes.begin() + static_cast<std::ptrdiff_t>(cnt));
        s.current_price = s.historical_prices.back();
        double v20 = compute_trailing_vol_(s.historical_prices, cnt - 1, 20);
        s.annualized_volatility = (v20 > 0.0) ? v20 : 0.22;
        return s;
    }

    static RealTimeAssetData slice_window_causal_(const RealTimeAssetData& d, size_t end_idx, size_t max_len) {
        const size_t start = (end_idx + 1 > max_len) ? (end_idx + 1 - max_len) : 0;
        const size_t stop = std::min(d.historical_prices.size(), end_idx + 1);
        RealTimeAssetData s;
        s.symbol = d.symbol;
        s.long_name = d.long_name;
        s.currency_code = d.currency_code;
        s.currency_display = d.currency_display;
        s.instrument_type = d.instrument_type;
        s.is_live = d.is_live;

        s.historical_prices.assign(d.historical_prices.begin() + static_cast<std::ptrdiff_t>(start),
                                   d.historical_prices.begin() + static_cast<std::ptrdiff_t>(stop));
        if (d.historical_highs.size() >= stop)
            s.historical_highs.assign(d.historical_highs.begin() + static_cast<std::ptrdiff_t>(start),
                                      d.historical_highs.begin() + static_cast<std::ptrdiff_t>(stop));
        if (d.historical_lows.size() >= stop)
            s.historical_lows.assign(d.historical_lows.begin() + static_cast<std::ptrdiff_t>(start),
                                     d.historical_lows.begin() + static_cast<std::ptrdiff_t>(stop));
        if (d.historical_volumes.size() >= stop)
            s.historical_volumes.assign(d.historical_volumes.begin() + static_cast<std::ptrdiff_t>(start),
                                        d.historical_volumes.begin() + static_cast<std::ptrdiff_t>(stop));
        s.current_price = s.historical_prices.back();
        return s;
    }

    static double compute_trailing_vol_(const std::vector<double>& p, size_t idx, size_t win = 20) {
        if (idx < 3 || idx >= p.size()) return 0.22;
        const size_t start = (idx >= win) ? (idx - win + 1) : 1;
        std::vector<double> lrs;
        lrs.reserve(idx - start + 1);
        for (size_t i = start; i <= idx; ++i) {
            if (p[i - 1] > 0.0 && p[i] > 0.0) lrs.push_back(std::log(p[i] / p[i - 1]));
        }
        if (lrs.size() < 3) return 0.22;
        double m = std::accumulate(lrs.begin(), lrs.end(), 0.0) / static_cast<double>(lrs.size());
        double v = 0.0;
        for (double x : lrs) v += (x - m) * (x - m);
        double sd = std::sqrt(v / static_cast<double>(lrs.size() - 1));
        return std::clamp(sd * std::sqrt(252.0), 0.05, 1.50);
    }

    static double compute_sharpe_(const std::vector<double>& rets) {
        if (rets.size() < 3) return 0.0;
        double m = std::accumulate(rets.begin(), rets.end(), 0.0) / static_cast<double>(rets.size());
        double v = 0.0;
        for (double r : rets) v += (r - m) * (r - m);
        double sd = std::sqrt(v / static_cast<double>(rets.size() - 1));
        return (sd > 1e-12) ? ((m / sd) * std::sqrt(252.0)) : 0.0;
    }
};

} // namespace quant
