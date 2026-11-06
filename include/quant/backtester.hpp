#pragma once

#include <vector>
#include <string>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <format>
#include <iostream>

namespace quant {

enum class StrategyType {
    SmaCrossover,
    BollingerMeanReversion,
    RsiMomentum
};

struct BacktestResult {
    std::string strategy_name;
    double total_return_pct{0.0};
    double benchmark_return_pct{0.0};
    double annualized_sharpe{0.0};
    double max_drawdown_pct{0.0};
    int total_trades{0};
    double win_rate_pct{0.0};
};

class BacktestEngine {
public:
    static BacktestResult run(const std::vector<double>& prices,
                              StrategyType strat = StrategyType::SmaCrossover,
                              int fast_period = 5,
                              int slow_period = 20,
                              double one_way_cost_bps = 10.0) {
        BacktestResult res;
        if (prices.size() < static_cast<size_t>(slow_period) + 2 || prices.front() <= 0.0) {
            return res;
        }

        const size_t n = prices.size();
        const double cost_rate = std::max(0.0, one_way_cost_bps) * 1e-4;

        // Benchmark Buy & Hold return
        res.benchmark_return_pct = ((prices.back() - prices.front()) / prices.front()) * 100.0;

        // Causal end-of-bar signal -> executed over next bar [i, i+1] (zero look-ahead bias)
        std::vector<int> positions(n, 0);

        if (strat == StrategyType::SmaCrossover) {
            res.strategy_name = std::format("SMA Crossover (Fast={}, Slow={}, 10bps Cost)", fast_period, slow_period);
            for (size_t i = static_cast<size_t>(slow_period); i < n; ++i) {
                double fast_sma = std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(i - fast_period + 1),
                                                  prices.begin() + static_cast<std::ptrdiff_t>(i + 1), 0.0) / fast_period;
                double slow_sma = std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(i - slow_period + 1),
                                                  prices.begin() + static_cast<std::ptrdiff_t>(i + 1), 0.0) / slow_period;
                positions[i] = (fast_sma > slow_sma) ? 1 : 0;
            }
        } else if (strat == StrategyType::BollingerMeanReversion) {
            res.strategy_name = "Bollinger Mean Reversion (Win=20, Sample Std=2.0, 10bps Cost)";
            constexpr size_t win = 20;
            for (size_t i = win; i < n; ++i) {
                double mean = std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(i - win + 1),
                                              prices.begin() + static_cast<std::ptrdiff_t>(i + 1), 0.0) / static_cast<double>(win);
                double sq_sum = 0.0;
                for (size_t j = i - win + 1; j <= i; ++j) {
                    double d = prices[j] - mean;
                    sq_sum += d * d;
                }
                double std_dev = std::sqrt(sq_sum / static_cast<double>(win - 1));
                double lower = mean - 2.0 * std_dev;
                double upper = mean + 2.0 * std_dev;

                if (prices[i] < lower) positions[i] = 1;      // Oversold -> Buy
                else if (prices[i] > upper) positions[i] = 0; // Overbought -> Exit
                else positions[i] = positions[i - 1];
            }
        } else {
            res.strategy_name = "Wilder RSI(14) Mean-Reversion (Buy<30, Exit>70, 10bps Cost)";
            constexpr size_t period = 14;
            if (n > period + 1) {
                double avg_gain = 0.0, avg_loss = 0.0;
                for (size_t i = 1; i <= period; ++i) {
                    double diff = prices[i] - prices[i - 1];
                    if (diff > 0.0) avg_gain += diff;
                    else avg_loss -= diff;
                }
                avg_gain /= static_cast<double>(period);
                avg_loss /= static_cast<double>(period);

                for (size_t i = period + 1; i < n; ++i) {
                    double diff = prices[i] - prices[i - 1];
                    double g = (diff > 0.0) ? diff : 0.0;
                    double l = (diff < 0.0) ? -diff : 0.0;
                    avg_gain = (avg_gain * (period - 1) + g) / static_cast<double>(period);
                    avg_loss = (avg_loss * (period - 1) + l) / static_cast<double>(period);
                    double rsi = (avg_loss < 1e-12) ? 100.0 : (100.0 - (100.0 / (1.0 + avg_gain / avg_loss)));

                    if (rsi < 30.0) positions[i] = 1;
                    else if (rsi > 70.0) positions[i] = 0;
                    else positions[i] = positions[i - 1];
                }
            }
        }

        double capital = 100000.0;
        double peak_capital = capital;
        double max_dd = 0.0;
        int win_trades = 0, total_trades = 0;
        double entry_capital = capital;
        int prev_pos = 0;
        std::vector<double> daily_returns;
        daily_returns.reserve(n);

        for (size_t i = 1; i < n; ++i) {
            int active_pos = positions[i - 1];
            int turnover = std::abs(active_pos - prev_pos);
            double asset_return = (prices[i - 1] > 0.0) ? ((prices[i] - prices[i - 1]) / prices[i - 1]) : 0.0;
            double net_ret = active_pos * asset_return - turnover * cost_rate;
            daily_returns.push_back(net_ret);

            if (prev_pos == 1 && active_pos == 0) {
                ++total_trades;
                if (capital * (1.0 - cost_rate) > entry_capital) ++win_trades;
            }
            if (prev_pos == 0 && active_pos == 1) {
                entry_capital = capital;
            }

            capital *= (1.0 + net_ret);
            peak_capital = std::max(peak_capital, capital);
            double dd = (peak_capital - capital) / peak_capital;
            max_dd = std::max(max_dd, dd);
            prev_pos = active_pos;
        }

        // Mark-to-market open position at end of backtest sample
        if (prev_pos == 1) {
            ++total_trades;
            if (capital > entry_capital) ++win_trades;
        }

        res.total_return_pct = ((capital - 100000.0) / 100000.0) * 100.0;
        res.max_drawdown_pct = max_dd * 100.0;
        res.total_trades = total_trades;
        res.win_rate_pct = (total_trades > 0) ? (static_cast<double>(win_trades) / total_trades * 100.0) : 0.0;

        if (daily_returns.size() >= 2) {
            double mean_r = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) /
                            static_cast<double>(daily_returns.size());
            double sq_diff = 0.0;
            for (double r : daily_returns) sq_diff += (r - mean_r) * (r - mean_r);
            double std_r = std::sqrt(sq_diff / static_cast<double>(daily_returns.size() - 1));
            res.annualized_sharpe = (std_r > 1e-8) ? ((mean_r / std_r) * std::sqrt(252.0)) : 0.0;
        }

        return res;
    }
};

} // namespace quant
