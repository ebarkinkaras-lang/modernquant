#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
#include "fundamentals.hpp"
#include "i18n.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <format>
#include <iostream>
#include <algorithm>

namespace quant {

enum class ForecastDirection {
    StrongBullish,  // 🚀 GÜÇLÜ YUKARI
    Bullish,        // 📈 YUKARI
    Neutral,        // ⚖️ NÖTR / KONSOLİDASYON
    Bearish,        // 📉 AŞAĞI
    StrongBearish   // 🔻 GÜÇLÜ AŞAĞI
};

struct ForecastResult {
    std::string symbol;
    std::string currency;
    int days{30};
    int trading_days{21};
    std::string horizon_name;
    double spot_price{0.0};
    double target_price{0.0};    // Medyan Log-Normal Hedef Fiyat
    double bull_target{0.0};     // +1 Sigma Kalibre Boğa Bandı
    double bear_support{0.0};    // -1 Sigma Kalibre Ayı Desteği
    double expected_return_pct{0.0};
    double bull_return_pct{0.0};
    double bear_return_pct{0.0};
    ForecastDirection direction{ForecastDirection::Neutral};
    std::string direction_label;
    double quant_score{0.0};     // -1.0 ile +1.0 arası tutarlı bilgi oranı skoru
    double tech_signal{0.0};
    double regime_confidence{1.0};   // Silahian Ch.4 nedensel rejim/sinyal-gürültü güven katsayısı
    double downside_tail_mult{1.0};  // Silahian Ch.7 Heston/Jump asimetrik kuyruk çarpanı (-1σ)
    double var_95_pct{0.0};          // Silahian Ch.4 vade içi %95 VaR düşüş riski (%)
    double cvar_95_pct{0.0};         // Silahian Ch.4 Expected Shortfall (CVaR %95) (%)
    std::string rationale;
};

struct MultiHorizonForecast {
    std::string symbol;
    std::string company_name;
    std::string currency;
    double spot_price{0.0};
    double vwap{0.0};
    double annualized_volatility{0.0};
    double atr_14{0.0};
    double sentiment_score{0.0};
    double sticky_sentiment_score{0.0};
    double sent_strength{0.0};
    double sent_dispersion{0.0};
    bool is_event_shock{false};
    std::string sentiment_label;
    bool is_live{true};
    bool has_live_news{false};

    // Optional Fundamental / Commodity Macro Additional Card
    bool has_fundamentals_card{false};
    FundamentalProfile fundamentals;
    bool has_yield_curve{false};
    TreasuryYieldCurveData yield_curve;
    RealTimeAssetData asset_snapshot;

    std::vector<ForecastResult> horizons; // 5, 21, 63 İş Günü (1H, 1A, 3A)
};

struct CausalFeatureSnapshot {
    double rev5{0.0};
    double rev10{0.0};
    double mom20{0.0};
    double mom60{0.0};
    double rsi_rev{0.0};
    double er20{0.0};
    double flow10{0.0};
    double clv3{0.0};
    double vsp{0.0};
    double sig_d{0.0};
    double composite{0.0};
    bool valid{false};
};

class QuantForecastEngine {
public:
    static int calendar_to_trading_days(int calendar_days) {
        if (calendar_days <= 1) return 1;
        if (calendar_days == 5) return 5; // Already 5 trading days (1 trading week)
        if (calendar_days == 7) return 5;
        if (calendar_days == 21) return 21;
        if (calendar_days == 30) return 21;
        if (calendar_days == 63) return 63;
        if (calendar_days == 90) return 63;
        return std::max(1, static_cast<int>(std::llround(static_cast<double>(calendar_days) * 252.0 / 365.25)));
    }

    // Core unified mathematical forecast engine on 252-trading-day convention (shared 1:1 with Backtest)
    static ForecastResult forecast_for_trading_days(const RealTimeAssetData& data,
                                                    int trading_days,
                                                    double sentiment_score = 0.0,
                                                    bool has_live_news = false,
                                                    const FundamentalProfile* fund = nullptr,
                                                    const std::string& horizon_label = "",
                                                    int display_calendar_days = 0,
                                                    const TreasuryYieldCurveData* yc = nullptr,
                                                    const AssetSentimentProfile* sent_prof = nullptr) {
        ForecastResult res;
        res.symbol = data.symbol;
        res.trading_days = std::max(1, trading_days);
        res.days = (display_calendar_days > 0)
            ? display_calendar_days
            : std::max(1, static_cast<int>(std::llround(res.trading_days * 365.25 / 252.0)));
        res.spot_price = (data.current_price > 0.0) ? data.current_price : 100.0;

        // Case-insensitive currency resolution
        std::string ccy_code = data.currency_code;
        std::string inferred = LiveMarketDataClient::infer_currency_code(data.symbol);
        if (ccy_code.empty() || (ccy_code == "USD" && inferred != "USD")) {
            ccy_code = inferred;
        }
        res.currency = (!data.currency_display.empty() && data.currency_code == ccy_code)
            ? data.currency_display
            : LiveMarketDataClient::currency_display_for(ccy_code, data.symbol);

        if (!horizon_label.empty()) {
            res.horizon_name = horizon_label;
        } else {
            if (res.trading_days <= 6) {
                res.horizon_name = I18n::tr(std::format("{} İş Günü (1H)", res.trading_days),
                                            std::format("{} Trd Days (1W)", res.trading_days),
                                            std::format("{} Handelstage (1W)", res.trading_days));
            } else if (res.trading_days <= 25) {
                res.horizon_name = I18n::tr(std::format("{} İş Günü (1A)", res.trading_days),
                                            std::format("{} Trd Days (1M)", res.trading_days),
                                            std::format("{} Handelstage (1M)", res.trading_days));
            } else if (res.trading_days <= 70) {
                res.horizon_name = I18n::tr(std::format("{} İş Günü (3A)", res.trading_days),
                                            std::format("{} Trd Days (3M)", res.trading_days),
                                            std::format("{} Handelstage (3M)", res.trading_days));
            } else {
                res.horizon_name = I18n::tr(std::format("{} İş Günü (Uzun)", res.trading_days),
                                            std::format("{} Trd Days (Long)", res.trading_days),
                                            std::format("{} Handelstage (Lang)", res.trading_days));
            }
        }

        // 252-Trading-Day Time Fraction & Regime Volatility
        const double td = static_cast<double>(res.trading_days);
        const double T = td / 252.0;
        const double sigma = std::clamp(data.annualized_volatility, 0.05, 1.60);
        const double vol_horizon = sigma * std::sqrt(T);

        // 1. Live Macro Risk-Free Carry Overlay (applies USD Treasury curve term structure cleanly without distorting local-currency momentum)
        double carry_mu = 0.0;
        if (yc != nullptr && yc->is_live && ccy_code == "USD") {
            const double rf_annual = (td <= 65.0)
                ? yc->three_month_yield
                : ((td <= 130.0) ? 0.5 * (yc->three_month_yield + yc->five_year_yield) : yc->ten_year_yield);
            if (rf_annual > 0.0) {
                carry_mu = std::clamp(0.45 * rf_annual * T, -0.05, 0.12);
            }
        }

        // 2. Causal Empirical Bayes Price/Volume Technical Alpha (horizon-adaptive)
        const bool is_crypto = (data.instrument_type == "CRYPTOCURRENCY")
            || (data.symbol.find("-USD") != std::string::npos)
            || (data.symbol.find("-EUR") != std::string::npos);
        const bool is_fx = (data.instrument_type == "CURRENCY")
            || (data.symbol.find("=X") != std::string::npos);
        double w_tech = 1.0;
        double tech_alpha_z = compute_technical_pillar_(data, res.spot_price, res.trading_days, is_crypto, is_fx, w_tech);
        res.tech_signal = std::clamp(std::tanh(2.2 * tech_alpha_z), -1.0, 1.0);

        // 3. Live News & Sticky Event-Shock Sentiment Pillar (decays cleanly over medium horizons)
        const bool news_active = has_live_news || (std::abs(sentiment_score) > 1e-6);
        const double eff_sent = (sent_prof != nullptr && sent_prof->is_event_shock &&
                                 std::abs(sent_prof->sticky_sentiment_score) > std::abs(sentiment_score))
            ? (0.35 * sentiment_score + 0.65 * sent_prof->sticky_sentiment_score)
            : sentiment_score;
        const double sent_vol_boost = (sent_prof != nullptr && sent_prof->sent_count_log > 0.0)
            ? std::clamp(1.0 + 0.15 * std::max(0.0, sent_prof->sent_count_log - 1.0), 1.0, 1.35)
            : 1.0;
        const double sent_disp_damp = (sent_prof != nullptr)
            ? std::clamp(1.0 - 0.30 * sent_prof->sent_dispersion, 0.70, 1.0)
            : 1.0;
        const bool neg_shock = (sent_prof != nullptr && sent_prof->is_event_shock && sent_prof->sticky_sentiment_score <= -0.35)
                            || (eff_sent <= -0.55);
        const double w_news = news_active
            ? ((neg_shock ? 0.55 : 0.35) * sent_vol_boost * sent_disp_damp * std::exp(-td / 45.0))
            : 0.0;
        const double news_z = news_active ? (w_news * std::clamp(eff_sent, -1.0, 1.0)) : 0.0;

        // 4. Fundamental Quality & FAVÖK Valuation Pillar (ramps smoothly at medium/quarterly horizons)
        const bool fund_active = (fund != nullptr && fund->is_available);
        double fund_scale = 0.0;
        if (fund_active) {
            fund_scale = (res.trading_days <= 7) ? 0.08 : ((res.trading_days <= 25) ? 0.20 : 0.35);
        }
        const double fund_z = fund_active ? (fund_scale * std::clamp(fund->fundamental_score, -1.0, 1.0)) : 0.0;

        const double total_z = (w_tech * tech_alpha_z) + news_z + fund_z;

        // 5. Silahian Ch.4 Causal Regime Efficiency + MSE-Shrunk Expected Horizon Log-Return
        double regime_conf = 1.0;
        double skew_tilt = 0.0;
        if (data.historical_prices.size() >= 25) {
            const auto& px = data.historical_prices;
            const size_t n = px.size();
            const size_t lb = std::min<size_t>(60, n - 1);
            double sum_pos_sq = 0.0, sum_neg_sq = 0.0, sum_abs = 0.0, net_lr = 0.0;
            size_t pos_n = 0, neg_n = 0, crash_jumps = 0, cnt = 0;
            const double daily_sig = std::max(0.005, sigma / std::sqrt(252.0));
            for (size_t i = n - lb; i < n; ++i) {
                if (px[i - 1] > 0.0 && px[i] > 0.0) {
                    const double lr = std::log(px[i] / px[i - 1]);
                    if (i >= n - 20) {
                        net_lr += lr;
                        sum_abs += std::abs(lr);
                    }
                    if (lr >= 0.0) { sum_pos_sq += lr * lr; ++pos_n; }
                    else { sum_neg_sq += lr * lr; ++neg_n; }
                    if (lr < -2.35 * daily_sig) ++crash_jumps;
                    ++cnt;
                }
            }
            const double er = (sum_abs > 1e-10) ? std::clamp(std::abs(net_lr) / sum_abs, 0.0, 1.0) : 0.25;
            regime_conf = std::clamp(0.65 + 0.35 * std::sqrt(er), 0.65, 1.0);
            if (cnt >= 15 && pos_n > 0 && neg_n > 0) {
                const double pos_rms = std::sqrt(sum_pos_sq / static_cast<double>(pos_n));
                const double neg_rms = std::sqrt(sum_neg_sq / static_cast<double>(neg_n));
                const double tot_rms = std::max(1e-6, 0.5 * (pos_rms + neg_rms));
                const double ds_skew = (neg_rms - pos_rms) / tot_rms;
                const double jump_freq = static_cast<double>(crash_jumps) / static_cast<double>(cnt);
                skew_tilt = std::clamp(0.32 * ds_skew + 0.55 * jump_freq, -0.04, 0.16);
            }
        }
        res.regime_confidence = regime_conf;
        res.downside_tail_mult = 1.0 + skew_tilt;

        const double chop_damp = (regime_conf < 0.72) ? 0.90 : 1.0;
        double mu_horizon = std::clamp(0.42 * chop_damp * total_z, -0.50, 0.60) * vol_horizon + carry_mu;
        mu_horizon = std::clamp(mu_horizon, -1.65 * vol_horizon, carry_mu + 1.65 * vol_horizon);

        // 6. Silahian Ch.7 Asymmetric Heston / Jump-Tail ±1σ Cone & Cornish-Fisher VaR/CVaR (95%)
        constexpr double kBandScale = 1.045;
        const double bull_scale = kBandScale * std::clamp(1.0 - 0.35 * skew_tilt, 0.90, 1.03);
        const double bear_scale = kBandScale * std::clamp(1.0 + skew_tilt, 0.96, 1.18);
        res.target_price = res.spot_price * std::exp(mu_horizon);
        res.bull_target  = res.spot_price * std::exp(mu_horizon + bull_scale * vol_horizon);
        res.bear_support = res.spot_price * std::exp(mu_horizon - bear_scale * vol_horizon);

        const double z_cf95 = std::clamp(1.64485 + 1.45 * std::max(0.0, skew_tilt), 1.64485, 2.15);
        const double tail_vol = vol_horizon * res.downside_tail_mult;
        res.var_95_pct  = std::max(0.5, (1.0 - std::exp(std::min(0.0, mu_horizon) - z_cf95 * tail_vol)) * 100.0);
        res.cvar_95_pct = std::max(res.var_95_pct * 1.18,
                                   (1.0 - std::exp(std::min(0.0, mu_horizon) - (z_cf95 + 0.42) * tail_vol)) * 100.0);

        res.expected_return_pct = ((res.target_price / res.spot_price) - 1.0) * 100.0;
        res.bull_return_pct     = ((res.bull_target / res.spot_price) - 1.0) * 100.0;
        res.bear_return_pct     = ((res.bear_support / res.spot_price) - 1.0) * 100.0;

        // 7. Coherent Horizon Information Ratio Score (strictly sign-consistent with expected_return_pct)
        double ir_horizon = (vol_horizon > 1e-9) ? (mu_horizon / vol_horizon) : 0.0;
        res.quant_score = std::clamp(std::tanh(5.2 * ir_horizon), -1.0, 1.0);

        // 8. 5-Tier Institutional Direction Classification & Rationale
        std::string fund_clause;
        if (fund_active && res.trading_days >= 20) {
            if (fund->has_ebitda && std::isfinite(fund->ev_to_ebitda)) {
                fund_clause = I18n::tr(
                    std::format(" [Temel Not: {}, FD/FAVÖK: {:.1f}x]", fund->quality_grade, fund->ev_to_ebitda),
                    std::format(" [Fund. Grade: {}, EV/EBITDA: {:.1f}x]", fund->quality_grade, fund->ev_to_ebitda),
                    std::format(" [Fund.-Note: {}, EV/EBITDA: {:.1f}x]", fund->quality_grade, fund->ev_to_ebitda)
                );
            } else {
                fund_clause = std::format(" [Not: {}]", fund->quality_grade);
            }
        }

        if (res.quant_score >= 0.35) {
            res.direction = ForecastDirection::StrongBullish;
            res.direction_label = I18n::tr("🚀 GÜÇLÜ YUKARI (Strong Bullish)", "🚀 STRONG BULLISH", "🚀 STARK BULLISCH");
            res.rationale = I18n::tr(
                std::format("Güçlü trend/akım ve pozitif faktör bileşimi {:+.1f}% prim potansiyeline işaret ediyor{}.", res.expected_return_pct, fund_clause),
                std::format("Strong trend/flow and positive factor composite project {:+.1f}% upside{}.", res.expected_return_pct, fund_clause),
                std::format("Starker Trend und positives Faktorenprofil deuten auf {:+.1f}% Potenzial hin{}.", res.expected_return_pct, fund_clause)
            );
        } else if (res.quant_score >= 0.10) {
            res.direction = ForecastDirection::Bullish;
            res.direction_label = I18n::tr("📈 YUKARI (Bullish)", "📈 BULLISH", "📈 BULLISCH");
            res.rationale = I18n::tr(
                std::format("Kademeli pozitif getiri beklentisi ({:+.1f}%); {:.2f} {} VWAP dengesi izleniyor{}.", res.expected_return_pct, data.vwap > 0 ? data.vwap : res.spot_price, res.currency, fund_clause),
                std::format("Constructive return outlook ({:+.1f}%); tracking {:.2f} {} VWAP balance{}.", res.expected_return_pct, data.vwap > 0 ? data.vwap : res.spot_price, res.currency, fund_clause),
                std::format("Positiver Renditeausblick ({:+.1f}%); {:.2f} {} VWAP im Fokus{}.", res.expected_return_pct, data.vwap > 0 ? data.vwap : res.spot_price, res.currency, fund_clause)
            );
        } else if (res.quant_score > -0.10) {
            res.direction = ForecastDirection::Neutral;
            res.direction_label = I18n::tr("⚖️ NÖTR / KONSOLİDASYON (Neutral)", "⚖️ NEUTRAL / CONSOLIDATION", "⚖️ NEUTRAL / KONSOLIDIERUNG");
            res.rationale = I18n::tr(
                std::format("Fiyat-hacim ve haber dengesi yatay bantta ({:+.1f}%); [{:.2f} – {:.2f}] ±1σ aralığında konsolidasyon{}.", res.expected_return_pct, res.bear_support, res.bull_target, fund_clause),
                std::format("Balanced price-volume & news regime ({:+.1f}%); consolidation within [{:.2f} – {:.2f}] ±1σ band{}.", res.expected_return_pct, res.bear_support, res.bull_target, fund_clause),
                std::format("Ausgewogenes Preis-Volumen-Regime ({:+.1f}%); Konsolidierung im [{:.2f} – {:.2f}] ±1σ-Band{}.", res.expected_return_pct, res.bear_support, res.bull_target, fund_clause)
            );
        } else if (res.quant_score > -0.35) {
            res.direction = ForecastDirection::Bearish;
            res.direction_label = I18n::tr("📉 AŞAĞI (Bearish)", "📉 BEARISH", "📉 BÄRISCH");
            res.rationale = I18n::tr(
                std::format("Satış baskısı ve zayıf momentum ({:+.1f}% beklenti); {:.2f} {} (-1σ) destek bölgesi izlenmelidir{}.", res.expected_return_pct, res.bear_support, res.currency, fund_clause),
                std::format("Selling pressure and weak momentum ({:+.1f}% outlook); monitor -1σ support at {:.2f} {}{}.", res.expected_return_pct, res.bear_support, res.currency, fund_clause),
                std::format("Verkaufsdruck und schwaches Momentum ({:+.1f}%); -1σ-Unterstützung bei {:.2f} {} beachten{}.", res.expected_return_pct, res.bear_support, res.currency, fund_clause)
            );
        } else {
            res.direction = ForecastDirection::StrongBearish;
            res.direction_label = I18n::tr("🔻 GÜÇLÜ AŞAĞI (Strong Bearish)", "🔻 STRONG BEARISH", "🔻 STARK BÄRISCH");
            res.rationale = I18n::tr(
                std::format("Ağır negatif şok / dağıtım rejimi ({:+.1f}% risk); dip teyidi görülmeden acele edilmemelidir{}.", res.expected_return_pct, fund_clause),
                std::format("Severe negative shock / distribution ({:+.1f}% risk); await confirmed base formation{}.", res.expected_return_pct, fund_clause),
                std::format("Starker Abwärtsdruck ({:+.1f}% Risiko); bestätigte Bodenbildung abwarten{}.", res.expected_return_pct, fund_clause)
            );
        }

        return res;
    }

    // Backward-compatible calendar/custom-day entrypoint converting cleanly to 252-trading-day convention
    static ForecastResult forecast_for_days(const RealTimeAssetData& data,
                                            double sentiment_score,
                                            int days,
                                            const std::string& horizon_label = "",
                                            bool has_live_news = false,
                                            const FundamentalProfile* fund = nullptr,
                                            const TreasuryYieldCurveData* yc = nullptr,
                                            const AssetSentimentProfile* sent_prof = nullptr) {
        int td = calendar_to_trading_days(days);
        std::string lbl = horizon_label;
        if (lbl.empty()) {
            lbl = I18n::tr(std::format("{} Gün ({} İşG)", days, td), std::format("{}d ({} TrdD)", days, td), std::format("{}T ({} HT)", days, td));
        }
        return forecast_for_trading_days(data, td, sentiment_score, has_live_news, fund, lbl, days, yc, sent_prof);
    }

    static MultiHorizonForecast generate_multi_horizon(const RealTimeAssetData& data,
                                                       double sentiment_score,
                                                       const std::string& sentiment_label,
                                                       bool has_live_news = false,
                                                       const FundamentalProfile* fund = nullptr,
                                                       const TreasuryYieldCurveData* yc = nullptr,
                                                       const AssetSentimentProfile* sent_prof = nullptr) {
        MultiHorizonForecast multi;
        multi.symbol = data.symbol;
        multi.company_name = data.long_name.empty() ? data.symbol : data.long_name;

        std::string ccy_code = data.currency_code;
        std::string inferred = LiveMarketDataClient::infer_currency_code(data.symbol);
        if (ccy_code.empty() || (ccy_code == "USD" && inferred != "USD")) ccy_code = inferred;
        multi.currency = (!data.currency_display.empty() && data.currency_code == ccy_code)
            ? data.currency_display
            : LiveMarketDataClient::currency_display_for(ccy_code, data.symbol);

        multi.spot_price = data.current_price;
        multi.vwap = data.vwap;
        multi.annualized_volatility = data.annualized_volatility;
        multi.atr_14 = data.atr_14;
        multi.sentiment_score = sentiment_score;
        if (sent_prof != nullptr) {
            multi.sticky_sentiment_score = sent_prof->sticky_sentiment_score;
            multi.sent_strength = sent_prof->sent_strength;
            multi.sent_dispersion = sent_prof->sent_dispersion;
            multi.is_event_shock = sent_prof->is_event_shock;
        } else {
            multi.sticky_sentiment_score = sentiment_score;
        }
        multi.sentiment_label = sentiment_label;
        multi.is_live = data.is_live;
        multi.has_live_news = has_live_news;
        multi.asset_snapshot = data;

        if (fund != nullptr) {
            multi.has_fundamentals_card = true;
            multi.fundamentals = *fund;
            if (!fund->company_name.empty()) multi.company_name = fund->company_name;
        }
        if (yc != nullptr) {
            multi.has_yield_curve = true;
            multi.yield_curve = *yc;
        }

        // Standard Institutional Horizons: 5 Trading Days (1W), 21 Trading Days (1M), 63 Trading Days (3M)
        multi.horizons.push_back(forecast_for_trading_days(
            data, 5, sentiment_score, has_live_news, fund,
            I18n::tr("1 Hafta (5 İşG)", "1 Week (5 TrdD)", "1 Woche (5 HT)"), 7, yc, sent_prof));
        multi.horizons.push_back(forecast_for_trading_days(
            data, 21, sentiment_score, has_live_news, fund,
            I18n::tr("1 Ay (21 İşG)", "1 Month (21 TrdD)", "1 Monat (21 HT)"), 30, yc, sent_prof));
        multi.horizons.push_back(forecast_for_trading_days(
            data, 63, sentiment_score, has_live_news, fund,
            I18n::tr("3 Ay (63 İşG)", "3 Months (63 TrdD)", "3 Monate (63 HT)"), 90, yc, sent_prof));

        return multi;
    }

    static void render_forecast_matrix(const MultiHorizonForecast& multi) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        if (!multi.is_live) {
            std::cout << "\n" << pal.red_rgb
                      << I18n::tr("  ╔════════════════════════════════════════════════════════════════════════════╗\n"
                                  "  ║ ⚠️ UYARI: CANLI VERİ ALINAMADI — ÇEVRİMDIŞI SİMÜLASYON VERİSİ GÖSTERİLİYOR ║\n"
                                  "  ╚════════════════════════════════════════════════════════════════════════════╝\n",
                                  "  ╔════════════════════════════════════════════════════════════════════════════╗\n"
                                  "  ║ ⚠️ WARNING: LIVE FEED OFFLINE — DISPLAYING SIMULATED FALLBACK DATA         ║\n"
                                  "  ╚════════════════════════════════════════════════════════════════════════════╝\n",
                                  "  ╔════════════════════════════════════════════════════════════════════════════╗\n"
                                  "  ║ ⚠️ WARNUNG: KEINE LIVE-DATEN — OFFLINE-SIMULATIONSDATEN WERDEN ANGEZEIGT   ║\n"
                                  "  ╚════════════════════════════════════════════════════════════════════════════╝\n")
                      << pal.reset_code;
        }

        std::string hdr = I18n::tr(
            std::format("║  🎯 {} HEDEF FİYAT & ZAMAN DİLİMLERİNE GÖRE YÖN TAHMİN MATRİSİ", multi.symbol),
            std::format("║  🎯 {} TARGET PRICE & MULTI-HORIZON DIRECTION FORECAST MATRIX", multi.symbol),
            std::format("║  🎯 {} ZIELKURS & MULTI-HORIZONT-RICHTUNGSPROGNOSE-MATRIX", multi.symbol)
        );
        int pad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << hdr << std::string(static_cast<size_t>(pad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        std::string feed_badge = multi.is_live
            ? (pal.green_rgb + I18n::tr("[CANLI PİYASA VERİSİ]", "[LIVE MARKET FEED]", "[LIVE-MARKTDATEN]") + pal.reset_code)
            : (pal.red_rgb + I18n::tr("[ÇEVRİMDIŞI]", "[OFFLINE]", "[OFFLINE]") + pal.reset_code);

        std::cout << std::format("   {:<26}: {}{:.2f} {}{}  {}\n",
                                 I18n::tr("Anlık Spot Fiyat", "Current Spot Price", "Aktueller Kassakurs"),
                                 pal.glow_rgb, multi.spot_price, multi.currency, pal.reset_code, feed_badge);
        std::cout << std::format("   {:<26}: {:.2f} {}  (Sapma: {:+.2f}%)\n",
                                 I18n::tr("22-İşG Kurumsal VWAP", "22-Day Institutional VWAP", "22-Tage Institutioneller VWAP"),
                                 multi.vwap, multi.currency,
                                 (multi.vwap > 0.0) ? ((multi.spot_price - multi.vwap) / multi.vwap * 100.0) : 0.0);
        if (multi.atr_14 > 0.0) {
            std::cout << std::format("   {:<26}: %{:.2f}  │  14-Gün OHLC ATR: {:.2f} {} (%{:.2f})\n",
                                     I18n::tr("Yıllık Volatilite (EWMA)", "Annualized Vol (EWMA)", "Annualisierte Volatilität"),
                                     multi.annualized_volatility * 100.0,
                                     multi.atr_14, multi.currency,
                                     (multi.spot_price > 0.0) ? (multi.atr_14 / multi.spot_price * 100.0) : 0.0);
        } else {
            std::cout << std::format("   {:<26}: %{:.2f}\n",
                                     I18n::tr("Yıllıklandırılmış Vol.", "Annualized Volatility", "Annualisierte Volatilität"),
                                     multi.annualized_volatility * 100.0);
        }
        std::cout << std::format("   {:<26}: {}{:+.2f}{} ({})\n",
                                 I18n::tr("Canlı Haber Sentimeli", "Live News Sentiment", "Live-News-Sentiment"),
                                 pal.glow_rgb, multi.sentiment_score, pal.reset_code, multi.sentiment_label);
        if (multi.has_live_news) {
            std::string shock_tag = multi.is_event_shock
                ? (pal.red_rgb + I18n::tr("  ⚡ OLAY ŞOKU AKTİF", "  ⚡ EVENT SHOCK ACTIVE", "  ⚡ EREIGNIS-SCHOCK") + pal.reset_code)
                : "";
            std::cout << std::format("   {:<26}: {:+.2f} │ Güç: {:+.2f} │ Dağılım σ: {:.2f}{}\n",
                                     I18n::tr("Yapışkan Hafıza (2-Akım)", "Sticky Memory (2-Stream)", "Sticky-Speicher (2-Strom)"),
                                     multi.sticky_sentiment_score,
                                     multi.sent_strength,
                                     multi.sent_dispersion,
                                     shock_tag);
        }

        if (multi.has_fundamentals_card && !multi.fundamentals.summary_line.empty()) {
            std::cout << std::format("   {:<26}: {}\n",
                                     I18n::tr("Temel / FAVÖK Özeti", "Fundamental / EBITDA", "Fundamental / EBITDA"),
                                     multi.fundamentals.summary_line);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::string col_hdr = I18n::tr(
            "║ VADE DİLİMİ         YÖN TAHMİNİ           HEDEF FİYAT     BOĞA (+1σ)  AYI (-1σ)║",
            "║ TIME HORIZON        DIRECTION FORECAST    TARGET PRICE    BULL (+1σ)  BEAR (-1σ)║",
            "║ ZEITHORIZONT        TREND-PROGNOSE        ZIELKURS        BULL (+1σ)  BÄR (-1σ) ║"
        );
        std::cout << col_hdr << "\n";
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::cout << pal.reset_code;

        for (const auto& h : multi.horizons) {
            std::string dir_short;
            std::string dir_color = pal.base_rgb;
            if (h.direction == ForecastDirection::StrongBullish) {
                dir_short = I18n::tr("🚀 GÜÇLÜ YUKARI", "🚀 STRONG BULLISH", "🚀 STARK BULLISCH");
                dir_color = pal.green_rgb;
            } else if (h.direction == ForecastDirection::Bullish) {
                dir_short = I18n::tr("📈 YUKARI", "📈 BULLISH", "📈 BULLISCH");
                dir_color = pal.glow_rgb;
            } else if (h.direction == ForecastDirection::Neutral) {
                dir_short = I18n::tr("⚖️ NÖTR / YATAY", "⚖️ NEUTRAL / RANGE", "⚖️ NEUTRAL/SEITW");
                dir_color = pal.dim_rgb;
            } else if (h.direction == ForecastDirection::Bearish) {
                dir_short = I18n::tr("📉 AŞAĞI", "📉 BEARISH", "📉 BÄRISCH");
                dir_color = pal.base_rgb;
            } else {
                dir_short = I18n::tr("🔻 GÜÇLÜ AŞAĞI", "🔻 STRONG BEARISH", "🔻 STARK BÄRISCH");
                dir_color = pal.red_rgb;
            }

            std::string col1 = ThemeEngine::pad_right(h.horizon_name, 19);
            std::string col2 = ThemeEngine::pad_right(dir_short, 21);
            std::string target_str = std::format("{:.2f} ({:+5.1f}%)", h.target_price, h.expected_return_pct);
            std::string bull_str   = std::format("{:.2f}", h.bull_target);
            std::string bear_str   = std::format("{:.2f}", h.bear_support);

            std::cout << std::format("  {} {}{}{} {:<16} {:<11} {:<10}\n",
                                     col1, dir_color, col2, pal.reset_code,
                                     target_str, bull_str, bear_str);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::string rat_hdr = I18n::tr(
            "║  🔍 VADELERE GÖRE AYRINTILI QUANT GEREKÇELERİ (252 İŞ GÜNÜ KALİBRASYONLU)",
            "║  🔍 DETAILED QUANT RATIONALE BY HORIZON (252-TRADING-DAY CALIBRATED)",
            "║  🔍 DETAILLIERTE QUANT-BEGRÜNDUNG NACH ZEITHORIZONT (252-HANDELSTAGE)"
        );
        int rpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(rat_hdr));
        std::cout << rat_hdr << std::string(static_cast<size_t>(rpad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        for (const auto& h : multi.horizons) {
            std::cout << std::format("   • {}{}{}: {}\n", pal.glow_rgb, h.horizon_name, pal.reset_code, h.rationale);
        }
        if (!multi.horizons.empty()) {
            const auto& h_ref = multi.horizons.front();
            std::cout << std::format("   • {}{}{}: Conf %{:.0f} │ Kuyruk Çarpanı {:.2f}x │ {}G %95 VaR: -%{:.2f} │ CVaR: -%{:.2f}\n",
                                     pal.glow_rgb,
                                     I18n::tr("Silahian Ch.4/7 Risk & Asimetri", "Silahian Ch.4/7 Tail Risk", "Silahian Ch.4/7 Tail-Risiko"),
                                     pal.reset_code,
                                     h_ref.regime_confidence * 100.0,
                                     h_ref.downside_tail_mult,
                                     h_ref.trading_days,
                                     h_ref.var_95_pct,
                                     h_ref.cvar_95_pct);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;

        // Render Additional Fundamental / FAVÖK (or Commodity/ETF Macro) Card automatically when attached!
        if (multi.has_fundamentals_card) {
            FundamentalEngine::render_fundamental_card(
                multi.fundamentals,
                multi.asset_snapshot,
                multi.has_yield_curve ? &multi.yield_curve : nullptr
            );
        }
    }

    // Public causal feature inspector used by Look-Ahead Leakage & Causality Auditor
    static CausalFeatureSnapshot inspect_causal_features_at(const RealTimeAssetData& data,
                                                            size_t bar_idx,
                                                            int trading_days = 5) {
        CausalFeatureSnapshot snap;
        const auto& P = data.historical_prices;
        if (P.size() < 25 || bar_idx < 25 || bar_idx >= P.size()) return snap;
        const bool is_crypto = (data.instrument_type == "CRYPTOCURRENCY")
            || (data.symbol.find("-USD") != std::string::npos)
            || (data.symbol.find("-EUR") != std::string::npos);
        const auto fb = extract_causal_features_(P, data.historical_highs, data.historical_lows, data.historical_volumes);
        snap.rev5 = fb.rev5[bar_idx];
        snap.rev10 = fb.rev10[bar_idx];
        snap.mom20 = fb.mom20[bar_idx];
        snap.mom60 = fb.mom60[bar_idx];
        snap.rsi_rev = fb.rsi_rev[bar_idx];
        snap.er20 = fb.er20[bar_idx];
        snap.flow10 = fb.flow10[bar_idx];
        snap.clv3 = fb.clv3[bar_idx];
        snap.vsp = fb.vsp[bar_idx];
        snap.sig_d = fb.sig_d[bar_idx];
        const double td = static_cast<double>(std::max(1, trading_days));
        const double w_short = std::exp(-std::max(0.0, td - 5.0) / 10.0);
        snap.composite = eval_composite_signal_(fb, bar_idx, w_short, is_crypto);
        snap.valid = true;
        return snap;
    }

private:
    struct CausalFeatures_ {
        std::vector<double> sig_d;    // Causal daily EWMA(0.94) volatility
        std::vector<double> rev5;     // Normalized 5-bar short-term reversal
        std::vector<double> rev10;    // Normalized 10-bar reversal
        std::vector<double> rsi_rev;  // Mean-reverting RSI(14) z-transform
        std::vector<double> clv3;     // 3-bar Close Location Value (buying/selling climax)
        std::vector<double> flow10;   // 10-bar volume-weighted accumulation/climax flow
        std::vector<double> mom20;    // Vol-normalized 20-bar trend
        std::vector<double> mom60;    // Vol-normalized 60-bar trend
        std::vector<double> vsp;      // Log short-to-long volatility ratio log(RV5/RV40)
        std::vector<double> er20;     // 20-bar Kaufman efficiency ratio |net| / sum(|lr|)
    };

    /// Extracts strictly causal per-bar technical indicators and volatility without future leakage.
    static CausalFeatures_ extract_causal_features_(const std::vector<double>& P,
                                                    const std::vector<double>& H,
                                                    const std::vector<double>& L,
                                                    const std::vector<double>& V) {
        const size_t n = P.size();
        CausalFeatures_ f;
        f.sig_d.assign(n, 0.20 / std::sqrt(252.0));
        f.rev5.assign(n, 0.0);
        f.rev10.assign(n, 0.0);
        f.rsi_rev.assign(n, 0.0);
        f.clv3.assign(n, 0.0);
        f.flow10.assign(n, 0.0);
        f.mom20.assign(n, 0.0);
        f.mom60.assign(n, 0.0);
        f.vsp.assign(n, 0.0);
        f.er20.assign(n, 0.25);
        if (n < 25) return f;

        std::vector<double> lr1(n, 0.0), rsi(n, 50.0), clv(n, 0.0);
        double ewma_v = 0.00025, ag = 0.0, al = 0.0;
        const bool has_hl = (H.size() == n && L.size() == n);
        const bool has_vol = (V.size() == n);
        for (size_t i = 1; i < n; ++i) {
            if (P[i - 1] > 0.0 && P[i] > 0.0) {
                lr1[i] = std::log(P[i] / P[i - 1]);
                ewma_v = 0.94 * ewma_v + 0.06 * lr1[i] * lr1[i];
            }
            f.sig_d[i] = std::clamp(std::sqrt(ewma_v), 0.003, 0.12);

            const double df = P[i] - P[i - 1];
            const double g = (df > 0.0) ? df : 0.0;
            const double l = (df < 0.0) ? -df : 0.0;
            if (i <= 14) {
                ag += g / 14.0;
                al += l / 14.0;
            } else {
                ag = (ag * 13.0 + g) / 14.0;
                al = (al * 13.0 + l) / 14.0;
            }
            if (i >= 14) {
                rsi[i] = (al < 1e-12) ? 100.0 : (100.0 - 100.0 / (1.0 + ag / al));
            }
            if (has_hl && H[i] > L[i] + 1e-9) {
                clv[i] = ((P[i] - L[i]) - (H[i] - P[i])) / (H[i] - L[i]);
            }
        }

        for (size_t i = 20; i < n; ++i) {
            const double sd = f.sig_d[i];
            double net20 = 0.0, abs20 = 0.0, obv10 = 0.0, vsum10 = 0.0;
            for (size_t j = i - 19; j <= i; ++j) {
                net20 += lr1[j];
                abs20 += std::abs(lr1[j]);
                if (j >= i - 9) {
                    const double vj = (has_vol && V[j] > 0.0) ? V[j] : 1.0;
                    obv10 += clv[j] * vj;
                    vsum10 += vj;
                }
            }
            f.er20[i] = (abs20 > 1e-10) ? std::clamp(std::abs(net20) / abs20, 0.0, 1.0) : 0.25;
            if (P[i] > 0.0 && P[i - 5] > 0.0) {
                f.rev5[i] = std::tanh(-std::log(P[i] / P[i - 5]) / (sd * std::sqrt(5.0)) * 0.75);
            }
            if (P[i] > 0.0 && P[i - 10] > 0.0) {
                f.rev10[i] = std::tanh(-std::log(P[i] / P[i - 10]) / (sd * std::sqrt(10.0)) * 0.75);
            }
            f.rsi_rev[i] = std::tanh(-(rsi[i] - 50.0) / 18.0);
            if (P[i] > 0.0 && P[i - 20] > 0.0) {
                f.mom20[i] = std::tanh(std::log(P[i] / P[i - 20]) / (sd * std::sqrt(20.0)));
            }
            const size_t lb60 = std::min<size_t>(60, i);
            if (P[i] > 0.0 && P[i - lb60] > 0.0) {
                f.mom60[i] = std::tanh(std::log(P[i] / P[i - lb60]) / (sd * std::sqrt(static_cast<double>(lb60))));
            }
            f.flow10[i] = (vsum10 > 0.0) ? std::tanh(2.5 * obv10 / vsum10) : 0.0;
            f.clv3[i] = (clv[i] + clv[i - 1] + clv[i - 2]) / 3.0;

            double rv5 = 0.0, rv40 = 0.0;
            const size_t lb40 = std::min<size_t>(40, i + 1);
            for (size_t j = i - 4; j <= i; ++j) rv5 += lr1[j] * lr1[j] / 5.0;
            for (size_t j = i + 1 - lb40; j <= i; ++j) rv40 += lr1[j] * lr1[j] / static_cast<double>(lb40);
            if (rv5 > 1e-14 && rv40 > 1e-14) {
                f.vsp[i] = std::tanh(std::log(std::sqrt(rv5) / std::sqrt(rv40)) * 1.5);
            }
        }
        return f;
    }

    /// Evaluates the horizon-adaptive orthogonal reversal + climax/flow composite (or crypto momentum) at bar s.
    static double eval_composite_signal_(const CausalFeatures_& fb,
                                         size_t s,
                                         double w_short,
                                         bool is_crypto) {
        if (is_crypto) {
            return 0.45 * fb.mom20[s] + 0.55 * fb.mom60[s];
        }
        double rev = w_short * (0.45 * fb.rev5[s] + 0.25 * fb.rev10[s] + 0.30 * fb.rsi_rev[s])
                   + (1.0 - w_short) * (0.50 * fb.rev10[s] + 0.50 * fb.rsi_rev[s]);
        const double flw = -0.55 * fb.clv3[s] - 0.45 * fb.flow10[s];
        // Falling-knife guard: dampen dip conviction during strong directional waterfall selloffs
        if (rev > 0.30 && fb.mom20[s] < -0.50 && fb.mom60[s] < -0.45 && fb.er20[s] > 0.35) {
            rev *= 0.65;
        }
        return 0.60 * rev + 0.40 * flw;
    }

    struct CausalPosterior_ {
        double post_ic{0.06};
        double w_js{0.0};
        double z_now{0.0};
    };

    /// Computes causal James-Stein / Harvey-Liu Empirical Bayes posterior IC over resolved pairs [25 .. t-h].
    static CausalPosterior_ estimate_causal_posterior_js_(const std::vector<double>& P,
                                                         const CausalFeatures_& fb,
                                                         size_t t,
                                                         int h,
                                                         double w_short,
                                                         bool is_crypto) {
        CausalPosterior_ res;
        const double prior_ic = is_crypto ? 0.05 : 0.06;
        res.post_ic = prior_ic;
        const double x_now = eval_composite_signal_(fb, t, w_short, is_crypto);
        res.z_now = std::clamp(x_now * 2.0, -2.5, 2.5);

        if (t <= 40 + static_cast<size_t>(h)) return res;

        const size_t s_max = t - static_cast<size_t>(h);
        const size_t s_min = (s_max > 280) ? (s_max - 280) : 25;
        const double td = static_cast<double>(std::max(1, h));
        const double decay = std::log(2.0) / 160.0;

        double sw = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, syy = 0.0, sxy = 0.0;
        for (size_t s = s_min; s <= s_max; ++s) {
            if (P[s] <= 0.0 || P[s + static_cast<size_t>(h)] <= 0.0) continue;
            const double vol_hs = std::max(1e-4, fb.sig_d[s] * std::sqrt(td));
            const double y_s = std::clamp(std::log(P[s + static_cast<size_t>(h)] / P[s]) / vol_hs, -2.5, 2.5);
            const double x_s = eval_composite_signal_(fb, s, w_short, is_crypto);
            const double w = std::exp(-static_cast<double>(s_max - s) * decay);
            sw  += w;
            sx  += w * x_s;
            sy  += w * y_s;
            sxx += w * x_s * x_s;
            syy += w * y_s * y_s;
            sxy += w * x_s * y_s;
        }
        if (sw < 12.0) return res;

        const double mx = sx / sw, my = sy / sw;
        const double vx = sxx / sw - mx * mx, vy = syy / sw - my * my, cxy = sxy / sw - mx * my;
        const double emp_ic = (vx > 1e-6 && vy > 1e-6) ? (cxy / std::sqrt(vx * vy)) : 0.0;
        const double n_eff = sw / std::max(1.0, td * 0.55);
        const double t_stat = emp_ic * std::sqrt(std::max(1.0, n_eff - 2.0));
        res.w_js = (t_stat * t_stat) / (t_stat * t_stat + 5.0);
        res.post_ic = std::clamp(res.w_js * emp_ic + (1.0 - res.w_js) * prior_ic, -0.25, 0.45);
        const double sd_x = std::sqrt(std::max(1e-5, vx));
        res.z_now = std::clamp((x_now - 0.25 * mx) / std::max(0.20, sd_x), -2.5, 2.5);
        return res;
    }

    /// Produces horizon-adaptive, strictly causal expected-return factor with asymmetric equity damping & James-Stein shrinkage.
    static double compute_technical_pillar_(const RealTimeAssetData& data,
                                            double spot,
                                            int trading_days,
                                            bool is_crypto,
                                            bool is_fx,
                                            double& out_weight) {
        const auto& P_raw = data.historical_prices;
        if (P_raw.size() < 5) {
            out_weight = 0.25;
            if (data.vwap > 0.0 && spot > 0.0) {
                const double vwap_dev = (spot - data.vwap) / data.vwap;
                const double sig_d = std::max(1e-4, data.annualized_volatility / std::sqrt(252.0));
                const double vwap_z = vwap_dev / (sig_d * std::sqrt(10.0));
                return std::clamp(-0.35 * std::tanh(vwap_z * 0.5), -1.0, 1.0);
            }
            return 0.0;
        }

        // Incorporate live intraday spot without mutating caller snapshot or copying when already aligned
        const bool need_spot_append = (spot > 0.0 && P_raw.back() > 0.0 && std::abs(spot - P_raw.back()) > P_raw.back() * 1e-4);
        std::vector<double> P_buf, H_buf, L_buf, V_buf;
        if (need_spot_append) {
            P_buf = P_raw;
            P_buf.push_back(spot);
            if (data.historical_highs.size() == P_raw.size() && data.historical_lows.size() == P_raw.size()) {
                H_buf = data.historical_highs;
                L_buf = data.historical_lows;
                H_buf.push_back(std::max(H_buf.back(), spot));
                L_buf.push_back(std::min(L_buf.back(), spot));
            }
            if (data.historical_volumes.size() == P_raw.size()) {
                V_buf = data.historical_volumes;
                V_buf.push_back(V_buf.empty() ? 1.0 : V_buf.back());
            }
        }
        const std::vector<double>& P = need_spot_append ? P_buf : P_raw;
        const std::vector<double>& H = need_spot_append ? H_buf : data.historical_highs;
        const std::vector<double>& L = need_spot_append ? L_buf : data.historical_lows;
        const std::vector<double>& V = need_spot_append ? V_buf : data.historical_volumes;
        const size_t n = P.size();
        const size_t t = n - 1;
        out_weight = 1.0;

        // Short histories (< 25 bars): simple momentum fallback
        if (n < 25) {
            const double r_short = (P[0] > 0.0) ? std::log(spot / P[0]) : 0.0;
            return std::clamp(0.15 * std::tanh(r_short * 3.0), -0.35, 0.35);
        }

        const auto fb = extract_causal_features_(P, H, L, V);
        const double td = static_cast<double>(std::max(1, trading_days));
        const double w_short = std::exp(-std::max(0.0, td - 5.0) / 10.0);

        const auto js = estimate_causal_posterior_js_(P, fb, t, trading_days, w_short, is_crypto);
        double sig_term = js.post_ic * js.z_now;

        // Convex equity asymmetry: overbought rallies decelerate toward drift rather than crashing below 50%,
        // while assets with high causal empirical signal t-stat (e.g. deterministic mean-reversion) retain full symmetry.
        if (!is_crypto && sig_term < 0.0) {
            const double asym_eff = 0.40 + 0.60 * js.w_js;
            sig_term *= asym_eff;
        }

        // Conditional volatility-spike hazard guard: penalize neutral/choppy short bars and medium horizons on RV spikes
        double v_pen = 0.0;
        if (!is_crypto && fb.vsp[t] > 0.12) {
            if (std::abs(fb.rev5[t]) < 0.22) {
                v_pen -= 0.12 * (fb.vsp[t] - 0.12);
            }
            v_pen -= 0.15 * (1.0 - w_short) * (fb.vsp[t] - 0.12);
        }

        const double mu_0 = (is_crypto || is_fx) ? 0.0 : (0.070 * std::pow(td / 5.0, 0.35));
        const double amp = (sig_term > 0.0) ? (0.50 * 1.15) : 0.50;
        const double mu_z = mu_0 + amp * sig_term + v_pen;

        // Scale back by 0.42 so forecast_for_trading_days' `0.42 * total_z` yields the calibrated horizon IR `mu_z`
        return mu_z / 0.42;
    }
};

} // namespace quant
