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
                              int slow_period = 20) {
        BacktestResult res;
        if (prices.size() < static_cast<size_t>(slow_period) + 2) {
            return res;
        }

        // Benchmark return
        res.benchmark_return_pct = ((prices.back() - prices.front()) / prices.front()) * 100.0;

        // Generate signals: 1 (Long), 0 (Flat/Cash), -1 (Short)
        std::vector<int> positions(prices.size(), 0);

        if (strat == StrategyType::SmaCrossover) {
            res.strategy_name = std::format("SMA Crossover (Fast={}, Slow={})", fast_period, slow_period);
            for (size_t i = slow_period; i < prices.size(); ++i) {
                double fast_sma = std::accumulate(prices.begin() + i - fast_period + 1, prices.begin() + i + 1, 0.0) / fast_period;
                double slow_sma = std::accumulate(prices.begin() + i - slow_period + 1, prices.begin() + i + 1, 0.0) / slow_period;
                positions[i] = (fast_sma > slow_sma) ? 1 : 0;
            }
        } else if (strat == StrategyType::BollingerMeanReversion) {
            res.strategy_name = "Bollinger Bands Mean Reversion (Win=20, Std=2.0)";
            int win = 20;
            for (size_t i = win; i < prices.size(); ++i) {
                double mean = std::accumulate(prices.begin() + i - win + 1, prices.begin() + i + 1, 0.0) / win;
                double sq_sum = 0.0;
                for (size_t j = i - win + 1; j <= i; ++j) {
                    sq_sum += (prices[j] - mean) * (prices[j] - mean);
                }
                double std_dev = std::sqrt(sq_sum / win);
                double lower = mean - 2.0 * std_dev;
                double upper = mean + 2.0 * std_dev;

                if (prices[i] < lower) positions[i] = 1;      // Oversold -> Buy
                else if (prices[i] > upper) positions[i] = 0; // Overbought -> Exit
                else positions[i] = positions[i - 1];
            }
        } else {
            res.strategy_name = "RSI Momentum (14-period, Buy<30, Sell>70)";
            positions[0] = 0;
            for (size_t i = 15; i < prices.size(); ++i) {
                // Approximate 14-period RSI
                double gains = 0.0, losses = 0.0;
                for (size_t j = i - 13; j <= i; ++j) {
                    double diff = prices[j] - prices[j - 1];
                    if (diff > 0) gains += diff;
                    else losses += std::abs(diff);
                }
                double rs = (losses > 1e-9) ? (gains / losses) : 100.0;
                double rsi = 100.0 - (100.0 / (1.0 + rs));

                if (rsi < 30.0) positions[i] = 1;
                else if (rsi > 70.0) positions[i] = 0;
                else positions[i] = positions[i - 1];
            }
        }

        // Calculate strategy returns, equity curve and trades
        double capital = 100000.0;
        double peak_capital = capital;
        double max_dd = 0.0;
        int win_trades = 0, total_trades = 0;
        double entry_price = 0.0;
        std::vector<double> daily_returns;

        for (size_t i = 1; i < prices.size(); ++i) {
            double asset_return = (prices[i] - prices[i - 1]) / prices[i - 1];
            double strat_return = positions[i - 1] * asset_return;
            daily_returns.push_back(strat_return);

            capital *= (1.0 + strat_return);
            peak_capital = std::max(peak_capital, capital);
            double dd = (peak_capital - capital) / peak_capital;
            max_dd = std::max(max_dd, dd);

            // Trade tracking
            if (positions[i] != positions[i - 1]) {
                if (positions[i - 1] == 1) {
                    // Closed trade
                    total_trades++;
                    if (prices[i] > entry_price) win_trades++;
                }
                if (positions[i] == 1) {
                    entry_price = prices[i];
                }
            }
        }

        res.total_return_pct = ((capital - 100000.0) / 100000.0) * 100.0;
        res.max_drawdown_pct = max_dd * 100.0;
        res.total_trades = total_trades;
        res.win_rate_pct = (total_trades > 0) ? (static_cast<double>(win_trades) / total_trades * 100.0) : 0.0;

        // Annualized Sharpe Ratio (rf = 0)
        if (!daily_returns.empty()) {
            double mean_r = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) / daily_returns.size();
            double sq_diff = 0.0;
            for (double r : daily_returns) sq_diff += (r - mean_r) * (r - mean_r);
            double std_r = std::sqrt(sq_diff / daily_returns.size());
            res.annualized_sharpe = (std_r > 1e-8) ? ((mean_r / std_r) * std::sqrt(252.0)) : 0.0;
        }

        return res;
    }
};

} // namespace quant
