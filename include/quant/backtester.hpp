#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace quant {

enum class StrategyType {
  SmaCrossover,
  BollingerMeanReversion,
  RsiMomentum,
  RLDynamicPortfolio,
  OrderFlowImbalance
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
  static BacktestResult run(const std::vector<double> &prices,
                            StrategyType strat = StrategyType::SmaCrossover,
                            int fast_period = 5, int slow_period = 20,
                            double one_way_cost_bps = 10.0) {
    return run_ohlcv(prices, {}, {}, {}, strat, fast_period, slow_period,
                     one_way_cost_bps);
  }

  static BacktestResult
  run_ohlcv(const std::vector<double> &prices, const std::vector<double> &highs,
            const std::vector<double> &lows, const std::vector<double> &volumes,
            StrategyType strat = StrategyType::SmaCrossover,
            int fast_period = 5, int slow_period = 20,
            double one_way_cost_bps = 10.0) {
    BacktestResult res;
    if (prices.size() < static_cast<size_t>(slow_period) + 2 ||
        prices.front() <= 0.0) {
      return res;
    }

    const size_t n = prices.size();
    const double cost_rate = std::max(0.0, one_way_cost_bps) * 1e-4;

    // Benchmark Buy & Hold return
    res.benchmark_return_pct =
        ((prices.back() - prices.front()) / prices.front()) * 100.0;

    // Causal end-of-bar signal -> executed over next bar [i, i+1] (zero
    // look-ahead bias)
    std::vector<int> positions(n, 0);

    if (strat == StrategyType::SmaCrossover) {
      res.strategy_name =
          std::format("SMA Crossover (Fast={}, Slow={}, 10bps Cost)",
                      fast_period, slow_period);
      for (size_t i = static_cast<size_t>(slow_period); i < n; ++i) {
        double fast_sma =
            std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(
                                                 i - fast_period + 1),
                            prices.begin() + static_cast<std::ptrdiff_t>(i + 1),
                            0.0) /
            fast_period;
        double slow_sma =
            std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(
                                                 i - slow_period + 1),
                            prices.begin() + static_cast<std::ptrdiff_t>(i + 1),
                            0.0) /
            slow_period;
        positions[i] = (fast_sma > slow_sma) ? 1 : 0;
      }
    } else if (strat == StrategyType::BollingerMeanReversion) {
      res.strategy_name =
          "Bollinger Mean Reversion (Win=20, Sample Std=2.0, 10bps Cost)";
      constexpr size_t win = 20;
      for (size_t i = win; i < n; ++i) {
        double mean =
            std::accumulate(
                prices.begin() + static_cast<std::ptrdiff_t>(i - win + 1),
                prices.begin() + static_cast<std::ptrdiff_t>(i + 1), 0.0) /
            static_cast<double>(win);
        double sq_sum = 0.0;
        for (size_t j = i - win + 1; j <= i; ++j) {
          double d = prices[j] - mean;
          sq_sum += d * d;
        }
        double std_dev = std::sqrt(sq_sum / static_cast<double>(win - 1));
        double lower = mean - 2.0 * std_dev;
        double upper = mean + 2.0 * std_dev;

        if (prices[i] < lower)
          positions[i] = 1; // Oversold -> Buy
        else if (prices[i] > upper)
          positions[i] = 0; // Overbought -> Exit
        else
          positions[i] = positions[i - 1];
      }
    } else if (strat == StrategyType::RsiMomentum) {
      res.strategy_name =
          "Wilder RSI(14) Mean-Reversion (Buy<30, Exit>70, 10bps Cost)";
      constexpr size_t period = 14;
      if (n > period + 1) {
        double avg_gain = 0.0, avg_loss = 0.0;
        for (size_t i = 1; i <= period; ++i) {
          double diff = prices[i] - prices[i - 1];
          if (diff > 0.0)
            avg_gain += diff;
          else
            avg_loss -= diff;
        }
        avg_gain /= static_cast<double>(period);
        avg_loss /= static_cast<double>(period);

        for (size_t i = period + 1; i < n; ++i) {
          double diff = prices[i] - prices[i - 1];
          double g = (diff > 0.0) ? diff : 0.0;
          double l = (diff < 0.0) ? -diff : 0.0;
          avg_gain =
              (avg_gain * (period - 1) + g) / static_cast<double>(period);
          avg_loss =
              (avg_loss * (period - 1) + l) / static_cast<double>(period);
          double rsi = (avg_loss < 1e-12)
                           ? 100.0
                           : (100.0 - (100.0 / (1.0 + avg_gain / avg_loss)));

          if (rsi < 30.0)
            positions[i] = 1;
          else if (rsi > 70.0)
            positions[i] = 0;
          else
            positions[i] = positions[i - 1];
        }
      }
    } else if (strat == StrategyType::RLDynamicPortfolio) {
      // Silahian Ch 4: Walk-Forward Q-Learning + Experience Replay Trading
      // Agent (Causal)
      res.strategy_name = std::format(
          "Silahian Ch.4 RL Q-Learning + Experience Replay ({:.0f}bps)",
          one_way_cost_bps);
      constexpr size_t kLookback = 20;
      constexpr int kStates = 18; // 3 trend x 3 z-score x 2 vol regimes
      constexpr int kActs = 2;    // 0 = Flat, 1 = Long

      std::array<std::array<double, kActs>, kStates> q{};
      for (auto &row : q)
        row = {0.0, 0.0002};

      struct ReplayItem {
        int s;
        int a;
        double r;
        int s_next;
      };
      std::vector<ReplayItem> replay;
      replay.reserve(256);

      std::mt19937_64 rng(42ULL);
      std::uniform_real_distribution<double> u01(0.0, 1.0);
      double epsilon = 0.25;
      constexpr double alpha = 0.16;
      constexpr double gamma = 0.85;

      auto state_at = [&](size_t idx) -> int {
        double f_sma = 0.0, s_sma = 0.0;
        const size_t f_p = std::min<size_t>(
            kLookback, static_cast<size_t>(std::max(3, fast_period)));
        for (size_t j = idx + 1 - f_p; j <= idx; ++j)
          f_sma += prices[j];
        f_sma /= static_cast<double>(f_p);

        for (size_t j = idx + 1 - kLookback; j <= idx; ++j)
          s_sma += prices[j];
        s_sma /= static_cast<double>(kLookback);

        double sq = 0.0, r_abs = 0.0;
        for (size_t j = idx + 1 - kLookback; j <= idx; ++j) {
          const double d = prices[j] - s_sma;
          sq += d * d;
          if (j > idx + 1 - kLookback && prices[j - 1] > 0.0) {
            r_abs += std::abs(prices[j] - prices[j - 1]) / prices[j - 1];
          }
        }
        const double sd =
            std::sqrt(std::max(1e-10, sq / static_cast<double>(kLookback - 1)));
        const double trend = (s_sma > 0.0) ? ((f_sma - s_sma) / s_sma) : 0.0;
        const double z = (sd > 1e-8) ? ((prices[idx] - s_sma) / sd) : 0.0;
        const double avg_move = r_abs / static_cast<double>(kLookback - 1);

        const int t_bin = (trend < -0.005) ? 0 : ((trend > 0.005) ? 2 : 1);
        const int z_bin = (z < -1.0) ? 0 : ((z > 1.0) ? 2 : 1);
        const int v_bin = (avg_move > 0.018) ? 1 : 0;
        return (t_bin * 6) + (z_bin * 2) + v_bin;
      };

      int prev_s = -1, prev_a = 0;
      for (size_t i = kLookback; i < n; ++i) {
        const int cur_s = state_at(i);
        // Strictly causal transition update once bar i return [i-1 -> i] is
        // observed
        if (prev_s >= 0) {
          const double r_bar =
              (prices[i - 1] > 0.0)
                  ? ((prices[i] - prices[i - 1]) / prices[i - 1])
                  : 0.0;
          const double net_bar = (prev_a == 1 ? r_bar : 0.0) -
                                 (prev_a != positions[i - 2] ? cost_rate : 0.0);
          const double reward =
              net_bar * 100.0 - 0.15 * std::max(0.0, -net_bar * 100.0);
          replay.push_back(ReplayItem{prev_s, prev_a, reward, cur_s});
          if (replay.size() > 180)
            replay.erase(replay.begin());

          const size_t mb = std::min<size_t>(8, replay.size());
          for (size_t k = 0; k < mb; ++k) {
            const auto &ex = replay[static_cast<size_t>(rng() % replay.size())];
            const double max_next = std::max(q[ex.s_next][0], q[ex.s_next][1]);
            q[ex.s][ex.a] += alpha * (ex.r + gamma * max_next - q[ex.s][ex.a]);
          }
        }

        int act = (q[cur_s][1] > q[cur_s][0]) ? 1 : 0;
        if (u01(rng) < epsilon)
          act = static_cast<int>(rng() % 2);
        positions[i] = act;
        prev_s = cur_s;
        prev_a = act;
        epsilon = std::max(0.03, epsilon * 0.975);
      }
    } else {
      // Silahian Ch 3 + Ch 7: Game-Theoretic Microstructure Order Flow
      // Imbalance (OFI)
      res.strategy_name = std::format(
          "Silahian Ch.3/7 Game-Theoretic Flow Imbalance ({:.0f}bps)",
          one_way_cost_bps);
      const bool has_ohlcv =
          (highs.size() == n && lows.size() == n && volumes.size() == n);
      constexpr size_t win = 10;
      for (size_t i = win; i < n; ++i) {
        double obi_sum = 0.0, vol_sum = 0.0, sq_ret = 0.0;
        for (size_t j = i + 1 - win; j <= i; ++j) {
          const double r = (j > 0 && prices[j - 1] > 0.0)
                               ? ((prices[j] - prices[j - 1]) / prices[j - 1])
                               : 0.0;
          sq_ret += r * r;
          double bar_imb = (r > 0.0) ? 1.0 : ((r < 0.0) ? -1.0 : 0.0);
          double w = 1.0;
          if (has_ohlcv) {
            const double span = highs[j] - lows[j];
            if (span > 1e-9) {
              bar_imb = ((prices[j] - lows[j]) - (highs[j] - prices[j])) /
                        span; // CLV in [-1, 1]
            }
            w = std::max(1.0, volumes[j]);
          }
          obi_sum += bar_imb * w;
          vol_sum += w;
        }
        const double net_obi = (vol_sum > 0.0) ? (obi_sum / vol_sum) : 0.0;
        const double vol_loc = std::sqrt(sq_ret / static_cast<double>(win));
        // Game-theoretic adverse selection guard widens threshold during high
        // turbulence
        const double entry_thr = 0.14 + 2.5 * vol_loc;
        if (net_obi > entry_thr)
          positions[i] = 1;
        else if (net_obi < -0.10)
          positions[i] = 0;
        else
          positions[i] = positions[i - 1];
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
      double asset_return = (prices[i - 1] > 0.0)
                                ? ((prices[i] - prices[i - 1]) / prices[i - 1])
                                : 0.0;
      double net_ret = active_pos * asset_return - turnover * cost_rate;
      daily_returns.push_back(net_ret);

      if (prev_pos == 1 && active_pos == 0) {
        ++total_trades;
        if (capital * (1.0 - cost_rate) > entry_capital)
          ++win_trades;
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
      if (capital > entry_capital)
        ++win_trades;
    }

    res.total_return_pct = ((capital - 100000.0) / 100000.0) * 100.0;
    res.max_drawdown_pct = max_dd * 100.0;
    res.total_trades = total_trades;
    res.win_rate_pct =
        (total_trades > 0)
            ? (static_cast<double>(win_trades) / total_trades * 100.0)
            : 0.0;

    if (daily_returns.size() >= 2) {
      double mean_r =
          std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) /
          static_cast<double>(daily_returns.size());
      double sq_diff = 0.0;
      for (double r : daily_returns)
        sq_diff += (r - mean_r) * (r - mean_r);
      double std_r =
          std::sqrt(sq_diff / static_cast<double>(daily_returns.size() - 1));
      res.annualized_sharpe =
          (std_r > 1e-8) ? ((mean_r / std_r) * std::sqrt(252.0)) : 0.0;
    }

    return res;
  }
};

} // namespace quant
