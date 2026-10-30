#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "forecast_engine.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <format>
#include <iostream>
#include <numeric>
#include <algorithm>

namespace quant {

struct SampleForecastRecord {
    int bar_index{0};
    double spot_at_forecast{0.0};
    double predicted_target{0.0};
    double bull_band{0.0};
    double bear_band{0.0};
    std::string predicted_direction;
    double actual_price_at_horizon{0.0};
    double actual_return_pct{0.0};
    bool is_direction_hit{false};
    bool is_in_band{false};
};

struct ForecastBacktestReport {
    std::string symbol;
    std::string currency;
    int horizon_days{5}; // Tahmin vadesi
    int total_predictions{0};
    int correct_direction_hits{0};
    double directional_accuracy_pct{0.0};

    int in_band_hits{0};
    double in_band_coverage_pct{0.0};

    double mean_absolute_error_pct{0.0}; // MAPE

    // Strateji PnL vs Buy & Hold
    double strategy_return_pct{0.0};
    double buy_and_hold_return_pct{0.0};
    double strategy_sharpe{0.0};
    double strategy_max_drawdown_pct{0.0};
    double buy_and_hold_max_drawdown_pct{0.0};
    int total_trades{0};
    int win_trades{0};
    double trade_win_rate_pct{0.0};

    std::vector<SampleForecastRecord> recent_samples;
};

class ForecastBacktestEngine {
public:
    static ForecastBacktestReport run_walk_forward_backtest(LiveMarketDataClient& client,
                                                            const std::string& symbol,
                                                            int horizon_days = 5,
                                                            const std::string& range = "1y") {
        ForecastBacktestReport rep;
        rep.symbol = symbol;
        rep.horizon_days = horizon_days;
        rep.currency = symbol.ends_with(".IS") ? "TL" : (symbol.ends_with(".DE") ? "EUR" : "$");

        // 1 Yıllık tarihsel fiyat ve hacim verisini çek
        auto data = client.fetch_asset(symbol, range);
        const auto& prices = data.historical_prices;
        const auto& volumes = data.historical_volumes;

        const size_t N = prices.size();
        const size_t window = 30; // 30 günlük geçmiş pencere

        if (N < window + horizon_days + 5) {
            return rep;
        }

        double total_abs_error = 0.0;
        int in_band_count = 0;
        int hit_count = 0;
        int total_preds = 0;

        // Trading Strategy PnL takibi
        double capital = 100000.0;
        double peak_capital = capital;
        double max_dd = 0.0;
        std::vector<double> daily_strat_rets;

        bool in_position = false;
        double entry_price = 0.0;
        int trades = 0, win_trades = 0;

        // Buy & Hold
        double bnh_start = prices[window];
        double bnh_peak = bnh_start;
        double bnh_max_dd = 0.0;

        for (size_t i = window; i < N; ++i) {
            bnh_peak = std::max(bnh_peak, prices[i]);
            double dd = (bnh_peak - prices[i]) / bnh_peak;
            bnh_max_dd = std::max(bnh_max_dd, dd);
        }
        rep.buy_and_hold_return_pct = ((prices.back() - bnh_start) / bnh_start) * 100.0;
        rep.buy_and_hold_max_drawdown_pct = bnh_max_dd * 100.0;

        // Walk-Forward Kayan Pencere Döngüsü
        for (size_t t = window; t < N - horizon_days; ++t) {
            double p_t = prices[t];

            // 1. O tarihe kadarki 30-günlük VWAP
            double sum_pv = 0.0, sum_v = 0.0;
            for (size_t k = t - window + 1; k <= t; ++k) {
                double v = (k < volumes.size()) ? volumes[k] : 1000.0;
                sum_pv += prices[k] * v;
                sum_v += v;
            }
            double vwap_t = (sum_v > 0.0) ? (sum_pv / sum_v) : p_t;

            // 2. O tarihe kadarki 30-günlük Yıllıklandırılmış Volatilite
            std::vector<double> log_rets;
            for (size_t k = t - window + 1; k <= t; ++k) {
                if (prices[k - 1] > 0.0) {
                    log_rets.push_back(std::log(prices[k] / prices[k - 1]));
                }
            }
            double vol_t = 0.20;
            if (log_rets.size() > 1) {
                double m = std::accumulate(log_rets.begin(), log_rets.end(), 0.0) / log_rets.size();
                double sq = 0.0;
                for (double r : log_rets) sq += (r - m) * (r - m);
                vol_t = std::sqrt((sq / log_rets.size()) * 252.0);
                vol_t = std::clamp(vol_t, 0.06, 0.80);
            }

            // 3. İndikatörler (SMA, RSI)
            double sma10 = 0.0, sma30 = 0.0;
            for (size_t k = t - 9; k <= t; ++k) sma10 += prices[k];
            sma10 /= 10.0;
            for (size_t k = t - 29; k <= t; ++k) sma30 += prices[k];
            sma30 /= 30.0;

            double gains = 0.0, losses = 0.0;
            for (size_t k = t - 13; k <= t; ++k) {
                double diff = prices[k] - prices[k - 1];
                if (diff > 0) gains += diff;
                else losses += std::abs(diff);
            }
            double rsi = (losses > 1e-9) ? (100.0 - (100.0 / (1.0 + (gains / losses)))) : 50.0;

            // 4. Sentimel ve Momentum Proxy (-1.0 ile +1.0)
            double trend_score = (sma10 > sma30) ? 0.70 : -0.70;
            double vwap_bias = (p_t >= vwap_t) ? 0.30 : -0.30;
            double rsi_bias = 0.0;
            if (rsi > 70.0) rsi_bias = -0.40;      // Aşırı alım düzeltme riski
            else if (rsi < 35.0) rsi_bias = +0.40; // Aşırı satım tepki alımı

            double s_t = std::clamp(trend_score + vwap_bias + rsi_bias, -1.0, 1.0);

            // 5. Quant Forecast Engine Çağrısı
            RealTimeAssetData sub_data;
            sub_data.symbol = symbol;
            sub_data.current_price = p_t;
            sub_data.vwap = vwap_t;
            sub_data.annualized_volatility = vol_t;
            sub_data.is_live = true;

            auto fc = QuantForecastEngine::forecast_for_days(sub_data, s_t, horizon_days);

            // 6. Horizon Sonrasındaki Gerçek Fiyatla Karşılaştırma
            double p_actual = prices[t + horizon_days];
            double actual_ret = (p_actual - p_t) / p_t;
            total_preds++;

            bool dir_hit = false;
            if (fc.quant_score >= 0.10 && actual_ret > 0.0) dir_hit = true;
            else if (fc.quant_score <= -0.10 && actual_ret < 0.0) dir_hit = true;
            else if (std::abs(fc.quant_score) < 0.10 && std::abs(actual_ret) < 0.02) dir_hit = true;

            if (dir_hit) hit_count++;

            bool in_band = (p_actual >= fc.bear_support && p_actual <= fc.bull_target);
            if (in_band) in_band_count++;

            double mape = std::abs((fc.target_price - p_actual) / p_actual) * 100.0;
            total_abs_error += mape;

            // Strateji Simülasyonu: Boğa ise Al, Ayı ise Nakite Geç
            bool is_bull = (fc.direction == ForecastDirection::StrongBullish || fc.direction == ForecastDirection::Bullish);
            bool is_bear = (fc.direction == ForecastDirection::StrongBearish || fc.direction == ForecastDirection::Bearish);

            if (is_bull && !in_position) {
                in_position = true;
                entry_price = p_t;
                trades++;
            } else if (is_bear && in_position) {
                in_position = false;
                if (p_t > entry_price) win_trades++;
            }

            double daily_ret = (prices[t + 1] - p_t) / p_t;
            double strat_ret = in_position ? daily_ret : 0.0;
            daily_strat_rets.push_back(strat_ret);

            capital *= (1.0 + strat_ret);
            peak_capital = std::max(peak_capital, capital);
            double dd = (peak_capital - capital) / peak_capital;
            max_dd = std::max(max_dd, dd);

            // Son 5 örneği raporda göstermek için sakla
            if (t >= N - horizon_days - 5) {
                SampleForecastRecord srec;
                srec.bar_index = static_cast<int>(t);
                srec.spot_at_forecast = p_t;
                srec.predicted_target = fc.target_price;
                srec.bull_band = fc.bull_target;
                srec.bear_band = fc.bear_support;
                srec.predicted_direction = (fc.quant_score >= 0.10) ? "YUKARI" : ((fc.quant_score <= -0.10) ? "AŞAĞI" : "NÖTR");
                srec.actual_price_at_horizon = p_actual;
                srec.actual_return_pct = actual_ret * 100.0;
                srec.is_direction_hit = dir_hit;
                srec.is_in_band = in_band;
                rep.recent_samples.push_back(srec);
            }
        }

        // Özet Metrikleri Kaydet
        rep.total_predictions = total_preds;
        rep.correct_direction_hits = hit_count;
        rep.directional_accuracy_pct = (total_preds > 0) ? (static_cast<double>(hit_count) / total_preds * 100.0) : 0.0;
        rep.in_band_hits = in_band_count;
        rep.in_band_coverage_pct = (total_preds > 0) ? (static_cast<double>(in_band_count) / total_preds * 100.0) : 0.0;
        rep.mean_absolute_error_pct = (total_preds > 0) ? (total_abs_error / total_preds) : 0.0;

        rep.strategy_return_pct = ((capital - 100000.0) / 100000.0) * 100.0;
        rep.strategy_max_drawdown_pct = max_dd * 100.0;
        rep.total_trades = trades;
        rep.win_trades = win_trades;
        rep.trade_win_rate_pct = (trades > 0) ? (static_cast<double>(win_trades) / trades * 100.0) : 0.0;

        if (!daily_strat_rets.empty()) {
            double mean_r = std::accumulate(daily_strat_rets.begin(), daily_strat_rets.end(), 0.0) / daily_strat_rets.size();
            double sq_r = 0.0;
            for (double r : daily_strat_rets) sq_r += (r - mean_r) * (r - mean_r);
            double std_r = std::sqrt(sq_r / daily_strat_rets.size());
            rep.strategy_sharpe = (std_r > 1e-8) ? ((mean_r / std_r) * std::sqrt(252.0)) : 0.0;
        }

        return rep;
    }

    static void render_backtest_report(const ForecastBacktestReport& rep) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << std::format("║  📊 {} FORECAST ENGINE WALK-FORWARD BACKTEST RAPORU (1 YIL){:>17}║\n",
                                 rep.symbol, " ");
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        std::cout << std::format("   Test Edilen Varlık      : {}{}{}\n", pal.glow_rgb, rep.symbol, pal.reset_code);
        std::cout << std::format("   Tahmin Ufku / Vade      : {}{} Gün (İleriye Dönük Kayan Pencere){}\n", pal.glow_rgb, rep.horizon_days, pal.reset_code);
        std::cout << std::format("   Toplam Test Pencereleri : {} Adet Günlük Tahmin Barı\n\n", rep.total_predictions);

        std::cout << pal.glow_rgb << "   1. TAHMİN İSABETİ & HATA METRİKLERİ:\n" << pal.reset_code;
        std::string hit_color = (rep.directional_accuracy_pct >= 50.0) ? pal.green_rgb : pal.base_rgb;
        std::cout << std::format("   • Yön İsabet Oranı (Hit Rate)   : {}{:.1f}%{} ({}/{} Başarılı Tahmin)\n",
                                 hit_color, rep.directional_accuracy_pct, pal.reset_code,
                                 rep.correct_direction_hits, rep.total_predictions);
        std::cout << std::format("   • Olasılık Bandı Kapsama (±1σ)  : {}{:.1f}%{} (Teorik Beklenti: %68.27)\n",
                                 pal.glow_rgb, rep.in_band_coverage_pct, pal.reset_code);
        std::cout << std::format("   • Hedef Fiyat Hata Payı (MAPE)  : %{:.2f} (Tahmin ile Gerçek Arası Sapma)\n\n",
                                 rep.mean_absolute_error_pct);

        std::cout << pal.glow_rgb << "   2. SİNYAL STRATEJİSİ GETİRİSİ vs AL VE TUT (BUY & HOLD):\n" << pal.reset_code;
        std::string strat_color = (rep.strategy_return_pct >= rep.buy_and_hold_return_pct) ? pal.green_rgb : pal.base_rgb;
        std::cout << std::format("   • Model Strateji Getirisi       : {}{:+6.2f}%{}\n", strat_color, rep.strategy_return_pct, pal.reset_code);
        std::cout << std::format("   • Al ve Tut (Buy & Hold)        : {:+6.2f}%\n", rep.buy_and_hold_return_pct);
        std::cout << std::format("   • Strateji Sharpe Oranı         : {:.2f}\n", rep.strategy_sharpe);
        std::cout << std::format("   • Strateji Maksimum Risk (DD)   : %{:.1f} (Buy&Hold DD: %{:.1f})\n",
                                 rep.strategy_max_drawdown_pct, rep.buy_and_hold_max_drawdown_pct);
        std::cout << std::format("   • İşlem Başarı Oranı (Win Rate) : %{:.1f} ({}/{} Kârlı Pozisyon)\n",
                                 rep.trade_win_rate_pct, rep.win_trades, rep.total_trades);

        std::cout << pal.glow_rgb;
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << "║ SON 5 TEST PENCERESİNİN GERÇEKLEŞEN SOMUT KARŞILAŞTIRMASI                     ║\n";
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << "║ GÜN  SPOT       TAHMİN    HEDEF     GERÇEK    DEĞİŞİM  YÖN SONUÇ  BANT KORUMA║\n";
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << pal.reset_code;

        for (const auto& s : rep.recent_samples) {
            std::string hit_badge = s.is_direction_hit ? (pal.green_rgb + "✔ BAŞARILI" + pal.reset_code) : (pal.dim_rgb + "✘ SAPMA   " + pal.reset_code);
            std::string band_badge = s.is_in_band ? (pal.green_rgb + "✔ BANTTA  " + pal.reset_code) : (pal.dim_rgb + "✘ DIŞINDA " + pal.reset_code);

            std::cout << std::format("  t-{:<2} {:<10.2f} {:<9} {:<9.2f} {:<9.2f} {:+5.1f}%  {} {}\n",
                                     rep.horizon_days, s.spot_at_forecast,
                                     s.predicted_direction, s.predicted_target,
                                     s.actual_price_at_horizon, s.actual_return_pct,
                                     hit_badge, band_badge);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }
};

} // namespace quant
