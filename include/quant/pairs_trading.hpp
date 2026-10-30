#pragma once

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
    static PairsAnalysisResult analyze(const std::string& asset_a,
                                       const std::string& asset_b,
                                       const std::vector<double>& prices_a,
                                       const std::vector<double>& prices_b) {
        PairsAnalysisResult res;
        res.pair_name = asset_a + " / " + asset_b;

        size_t n = std::min(prices_a.size(), prices_b.size());
        if (n < 5) return res;

        // 1. Calculate Hedge Ratio (beta) via Least Squares: A = beta * B
        double mean_a = std::accumulate(prices_a.begin(), prices_a.begin() + n, 0.0) / n;
        double mean_b = std::accumulate(prices_b.begin(), prices_b.begin() + n, 0.0) / n;

        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < n; ++i) {
            num += (prices_a[i] - mean_a) * (prices_b[i] - mean_b);
            den += (prices_b[i] - mean_b) * (prices_b[i] - mean_b);
        }
        res.hedge_ratio = (den > 1e-12) ? (num / den) : 1.0;

        // 2. Spread series: Spread = A - beta * B
        std::vector<double> spread(n);
        for (size_t i = 0; i < n; ++i) {
            spread[i] = prices_a[i] - res.hedge_ratio * prices_b[i];
        }

        res.current_spread = spread.back();
        res.spread_mean = std::accumulate(spread.begin(), spread.end(), 0.0) / n;

        double sum_sq = 0.0;
        for (double s : spread) {
            sum_sq += (s - res.spread_mean) * (s - res.spread_mean);
        }
        res.spread_std = std::sqrt(sum_sq / (n - 1));

        res.current_zscore = (res.spread_std > 1e-12) ? ((res.current_spread - res.spread_mean) / res.spread_std) : 0.0;

        // 3. Half-life estimation via AR(1) regression: delta_s = lambda * s_{t-1} + error
        double num_lambda = 0.0, den_lambda = 0.0;
        for (size_t i = 1; i < n; ++i) {
            double delta_s = spread[i] - spread[i - 1];
            double prev_s = spread[i - 1] - res.spread_mean;
            num_lambda += delta_s * prev_s;
            den_lambda += prev_s * prev_s;
        }
        double lambda = (den_lambda > 1e-12) ? (num_lambda / den_lambda) : -0.1;
        if (lambda < 0.0) {
            res.half_life_days = -std::log(2.0) / lambda;
        } else {
            res.half_life_days = 999.0; // Non-mean reverting
        }

        // Trading signal
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
