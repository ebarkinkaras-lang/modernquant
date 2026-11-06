#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "forecast_engine.hpp"
#include "i18n.hpp"
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
    std::string range_used{"2y"};
    bool is_live_data{false};
    int horizon_days{5}; // İş günü cinsinden tahmin vadesi

    int total_predictions{0};
    int correct_direction_hits{0};
    double directional_accuracy_pct{0.0};

    // Active (non-neutral |score| >= 0.10) directional accuracy & benchmarks
    int active_predictions{0};
    int active_direction_hits{0};
    double active_directional_accuracy_pct{0.0};
    double always_up_accuracy_pct{0.0};

    // Non-overlapping independent horizon blocks (eliminates overlapping autocorrelation bias)
    int non_overlapping_blocks{0};
    int non_overlapping_hits{0};
    double non_overlapping_accuracy_pct{0.0};
    double binomial_z_stat{0.0};

    int in_band_hits{0};
    double in_band_coverage_pct{0.0};

    double mean_absolute_error_pct{0.0}; // Model MAPE
    double random_walk_mape_pct{0.0};    // Random-Walk (No-Change) Benchmark MAPE

    // Net Strategy PnL (after 10 bps one-way transaction cost) vs Buy & Hold
    double transaction_cost_bps{10.0};
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
    // Walk-Forward Out-of-Sample Backtest running the EXACT live QuantForecastEngine core
    static ForecastBacktestReport run_walk_forward_backtest(LiveMarketDataClient& client,
                                                            const std::string& raw_symbol,
                                                            int horizon_days = 5,
                                                            const std::string& range = "2y") {
        std::string symbol = LiveMarketDataClient::normalize_symbol(raw_symbol);
        auto data = client.fetch_asset(symbol, range);
        return evaluate_on_asset_data(data, horizon_days, range);
    }

    // Pure deterministic evaluator (also directly unit-testable on controlled synthetic series)
    static ForecastBacktestReport evaluate_on_asset_data(const RealTimeAssetData& data,
                                                         int horizon_days = 5,
                                                         const std::string& range_label = "2y") {
        ForecastBacktestReport rep;
        rep.symbol = data.symbol;
        rep.range_used = range_label;
        rep.is_live_data = data.is_live;
        rep.horizon_days = std::max(1, horizon_days);
        rep.currency = (!data.currency_display.empty())
            ? data.currency_display
            : LiveMarketDataClient::currency_display_for(LiveMarketDataClient::infer_currency_code(data.symbol), data.symbol);

        const auto& prices = data.historical_prices;
        const size_t N = prices.size();
        const size_t h_step = static_cast<size_t>(rep.horizon_days);

        // Use 60-day causal warmup on 1y/2y histories; gracefully adapt to 30-day warmup on shorter 6mo slices
        const size_t window = (N >= 140 + h_step) ? 60 : 30;
        if (N < window + h_step + 5) {
            return rep;
        }

        double total_abs_error = 0.0;
        double total_rw_abs_error = 0.0;
        int in_band_count = 0;
        int hit_count = 0;
        int up_days_count = 0;
        int active_preds = 0;
        int active_hits = 0;
        int non_overlap_blocks = 0;
        int non_overlap_hits = 0;
        int total_preds = 0;

        // Net Strategy PnL with 10 bps (0.10%) one-way execution cost
        constexpr double kOneWayFee = 0.0010;
        double capital = 100000.0;
        double peak_capital = capital;
        double max_dd = 0.0;
        std::vector<double> daily_strat_rets;

        bool in_position = false;
        double entry_capital = capital;
        int trades = 0;
        int win_trades = 0;

        // Buy & Hold benchmark over exact identical evaluation window [window .. N - h_step]
        const size_t eval_end = N - h_step;
        double bnh_start = prices[window];
        double bnh_peak = bnh_start;
        double bnh_max_dd = 0.0;
        for (size_t i = window; i <= eval_end; ++i) {
            bnh_peak = std::max(bnh_peak, prices[i]);
            if (bnh_peak > 0.0) {
                bnh_max_dd = std::max(bnh_max_dd, (bnh_peak - prices[i]) / bnh_peak);
            }
        }
        if (bnh_start > 0.0) {
            rep.buy_and_hold_return_pct = ((prices[eval_end] - bnh_start) / bnh_start) * 100.0;
        }
        rep.buy_and_hold_max_drawdown_pct = bnh_max_dd * 100.0;

        // Causal Walk-Forward Loop (strictly zero look-ahead: slice [t - lookback .. t])
        const size_t slice_cap = 90; // 90 causal bars are sufficient for 60d momentum, EWMA(0.94) vol, 22d VWAP & 14d ATR
        for (size_t t = window; t < eval_end; ++t) {
            double p_t = prices[t];
            if (p_t <= 0.0) continue;

            size_t s0 = (t > slice_cap) ? (t - slice_cap) : 0;
            RealTimeAssetData sub;
            sub.symbol = data.symbol;
            sub.currency_code = data.currency_code;
            sub.currency_display = rep.currency;
            sub.current_price = p_t;
            sub.is_live = data.is_live;

            sub.historical_prices.assign(prices.begin() + static_cast<std::ptrdiff_t>(s0),
                                         prices.begin() + static_cast<std::ptrdiff_t>(t + 1));
            if (data.historical_raw_closes.size() == N) {
                sub.historical_raw_closes.assign(data.historical_raw_closes.begin() + static_cast<std::ptrdiff_t>(s0),
                                                 data.historical_raw_closes.begin() + static_cast<std::ptrdiff_t>(t + 1));
            } else {
                sub.historical_raw_closes = sub.historical_prices;
            }
            if (data.historical_opens.size() == N && data.historical_highs.size() == N && data.historical_lows.size() == N) {
                sub.historical_opens.assign(data.historical_opens.begin() + static_cast<std::ptrdiff_t>(s0),
                                            data.historical_opens.begin() + static_cast<std::ptrdiff_t>(t + 1));
                sub.historical_highs.assign(data.historical_highs.begin() + static_cast<std::ptrdiff_t>(s0),
                                            data.historical_highs.begin() + static_cast<std::ptrdiff_t>(t + 1));
                sub.historical_lows.assign(data.historical_lows.begin() + static_cast<std::ptrdiff_t>(s0),
                                           data.historical_lows.begin() + static_cast<std::ptrdiff_t>(t + 1));
            }
            if (data.historical_volumes.size() == N) {
                sub.historical_volumes.assign(data.historical_volumes.begin() + static_cast<std::ptrdiff_t>(s0),
                                              data.historical_volumes.begin() + static_cast<std::ptrdiff_t>(t + 1));
            }

            LiveMarketDataClient::compute_analytics(sub);

            // Call exact live price/volume engine (has_live_news = false on historical bars)
            auto fc = QuantForecastEngine::forecast_for_trading_days(sub, rep.horizon_days, 0.0, false, nullptr);

            double p_actual = prices[t + h_step];
            if (p_actual <= 0.0) continue;

            double actual_ret = (p_actual - p_t) / p_t;
            total_preds++;
            if (actual_ret > 0.0) up_days_count++;

            // Strict directional hit (no neutral freebie!)
            bool pred_up = (fc.expected_return_pct >= 0.0);
            bool dir_hit = (pred_up && actual_ret >= 0.0) || (!pred_up && actual_ret < 0.0);
            if (dir_hit) hit_count++;

            // Active conviction subset (|quant_score| >= 0.10)
            if (std::abs(fc.quant_score) >= 0.10) {
                active_preds++;
                if ((fc.quant_score > 0.0 && actual_ret > 0.0) || (fc.quant_score < 0.0 && actual_ret < 0.0)) {
                    active_hits++;
                }
            }

            // Independent non-overlapping horizon blocks (every h_step bars)
            if (((t - window) % h_step) == 0) {
                non_overlap_blocks++;
                if (dir_hit) non_overlap_hits++;
            }

            bool in_band = (p_actual >= fc.bear_support && p_actual <= fc.bull_target);
            if (in_band) in_band_count++;

            total_abs_error += std::abs((fc.target_price - p_actual) / p_actual) * 100.0;
            total_rw_abs_error += std::abs((p_t - p_actual) / p_actual) * 100.0;

            // Realistic Next-Bar Execution Strategy with 10 bps One-Way Fee
            bool want_long = (fc.quant_score >= 0.08);
            bool want_exit = (fc.quant_score <= -0.05);

            double fee_drag = 0.0;
            if (want_long && !in_position) {
                in_position = true;
                fee_drag = kOneWayFee;
                capital *= (1.0 - fee_drag);
                entry_capital = capital;
                trades++;
            } else if (want_exit && in_position) {
                in_position = false;
                fee_drag = kOneWayFee;
                capital *= (1.0 - fee_drag);
                if (capital > entry_capital) win_trades++;
            }

            double next_bar_ret = (prices[t + 1] - p_t) / p_t;
            double net_daily_ret = in_position ? next_bar_ret : 0.0;
            // Include fee impact in daily strategy return series for honest Sharpe calculation
            double total_step_ret = (1.0 - fee_drag) * (1.0 + net_daily_ret) - 1.0;
            daily_strat_rets.push_back(total_step_ret);

            capital *= (1.0 + net_daily_ret);
            peak_capital = std::max(peak_capital, capital);
            if (peak_capital > 0.0) {
                max_dd = std::max(max_dd, (peak_capital - capital) / peak_capital);
            }

            if (t + 5 >= eval_end) {
                SampleForecastRecord srec;
                srec.bar_index = static_cast<int>(eval_end - t);
                srec.spot_at_forecast = p_t;
                srec.predicted_target = fc.target_price;
                srec.bull_band = fc.bull_target;
                srec.bear_band = fc.bear_support;
                srec.predicted_direction = (fc.quant_score >= 0.10)
                    ? I18n::tr("YUKARI", "BULL", "AUF")
                    : ((fc.quant_score <= -0.10) ? I18n::tr("AŞAĞI", "BEAR", "AB") : I18n::tr("DENGELİ", "FLAT", "NEUT"));
                srec.actual_price_at_horizon = p_actual;
                srec.actual_return_pct = actual_ret * 100.0;
                srec.is_direction_hit = dir_hit;
                srec.is_in_band = in_band;
                rep.recent_samples.push_back(srec);
            }
        }

        // Close any still-open trade at the end of the evaluation window (with exit fee)
        if (in_position) {
            capital *= (1.0 - kOneWayFee);
            if (capital > entry_capital) win_trades++;
            in_position = false;
        }

        rep.total_predictions = total_preds;
        rep.correct_direction_hits = hit_count;
        rep.directional_accuracy_pct = (total_preds > 0) ? (100.0 * hit_count / total_preds) : 0.0;
        rep.always_up_accuracy_pct = (total_preds > 0) ? (100.0 * up_days_count / total_preds) : 0.0;

        rep.active_predictions = active_preds;
        rep.active_direction_hits = active_hits;
        rep.active_directional_accuracy_pct = (active_preds > 0)
            ? (100.0 * active_hits / active_preds)
            : rep.directional_accuracy_pct;

        rep.non_overlapping_blocks = non_overlap_blocks;
        rep.non_overlapping_hits = non_overlap_hits;
        rep.non_overlapping_accuracy_pct = (non_overlap_blocks > 0)
            ? (100.0 * non_overlap_hits / non_overlap_blocks)
            : 0.0;
        if (non_overlap_blocks >= 4) {
            double p_hat = static_cast<double>(non_overlap_hits) / static_cast<double>(non_overlap_blocks);
            rep.binomial_z_stat = (p_hat - 0.50) / std::sqrt(0.25 / static_cast<double>(non_overlap_blocks));
        }

        rep.in_band_hits = in_band_count;
        rep.in_band_coverage_pct = (total_preds > 0) ? (100.0 * in_band_count / total_preds) : 0.0;
        rep.mean_absolute_error_pct = (total_preds > 0) ? (total_abs_error / total_preds) : 0.0;
        rep.random_walk_mape_pct = (total_preds > 0) ? (total_rw_abs_error / total_preds) : 0.0;

        rep.strategy_return_pct = ((capital - 100000.0) / 100000.0) * 100.0;
        rep.strategy_max_drawdown_pct = max_dd * 100.0;
        rep.total_trades = trades;
        rep.win_trades = win_trades;
        rep.trade_win_rate_pct = (trades > 0) ? (100.0 * win_trades / trades) : 0.0;

        if (daily_strat_rets.size() > 2) {
            double mean_r = std::accumulate(daily_strat_rets.begin(), daily_strat_rets.end(), 0.0) / daily_strat_rets.size();
            double sq_r = 0.0;
            for (double r : daily_strat_rets) sq_r += (r - mean_r) * (r - mean_r);
            double std_r = std::sqrt(sq_r / (daily_strat_rets.size() - 1));
            rep.strategy_sharpe = (std_r > 1e-8) ? ((mean_r / std_r) * std::sqrt(252.0)) : 0.0;
        }

        return rep;
    }

    static void render_backtest_report(const ForecastBacktestReport& rep) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::string hdr = I18n::tr(
            std::format("║  📊 {} WALK-FORWARD DOĞRULAMA & BACKTEST RAPORU ({})", rep.symbol, rep.range_used),
            std::format("║  📊 {} WALK-FORWARD VALIDATION & BACKTEST REPORT ({})", rep.symbol, rep.range_used),
            std::format("║  📊 {} WALK-FORWARD-VALIDIERUNGS- & BACKTEST-BERICHT ({})", rep.symbol, rep.range_used)
        );
        int pad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << hdr << std::string(static_cast<size_t>(pad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        std::cout << std::format("   {:<28}: {}{}{} {}\n",
                                 I18n::tr("Test Edilen Varlık", "Tested Asset", "Getestetes Asset"),
                                 pal.glow_rgb, rep.symbol, pal.reset_code,
                                 rep.is_live_data ? (pal.green_rgb + "[CANLI TARİHSEL SERİ]" + pal.reset_code)
                                                  : (pal.red_rgb + "[ÇEVRİMDIŞI SERİ]" + pal.reset_code));
        std::cout << std::format("   {:<28}: {}{} İş Günü{} (Birebir Canlı QuantForecastEngine Çekirdeği)\n",
                                 I18n::tr("Tahmin Ufku / Vade", "Forecast Horizon", "Prognosehorizont"),
                                 pal.glow_rgb, rep.horizon_days, pal.reset_code);
        std::cout << std::format("   {:<28}: {} Kayan Pencere  │  {} Bağımsız (Örtüşmeyen) Blok\n\n",
                                 I18n::tr("Örneklem Büyüklüğü", "Sample Size", "Stichprobenumfang"),
                                 rep.total_predictions, rep.non_overlapping_blocks);

        std::cout << pal.glow_rgb
                  << I18n::tr("   1. İSTATİSTİKSEL İSABET & KALİBRASYON METRİKLERİ (NÖTR ŞİŞİRME YOK):\n",
                              "   1. STATISTICAL ACCURACY & CALIBRATION METRICS (NO NEUTRAL INFLATION):\n",
                              "   1. STATISTISCHE TREFFERQUOTE & KALIBRIERUNG:\n")
                  << pal.reset_code;

        std::string hit_color = (rep.directional_accuracy_pct >= 51.0) ? pal.green_rgb : pal.base_rgb;
        std::cout << std::format("   • Genel Yön İsabeti (Hit Rate)  : {}{:.1f}%{} ({}/{})  │  Baz Oran (Hep-Yukarı): %{:.1f}\n",
                                 hit_color, rep.directional_accuracy_pct, pal.reset_code,
                                 rep.correct_direction_hits, rep.total_predictions,
                                 rep.always_up_accuracy_pct);
        std::cout << std::format("   • Aktif Sinyal İsabeti (|S|≥0.1): %{:.1f} ({}/{})  │  Örtüşmeyen Blok: %{:.1f} (z={:+.2f})\n",
                                 rep.active_directional_accuracy_pct, rep.active_direction_hits, rep.active_predictions,
                                 rep.non_overlapping_accuracy_pct, rep.binomial_z_stat);

        bool band_well_calibrated = (rep.in_band_coverage_pct >= 60.0 && rep.in_band_coverage_pct <= 76.0);
        std::cout << std::format("   • Olasılık Bandı Kapsama (±1σ)  : {}{:.1f}%{} (Teorik Gauss Hedefi: %68.27)\n",
                                 band_well_calibrated ? pal.green_rgb : pal.glow_rgb,
                                 rep.in_band_coverage_pct, pal.reset_code);
        std::cout << std::format("   • Hedef Fiyat Hatası (MAPE)     : %{:.2f}  (Rastgele Yürüyüş / No-Change MAPE: %{:.2f})\n\n",
                                 rep.mean_absolute_error_pct, rep.random_walk_mape_pct);

        std::cout << pal.glow_rgb
                  << std::format("   2. NET SİNYAL STRATEJİSİ ({:.0f} bps Komisyon/Kayma Dahil) vs AL VE TUT:\n", rep.transaction_cost_bps)
                  << pal.reset_code;
        std::string strat_color = (rep.strategy_return_pct >= rep.buy_and_hold_return_pct) ? pal.green_rgb : pal.base_rgb;
        std::cout << std::format("   • Net Model Strateji Getirisi   : {}{:+6.2f}%{}\n", strat_color, rep.strategy_return_pct, pal.reset_code);
        std::cout << std::format("   • Al ve Tut (Buy & Hold)        : {:+6.2f}%\n", rep.buy_and_hold_return_pct);
        std::cout << std::format("   • Yıllıklandırılmış Sharpe      : {:.2f}\n", rep.strategy_sharpe);
        std::cout << std::format("   • Maksimum Gerileme (Max DD)    : %{:.1f}  (Buy & Hold Max DD: %{:.1f})\n",
                                 rep.strategy_max_drawdown_pct, rep.buy_and_hold_max_drawdown_pct);
        std::cout << std::format("   • Kapanmış İşlem Başarısı       : %{:.1f}  ({}/{} Net Kârlı Pozisyon)\n",
                                 rep.trade_win_rate_pct, rep.win_trades, rep.total_trades);

        std::cout << pal.glow_rgb;
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << "║ SON 5 TEST PENCERESİNİN GERÇEKLEŞEN SOMUT KARŞILAŞTIRMASI                     ║\n";
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << "║ BAR   SPOT       YÖN       HEDEF     GERÇEK    DEĞİŞİM  YÖN SONUÇ  ±1σ BANDI ║\n";
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << pal.reset_code;

        for (const auto& s : rep.recent_samples) {
            std::string hit_badge = s.is_direction_hit
                ? (pal.green_rgb + "✔ İSABET " + pal.reset_code)
                : (pal.dim_rgb   + "✘ SAPMA  " + pal.reset_code);
            std::string band_badge = s.is_in_band
                ? (pal.green_rgb + "✔ BANTTA " + pal.reset_code)
                : (pal.dim_rgb   + "✘ DIŞINDA" + pal.reset_code);
            std::string dir_col = ThemeEngine::pad_right(s.predicted_direction, 9);

            std::cout << std::format("  t-{:<3} {:<10.2f} {} {:<9.2f} {:<9.2f} {:+5.1f}%   {} {}\n",
                                     s.bar_index, s.spot_at_forecast,
                                     dir_col, s.predicted_target,
                                     s.actual_price_at_horizon, s.actual_return_pct,
                                     hit_badge, band_badge);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }
};

} // namespace quant
