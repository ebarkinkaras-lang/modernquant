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
                                                    int display_calendar_days = 0) {
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
        const double T = static_cast<double>(res.trading_days) / 252.0;
        const double sigma = std::clamp(data.annualized_volatility, 0.05, 1.60);
        const double vol_horizon = sigma * std::sqrt(T);

        // 1. Shrunk Nominal Carry Baseline (smooth, currency-consistent, no arbitrary switches)
        double r_carry = 0.022; // USD shrunk nominal drift (~2.2%/yr)
        if (ccy_code == "TRY") {
            r_carry = 0.175;    // TRY shrunk nominal carry (~17.5%/yr; ~0.35% per 5 trading days)
        } else if (ccy_code == "EUR") {
            r_carry = 0.016;    // EUR shrunk nominal drift (~1.6%/yr)
        }

        // 2. Causal Price/Volume Technical Pillar [-1, +1]
        double w_tech = 0.25;
        double tech_sig = compute_technical_pillar_(data, res.spot_price, sigma, r_carry, res.trading_days, w_tech);
        res.tech_signal = tech_sig;

        // 3. Live News Sentiment Pillar [-1, +1] (only active when real news or explicit test shock exists)
        bool news_active = has_live_news || (std::abs(sentiment_score) > 1e-6);
        double w_news = news_active ? (1.25 * std::exp(-static_cast<double>(res.trading_days) / 45.0)) : 0.0;
        double news_sig = std::clamp(sentiment_score, -1.0, 1.0);

        // 4. Fundamental Quality & FAVÖK Valuation Pillar [-1, +1] (ramps up on medium/quarterly horizons)
        bool fund_active = (fund != nullptr && fund->is_available);
        double w_fund = 0.0;
        double fund_sig = 0.0;
        if (fund_active) {
            fund_sig = std::clamp(fund->fundamental_score, -1.0, 1.0);
            w_fund = (res.trading_days <= 7) ? 0.15 : ((res.trading_days <= 25) ? 0.45 : 0.85);
        }

        double w_sum = w_tech + w_news + w_fund;
        double z_composite = (w_sum > 1e-9)
            ? std::clamp((w_tech * tech_sig + w_news * news_sig + w_fund * fund_sig) / w_sum, -1.0, 1.0)
            : 0.0;

        // 5. Grinold-Kahn Expected Horizon Log-Return: E[ln(S_T/S_0)] = r_carry*T + IC_eff * z * sigma * sqrt(T)
        double ic_eff = 0.30 + 0.16 * std::abs(z_composite);
        double mu_horizon = (r_carry * T) + (ic_eff * z_composite * vol_horizon);
        mu_horizon = std::clamp(mu_horizon, -1.65 * vol_horizon, (r_carry * T) + 1.65 * vol_horizon);

        // 6. Symmetric Log-Space Target & Empirically Calibrated ±1σ Cone (Leptokurtic Student-t factor = 1.045)
        constexpr double kBandScale = 1.045;
        res.target_price = res.spot_price * std::exp(mu_horizon);
        res.bull_target  = res.spot_price * std::exp(mu_horizon + kBandScale * vol_horizon);
        res.bear_support = res.spot_price * std::exp(mu_horizon - kBandScale * vol_horizon);

        res.expected_return_pct = ((res.target_price / res.spot_price) - 1.0) * 100.0;
        res.bull_return_pct     = ((res.bull_target / res.spot_price) - 1.0) * 100.0;
        res.bear_return_pct     = ((res.bear_support / res.spot_price) - 1.0) * 100.0;

        // 7. Coherent Horizon Information Ratio Score (strictly sign-consistent with expected_return_pct)
        double ir_horizon = (vol_horizon > 1e-9) ? (mu_horizon / vol_horizon) : 0.0;
        res.quant_score = std::clamp(std::tanh(1.55 * ir_horizon), -1.0, 1.0);

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
                                            const FundamentalProfile* fund = nullptr) {
        int td = calendar_to_trading_days(days);
        std::string lbl = horizon_label;
        if (lbl.empty()) {
            if (days <= 7) lbl = I18n::tr(std::format("{} Gün ({} İşG)", days, td), std::format("{}d ({} TrdD)", days, td), std::format("{}T ({} HT)", days, td));
            else if (days <= 35) lbl = I18n::tr(std::format("{} Gün ({} İşG)", days, td), std::format("{}d ({} TrdD)", days, td), std::format("{}T ({} HT)", days, td));
            else lbl = I18n::tr(std::format("{} Gün ({} İşG)", days, td), std::format("{}d ({} TrdD)", days, td), std::format("{}T ({} HT)", days, td));
        }
        return forecast_for_trading_days(data, td, sentiment_score, has_live_news, fund, lbl, days);
    }

    static MultiHorizonForecast generate_multi_horizon(const RealTimeAssetData& data,
                                                       double sentiment_score,
                                                       const std::string& sentiment_label,
                                                       bool has_live_news = false,
                                                       const FundamentalProfile* fund = nullptr,
                                                       const TreasuryYieldCurveData* yc = nullptr) {
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
            I18n::tr("1 Hafta (5 İşG)", "1 Week (5 TrdD)", "1 Woche (5 HT)"), 7));
        multi.horizons.push_back(forecast_for_trading_days(
            data, 21, sentiment_score, has_live_news, fund,
            I18n::tr("1 Ay (21 İşG)", "1 Month (21 TrdD)", "1 Monat (21 HT)"), 30));
        multi.horizons.push_back(forecast_for_trading_days(
            data, 63, sentiment_score, has_live_news, fund,
            I18n::tr("3 Ay (63 İşG)", "3 Months (63 TrdD)", "3 Monate (63 HT)"), 90));

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

private:
    static double compute_technical_pillar_(const RealTimeAssetData& data,
                                            double spot,
                                            double sigma,
                                            double r_carry,
                                            int trading_days,
                                            double& out_weight) {
        const auto& P = data.historical_prices;
        const auto& V = data.historical_volumes;
        const double sigma_daily = std::max(1e-4, sigma / std::sqrt(252.0));

        double vwap_z = 0.0;
        if (data.vwap > 0.0 && spot > 0.0) {
            double vwap_dev = (spot - data.vwap) / data.vwap;
            vwap_z = vwap_dev / (sigma_daily * std::sqrt(10.0));
        }

        if (P.size() < 5) {
            // Minimal fallback when only spot & vwap are supplied (e.g. synthetic unit-test structs)
            out_weight = 0.25;
            return std::clamp(-0.35 * std::tanh(vwap_z * 0.5), -1.0, 1.0);
        }

        out_weight = 1.0;
        const size_t n = P.size();

        // 1. Excess Volatility-Scaled Medium-Term Momentum (20d & 60d)
        auto calc_excess_mom_z = [&](size_t lookback) -> double {
            size_t k = std::min(lookback, n - 1);
            if (k == 0 || P[n - 1 - k] <= 0.0 || spot <= 0.0) return 0.0;
            double raw_lr = std::log(spot / P[n - 1 - k]);
            double carry_lr = r_carry * (static_cast<double>(k) / 252.0);
            double excess_lr = raw_lr - carry_lr;
            return excess_lr / (sigma_daily * std::sqrt(static_cast<double>(k)));
        };

        double mom20_z = calc_excess_mom_z(20);
        double mom60_z = calc_excess_mom_z(60);
        double z_trend = std::tanh(0.55 * mom20_z + 0.45 * mom60_z);

        // 2. Wilder-style 14-Period RSI & Short-Horizon Overextension Mean-Reversion
        size_t rsi_len = std::min<size_t>(14, n - 1);
        double gains = 0.0, losses = 0.0;
        for (size_t i = n - rsi_len; i < n; ++i) {
            double d = P[i] - P[i - 1];
            if (d > 0.0) gains += d;
            else losses -= d;
        }
        double rsi = 50.0;
        if (gains + losses > 1e-12) {
            rsi = 100.0 * (gains / (gains + losses));
        }
        double rsi_overext = (rsi - 50.0) / 24.0;
        double z_mr = -std::tanh(0.45 * vwap_z + 0.55 * rsi_overext);

        // 3. 20-Day Volume-Confirmed OBV Accumulation Flow
        double z_obv = 0.0;
        if (V.size() == P.size()) {
            size_t v_len = std::min<size_t>(20, n - 1);
            double up_v = 0.0, dn_v = 0.0;
            for (size_t i = n - v_len; i < n; ++i) {
                double d = P[i] - P[i - 1];
                if (d > 0.0) up_v += V[i];
                else if (d < 0.0) dn_v += V[i];
            }
            if (up_v + dn_v > 0.0) {
                z_obv = std::clamp((up_v - dn_v) / (up_v + dn_v), -1.0, 1.0);
            }
        }

        // Horizon-adaptive blend: short-term mean-reversion decays over longer horizons
        double mr_weight = 0.32 * std::exp(-static_cast<double>(trading_days) / 18.0);
        double obv_weight = 0.18;
        double trend_weight = std::max(0.25, 1.0 - mr_weight - obv_weight);

        return std::clamp(trend_weight * z_trend + mr_weight * z_mr + obv_weight * z_obv, -1.0, 1.0);
    }
};

} // namespace quant
