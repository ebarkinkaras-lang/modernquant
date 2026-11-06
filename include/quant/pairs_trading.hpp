#pragma once

#include "live_market_data.hpp"
#include <vector>
#include <string>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <format>
#include <iostream>

namespace quant {

struct PairsAnalysisResult {
    std::string pair_name;
    int aligned_days{0};
    double correlation{0.0};
    double hedge_ratio{1.0};
    double current_spread{0.0};
    double spread_mean{0.0};
    double spread_std{0.0};
    double current_zscore{0.0};
    double half_life_days{0.0};
    std::string signal; // "LONG SPREAD (BUY A / SELL B)", "SHORT SPREAD", "NEUTRAL"
};

class PairsTradingEngine {
public:
    // Date-aligned Pairs Analysis eliminating cross-exchange holiday & 24/7 vs 5-day index drift
    static PairsAnalysisResult analyze_aligned(const RealTimeAssetData& a_data,
                                               const RealTimeAssetData& b_data) {
        auto aligned = LiveMarketDataClient::align_series_by_date(a_data, b_data, false);
        if (aligned.prices_a.size() >= 10) {
            auto res = analyze(a_data.symbol, b_data.symbol, aligned.prices_a, aligned.prices_b);
            res.aligned_days = static_cast<int>(aligned.prices_a.size());
            return res;
        }
        return analyze(a_data.symbol, b_data.symbol, a_data.historical_prices, b_data.historical_prices);
    }

    static PairsAnalysisResult analyze(const std::string& asset_a,
                                       const std::string& asset_b,
                                       const std::vector<double>& prices_a,
                                       const std::vector<double>& prices_b) {
        PairsAnalysisResult res;
        res.pair_name = asset_a + " / " + asset_b;

        size_t n = std::min(prices_a.size(), prices_b.size());
        res.aligned_days = static_cast<int>(n);
        if (n < 5) {
            res.signal = "INSUFFICIENT ALIGNED DATA";
            return res;
        }

        // 1. Calculate OLS Hedge Ratio (beta) & Correlation: A = alpha + beta * B
        double mean_a = std::accumulate(prices_a.begin(), prices_a.begin() + static_cast<std::ptrdiff_t>(n), 0.0) / static_cast<double>(n);
        double mean_b = std::accumulate(prices_b.begin(), prices_b.begin() + static_cast<std::ptrdiff_t>(n), 0.0) / static_cast<double>(n);

        double num = 0.0, den_b = 0.0, den_a = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double da = prices_a[i] - mean_a;
            double db = prices_b[i] - mean_b;
            num += da * db;
            den_b += db * db;
            den_a += da * da;
        }
        res.hedge_ratio = (den_b > 1e-12) ? (num / den_b) : 1.0;
        if (den_a > 1e-12 && den_b > 1e-12) {
            res.correlation = std::clamp(num / std::sqrt(den_a * den_b), -1.0, 1.0);
        }

        // 2. Spread series: Spread_t = A_t - beta * B_t
        std::vector<double> spread(n);
        for (size_t i = 0; i < n; ++i) {
            spread[i] = prices_a[i] - res.hedge_ratio * prices_b[i];
        }

        res.current_spread = spread.back();
        res.spread_mean = std::accumulate(spread.begin(), spread.end(), 0.0) / static_cast<double>(n);

        double sum_sq = 0.0;
        for (double s : spread) {
            sum_sq += (s - res.spread_mean) * (s - res.spread_mean);
        }
        res.spread_std = std::sqrt(sum_sq / static_cast<double>(n - 1));
        res.current_zscore = (res.spread_std > 1e-12)
            ? ((res.current_spread - res.spread_mean) / res.spread_std)
            : 0.0;

        // 3. Ornstein-Uhlenbeck Half-life estimation via AR(1) regression: delta_s_t = lambda * (s_{t-1} - mu) + eps
        double num_lambda = 0.0, den_lambda = 0.0;
        for (size_t i = 1; i < n; ++i) {
            double delta_s = spread[i] - spread[i - 1];
            double prev_s = spread[i - 1] - res.spread_mean;
            num_lambda += delta_s * prev_s;
            den_lambda += prev_s * prev_s;
        }
        double lambda = (den_lambda > 1e-12) ? (num_lambda / den_lambda) : -0.1;
        // Exact discrete-to-continuous OU mapping: phi = 1 + lambda; half_life = -ln(2) / ln(phi) when 0 < phi < 1
        double phi = 1.0 + lambda;
        if (phi > 0.01 && phi < 0.9999) {
            res.half_life_days = -std::log(2.0) / std::log(phi);
        } else if (lambda < 0.0) {
            res.half_life_days = -std::log(2.0) / lambda;
        } else {
            res.half_life_days = 999.0; // Non-mean reverting
        }

        if (res.current_zscore <= -2.0) {
            res.signal = "LONG SPREAD [BUY " + asset_a + " / SELL " + asset_b + "]";
        } else if (res.current_zscore >= 2.0) {
            res.signal = "SHORT SPREAD [SELL " + asset_a + " / BUY " + asset_b + "]";
        } else if (std::abs(res.current_zscore) <= 0.5) {
            res.signal = "EXIT / CLOSE POSITION [SPREAD AT MEAN]";
        } else {
            res.signal = "HOLD / NEUTRAL";
        }

        return res;
    }
};

} // namespace quant
