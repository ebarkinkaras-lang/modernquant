#pragma once

#include "theme_engine.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <format>

namespace quant {

struct CandleBar {
    double open{0.0};
    double high{0.0};
    double low{0.0};
    double close{0.0};
    double volume{0.0};
};

class TerminalChartEngine {
public:
    // Render a high-definition in-terminal ASCII/Unicode Candlestick chart
    static void render(const std::string& symbol,
                       const std::vector<double>& prices,
                       const std::vector<double>& volumes = {},
                       int chart_height = 13,
                       size_t num_bars = 28) {
        
        const auto& pal = ThemeEngine::instance().palette();

        if (prices.size() < 2) {
            std::cout << pal.red_rgb << "HATA: Grafik cizmek icin yetersiz veri noktasi.\n" << pal.reset_code;
            return;
        }

        // Subsample or take the last N prices
        size_t count = std::min(prices.size(), num_bars);
        std::vector<CandleBar> candles;
        candles.reserve(count);

        size_t start_idx = prices.size() - count;
        for (size_t i = start_idx; i < prices.size(); ++i) {
            double c = prices[i];
            double prev_c = (i > 0) ? prices[i - 1] : c;
            double o = prev_c;
            // Synthesize realistic intra-period high/low if only close is given
            double dev = std::abs(c - o) * 0.35 + (c * 0.003);
            double h = std::max(o, c) + dev;
            double l = std::min(o, c) - dev;
            double v = (!volumes.empty() && i < volumes.size()) ? volumes[i] : 100000.0;
            candles.push_back(CandleBar{o, h, l, c, v});
        }

        // Calculate 20-period SMA & 14-period RSI using C++ algorithms
        double sma20 = 0.0;
        size_t sma_len = std::min<size_t>(20, prices.size());
        sma20 = std::accumulate(prices.end() - sma_len, prices.end(), 0.0) / sma_len;

        double rsi14 = calculate_rsi_(prices, 14);
        std::string rsi_status = (rsi14 >= 70.0) ? (pal.red_rgb + "ASIRI ALIM" + pal.reset_code) :
                                 (rsi14 <= 30.0) ? (pal.green_rgb + "ASIRI SATIM" + pal.reset_code) :
                                 (pal.dim_rgb + "NOTR" + pal.reset_code);

        double last_p = candles.back().close;
        double first_p = candles.front().open;
        double change_pct = ((last_p - first_p) / first_p) * 100.0;
        std::string chg_color = (change_pct >= 0.0) ? pal.green_rgb : pal.red_rgb;

        // Chart Header Panel (Bloomberg / 80s Unix Style)
        std::cout << "\n";
        ThemeEngine::instance().print_rule("TERMINAL MUM GRAFIGI [C++20 RETRO ENGINE]");
        std::cout << std::format("  {}SYMBOL: {}{:<10}{}  │  SON FIYAT: {}${:<8.2f}{} ({}{:+6.2f}%{})\n",
                                 pal.glow_rgb, pal.reset_code, symbol, pal.reset_code,
                                 chg_color, last_p, pal.reset_code,
                                 chg_color, change_pct, pal.reset_code);
        std::cout << std::format("  SMA(20): {}${:<8.2f}{}    │  RSI(14): {}{:<5.1f}{} ({})\n",
                                 pal.glow_rgb, sma20, pal.reset_code,
                                 pal.base_rgb, rsi14, pal.reset_code, rsi_status);
        ThemeEngine::instance().print_rule();

        // Find min and max
        double min_val = 1e12, max_val = -1e12;
        double max_vol = 1.0;
        for (const auto& bar : candles) {
            min_val = std::min(min_val, bar.low);
            max_val = std::max(max_val, bar.high);
            max_vol = std::max(max_vol, bar.volume);
        }
        if (std::abs(max_val - min_val) < 1e-6) max_val += 1.0;

        auto price_to_y = [&](double p) -> int {
            int y = static_cast<int>((p - min_val) / (max_val - min_val) * (chart_height - 1));
            return std::clamp(y, 0, chart_height - 1);
        };

        // Render candlestick canvas
        for (int r = chart_height - 1; r >= 0; --r) {
            double price_level = min_val + (static_cast<double>(r) / (chart_height - 1)) * (max_val - min_val);
            std::cout << std::format("  {}${:7.2f} {}│ ", pal.dim_rgb, price_level, pal.base_rgb);

            for (const auto& bar : candles) {
                int y_h = price_to_y(bar.high);
                int y_l = price_to_y(bar.low);
                int y_o = price_to_y(bar.open);
                int y_c = price_to_y(bar.close);

                int body_top = std::max(y_o, y_c);
                int body_bot = std::min(y_o, y_c);
                bool bullish = bar.close >= bar.open;
                std::string col = bullish ? pal.green_rgb : pal.red_rgb;

                if (r >= body_bot && r <= body_top) {
                    std::cout << col << (bullish ? "█" : "▒") << " " << pal.reset_code;
                } else if (r >= y_l && r <= y_h) {
                    std::cout << col << "│ " << pal.reset_code;
                } else {
                    std::cout << "  ";
                }
            }
            std::cout << "\n";
        }

        // X-axis timeline line
        std::cout << "           " << pal.base_rgb << "└";
        for (size_t i = 0; i < candles.size() * 2; ++i) std::cout << "─";
        std::cout << "► (Zaman Serisi)" << pal.reset_code << "\n";

        // Volume histogram bar (UTF-8 unicode block characters)
        const std::vector<std::string> vol_chars = {" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
        std::cout << std::format("  {}HACIM    {}│ ", pal.dim_rgb, pal.base_rgb);
        for (const auto& bar : candles) {
            double ratio = bar.volume / max_vol;
            int idx = std::clamp(static_cast<int>(ratio * (vol_chars.size() - 1)), 0, static_cast<int>(vol_chars.size() - 1));
            bool bullish = bar.close >= bar.open;
            std::string col = bullish ? pal.green_rgb : pal.red_rgb;
            std::cout << col << vol_chars[idx] << " " << pal.reset_code;
        }
        std::cout << "\n";
        ThemeEngine::instance().print_rule();
    }

private:
    static double calculate_rsi_(const std::vector<double>& prices, size_t period = 14) {
        if (prices.size() <= period) return 50.0;

        std::vector<double> gains, losses;
        for (size_t i = 1; i < prices.size(); ++i) {
            double diff = prices[i] - prices[i - 1];
            if (diff > 0) {
                gains.push_back(diff);
                losses.push_back(0.0);
            } else {
                gains.push_back(0.0);
                losses.push_back(std::abs(diff));
            }
        }

        size_t n = gains.size();
        size_t p = std::min(period, n);
        double avg_gain = std::accumulate(gains.end() - p, gains.end(), 0.0) / p;
        double avg_loss = std::accumulate(losses.end() - p, losses.end(), 0.0) / p;

        if (avg_loss < 1e-9) return 100.0;
        double rs = avg_gain / avg_loss;
        return 100.0 - (100.0 / (1.0 + rs));
    }
};

} // namespace quant
