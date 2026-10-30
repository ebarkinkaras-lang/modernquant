#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/rolling_variance.hpp>
#include <boost/math/distributions/normal.hpp>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace quant {

namespace bacc = boost::accumulators;

// Chapter 9: Real-time technical indicator bar containing Bollinger Bands & Dual MA
struct IndicatorPoint {
    double price{0.0};
    double rolling_mean{0.0};
    double upper_band{0.0};
    double lower_band{0.0};
    double fast_ma{0.0};
    double slow_ma{0.0};
    int signal{0}; // 1 = Buy (Golden Cross), -1 = Sell (Death Cross), 0 = Hold
};

// Chapter 9: Online Streaming Technical Indicators using Boost Accumulators
class StreamingIndicators {
public:
    StreamingIndicators(size_t bollinger_win = 20, double num_std_dev = 2.0,
                        size_t fast_ma_win = 5, size_t slow_ma_win = 20)
        : bollinger_win_{bollinger_win}, num_std_dev_{num_std_dev},
          fast_ma_win_{fast_ma_win}, slow_ma_win_{slow_ma_win},
          boll_acc_{bacc::tag::rolling_window::window_size = bollinger_win},
          fast_acc_{bacc::tag::rolling_window::window_size = fast_ma_win},
          slow_acc_{bacc::tag::rolling_window::window_size = slow_ma_win} {}

    IndicatorPoint update(double price) {
        ++count_;
        boll_acc_(price);
        fast_acc_(price);
        slow_acc_(price);

        IndicatorPoint pt;
        pt.price = price;

        if (count_ >= bollinger_win_) {
            pt.rolling_mean = bacc::extract::rolling_mean(boll_acc_);
            double var = bacc::extract::rolling_variance(boll_acc_);
            double dev = num_std_dev_ * std::sqrt(std::max(0.0, var));
            pt.upper_band = pt.rolling_mean + dev;
            pt.lower_band = pt.rolling_mean - dev;
        }

        if (count_ >= fast_ma_win_) {
            pt.fast_ma = bacc::extract::rolling_mean(fast_acc_);
        }
        if (count_ >= slow_ma_win_) {
            pt.slow_ma = bacc::extract::rolling_mean(slow_acc_);
        }

        // Crossover logic
        if (count_ >= slow_ma_win_) {
            if (prev_fast_ <= prev_slow_ && pt.fast_ma > pt.slow_ma) {
                pt.signal = 1; // Golden Cross (Buy)
            } else if (prev_fast_ >= prev_slow_ && pt.fast_ma < pt.slow_ma) {
                pt.signal = -1; // Death Cross (Sell)
            }
            prev_fast_ = pt.fast_ma;
            prev_slow_ = pt.slow_ma;
        }

        return pt;
    }

private:
    size_t bollinger_win_;
    double num_std_dev_;
    size_t fast_ma_win_;
    size_t slow_ma_win_;
    size_t count_{0};

    double prev_fast_{0.0};
    double prev_slow_{0.0};

    using BollAcc = bacc::accumulator_set<
        double,
        bacc::stats<bacc::tag::rolling_mean, bacc::tag::rolling_variance>>;
    using MeanAcc = bacc::accumulator_set<
        double,
        bacc::stats<bacc::tag::rolling_mean>>;

    BollAcc boll_acc_;
    MeanAcc fast_acc_;
    MeanAcc slow_acc_;
};

// Chapter 6: Monte Carlo Drawdown & Value-at-Risk Permutation Testing
struct DrawdownAnalysisResult {
    double original_max_drawdown{0.0};
    double net_profit{0.0};
    double profit_to_max_dd_ratio{0.0};
    double mean_simulated_dd{0.0};
    double sd_simulated_dd{0.0};
    double parametric_95_dd{0.0};
    double parametric_99_dd{0.0};
    double nonparametric_95_dd{0.0};
    double nonparametric_99_dd{0.0};
};

class DrawdownRiskEngine {
public:
    static DrawdownAnalysisResult analyze(const std::vector<double>& pnl_series,
                                          unsigned num_simulations = 1000,
                                          unsigned seed = 42) {
        DrawdownAnalysisResult result;
        if (pnl_series.empty()) return result;

        // Helper lambda for single max drawdown calculation (Chapter 6, Page 212)
        auto calc_max_dd = [](const std::vector<double>& pnl) {
            std::vector<double> cum_pnl;
            cum_pnl.reserve(pnl.size());
            std::partial_sum(pnl.begin(), pnl.end(), std::back_inserter(cum_pnl));

            double peak = cum_pnl.front();
            double max_dd = 0.0;

            for (auto it = cum_pnl.cbegin() + 1; it != cum_pnl.cend(); ++it) {
                if (*it < peak) {
                    max_dd = std::max(peak - *it, max_dd);
                } else if (*it > peak) {
                    peak = *it;
                }
            }
            return max_dd;
        };

        // Original backtest metrics
        result.original_max_drawdown = calc_max_dd(pnl_series);
        result.net_profit = std::accumulate(pnl_series.begin(), pnl_series.end(), 0.0);
        result.profit_to_max_dd_ratio =
            (result.original_max_drawdown > 1e-6) ? (result.net_profit / result.original_max_drawdown) : 0.0;

        // Monte Carlo Shuffling without replacement
        std::vector<double> max_drawdowns;
        max_drawdowns.reserve(num_simulations);
        max_drawdowns.push_back(result.original_max_drawdown);

        std::mt19937_64 mt{seed};
        std::vector<double> shuffled_pnl = pnl_series;

        for (unsigned k = 0; k < num_simulations - 1; ++k) {
            std::ranges::shuffle(shuffled_pnl, mt);
            max_drawdowns.push_back(calc_max_dd(shuffled_pnl));
        }

        // Statistical evaluation (Mean & Sample Std Dev)
        double mean = std::accumulate(max_drawdowns.begin(), max_drawdowns.end(), 0.0) / max_drawdowns.size();
        double sum_sq = 0.0;
        for (double val : max_drawdowns) {
            sum_sq += (val - mean) * (val - mean);
        }
        double sd = std::sqrt(sum_sq / (max_drawdowns.size() - 1));

        result.mean_simulated_dd = mean;
        result.sd_simulated_dd = sd;

        // Boost Normal Distribution for Parametric Quantiles
        boost::math::normal std_norm(0.0, 1.0);
        double z_95 = boost::math::quantile(std_norm, 0.95);
        double z_99 = boost::math::quantile(std_norm, 0.99);

        result.parametric_95_dd = mean + sd * z_95;
        result.parametric_99_dd = mean + sd * z_99;

        // Non-parametric percentiles using sorted array
        std::ranges::sort(max_drawdowns);
        long loc_95 = std::lround(0.05 * max_drawdowns.size());
        long loc_99 = std::lround(0.01 * max_drawdowns.size());

        size_t idx_95 = max_drawdowns.size() - 1 - std::min<size_t>(loc_95, max_drawdowns.size() - 1);
        size_t idx_99 = max_drawdowns.size() - 1 - std::min<size_t>(loc_99, max_drawdowns.size() - 1);

        result.nonparametric_95_dd = max_drawdowns[idx_95];
        result.nonparametric_99_dd = max_drawdowns[idx_99];

        return result;
    }
};

} // namespace quant
