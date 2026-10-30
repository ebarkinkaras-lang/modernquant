#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
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
    std::string horizon_name;
    double spot_price{0.0};
    double target_price{0.0};    // E[S_T] - Beklenen Değer
    double bull_target{0.0};     // +1 Sigma (Boğa Hedefi)
    double bear_support{0.0};    // -1 Sigma (Ayı Tabanı / Risk Desteği)
    double expected_return_pct{0.0};
    double bull_return_pct{0.0};
    double bear_return_pct{0.0};
    ForecastDirection direction{ForecastDirection::Neutral};
    std::string direction_label; // "🚀 GÜÇLÜ YUKARI", vb.
    double quant_score{0.0};     // -1.0 ile +1.0 arası normalize bileşik skor
    std::string rationale;
};

struct MultiHorizonForecast {
    std::string symbol;
    std::string company_name;
    std::string currency;
    double spot_price{0.0};
    double vwap{0.0};
    double annualized_volatility{0.0};
    double sentiment_score{0.0};
    std::string sentiment_label;
    std::vector<ForecastResult> horizons; // 1 Hafta (7g), 1 Ay (30g), 3 Ay (90g)
};

class QuantForecastEngine {
public:
    static ForecastResult forecast_for_days(const RealTimeAssetData& data,
                                            double sentiment_score,
                                            int days,
                                            const std::string& horizon_label = "") {
        ForecastResult res;
        res.symbol = data.symbol;
        res.days = days;
        res.spot_price = (data.current_price > 0.0) ? data.current_price : 100.0;

        // Para birimi belirleme
        if (data.symbol.ends_with(".IS") || data.symbol.ends_with(".TL")) res.currency = "TL";
        else if (data.symbol.ends_with(".DE")) res.currency = "EUR";
        else res.currency = "$";

        // Vade başlığı
        if (!horizon_label.empty()) {
            res.horizon_name = horizon_label;
        } else {
            if (days <= 7) res.horizon_name = I18n::tr(std::format("{} Gün (Kısa Vade)", days), std::format("{} Days (Short-Term)", days), std::format("{} Tage (Kurzfristig)", days));
            else if (days <= 35) res.horizon_name = I18n::tr(std::format("{} Gün (Orta Vade / 1 Ay)", days), std::format("{} Days (Mid-Term / 1Mo)", days), std::format("{} Tage (Mittelfristig / 1M)", days));
            else if (days <= 100) res.horizon_name = I18n::tr(std::format("{} Gün (Çeyreklik / 3 Ay)", days), std::format("{} Days (Quarterly / 3Mo)", days), std::format("{} Tage (Quartal / 3M)", days));
            else res.horizon_name = I18n::tr(std::format("{} Gün (Uzun Vade)", days), std::format("{} Days (Long-Term)", days), std::format("{} Tage (Langfristig)", days));
        }

        double T = static_cast<double>(days) / 365.25;
        double sigma = std::max(0.05, data.annualized_volatility);

        // 1. Taban Risksiz Oran ve Bölgesel Dinamikler
        double r_base = 0.041; // US varsayılan %4.1
        if (res.currency == "TL") {
            // BIST: Enflasyonist faiz yüksek (%45) ancak hisse senedi krizinde risk primi aşırı negatife döner
            r_base = (sentiment_score < -0.30) ? 0.08 : 0.38;
        } else if (res.currency == "EUR") {
            r_base = 0.0325; // XETRA Bund %3.25
        }

        // 2. Sentimel ve Makro Haber Kayması (Drift Etkisi)
        // Pozitif haber sermaye çeker (drift yükselir), kriz haberi (MSCI uyarısı) drift'i baskılar
        double alpha_sentiment = sentiment_score * (sigma * 1.35);

        // 3. VWAP Ortalamaya Dönüş (Mean-Reversion) Çekimi
        double vwap_dev = 0.0;
        if (data.vwap > 0.0) {
            vwap_dev = (res.spot_price - data.vwap) / data.vwap;
        }
        // Kısa vadede VWAP'a dönüş etkisi daha güçlüdür (hızlı düzeltme/tepki)
        double kappa = (days <= 7) ? 0.75 : ((days <= 30) ? 0.45 : 0.20);
        double alpha_vwap = -kappa * vwap_dev;

        // Toplam Yıllıklandırılmış Quant Drift (mu)
        double mu_quant = r_base + alpha_sentiment + alpha_vwap;

        // BIST kriz durumunda aşırı pozitif çıkmasını engelleme filtresi
        if (res.currency == "TL" && sentiment_score < -0.50) {
            mu_quant = std::min(mu_quant, -0.12); // Krizde negatif drift garantisi
        }

        // 4. Log-Normal Geometric Brownian Motion (GBM) Fiyat Hedefleri
        // E[S_T] = S_0 * exp(mu * T)
        res.target_price = res.spot_price * std::exp(mu_quant * T);

        // +1 Sigma Boğa Hedefi: S_0 * exp((mu - 0.5*sigma^2)*T + sigma*sqrt(T))
        double drift_term = (mu_quant - 0.5 * sigma * sigma) * T;
        double vol_term = sigma * std::sqrt(T);

        res.bull_target = res.spot_price * std::exp(drift_term + vol_term);
        res.bear_support = res.spot_price * std::exp(drift_term - vol_term);

        // Yüzdesel Getiri Hesaplamaları
        res.expected_return_pct = ((res.target_price / res.spot_price) - 1.0) * 100.0;
        res.bull_return_pct = ((res.bull_target / res.spot_price) - 1.0) * 100.0;
        res.bear_return_pct = ((res.bear_support / res.spot_price) - 1.0) * 100.0;

        // 5. Bileşik Quant Yön Skoru (-1.0 ile +1.0)
        // Skor = 45% Sentimel + 35% Drift Sharpe Oranı + 20% VWAP momentum
        double drift_sharpe = (sigma > 0.0) ? (mu_quant / sigma) * std::sqrt(T) : 0.0;
        double vwap_score = (vwap_dev > 0.0) ? 0.3 : -0.3;

        double raw_score = (0.45 * sentiment_score) + (0.35 * std::tanh(drift_sharpe)) + (0.20 * vwap_score);
        res.quant_score = std::clamp(raw_score, -1.0, 1.0);

        // 6. 5 Kademeli Yön Sınıflandırması
        if (res.quant_score >= 0.38) {
            res.direction = ForecastDirection::StrongBullish;
            res.direction_label = I18n::tr("🚀 GÜÇLÜ YUKARI (Strong Bullish)", "🚀 STRONG BULLISH", "🚀 STARK BULLISCH");
            res.rationale = I18n::tr(
                std::format("Güçlü pozitif sentimel ve yukarı yönlü kurumsal momentum bu vadede belirgin prim potansiyeline (%{:.1f}) işaret ediyor.", res.expected_return_pct),
                std::format("Strong positive sentiment and institutional upward momentum indicate significant upside potential (+{:.1f}%).", res.expected_return_pct),
                std::format("Starkes positives Sentiment und institutionelles Momentum deuten auf erhebliches Aufwärtspotenzial (+{:.1f}%) hin.", res.expected_return_pct)
            );
        } else if (res.quant_score >= 0.12) {
            res.direction = ForecastDirection::Bullish;
            res.direction_label = I18n::tr("📈 YUKARI (Bullish)", "📈 BULLISH", "📈 BULLISCH");
            res.rationale = I18n::tr(
                std::format("Pozitif getiri beklentisi (%{:.1f}) hakim; kurumsal adil değer desteği üzerinde kademeli yükseliş öngörülüyor.", res.expected_return_pct),
                std::format("Positive return outlook (+{:.1f}%); gradual upside above institutional fair value support is projected.", res.expected_return_pct),
                std::format("Positiver Renditeausblick (+{:.1f}%); schrittweiser Anstieg über der institutionellen Fair-Value-Unterstützung erwartet.", res.expected_return_pct)
            );
        } else if (res.quant_score > -0.12) {
            res.direction = ForecastDirection::Neutral;
            res.direction_label = I18n::tr("⚖️ NÖTR / KONSOLİDASYON (Neutral)", "⚖️ NEUTRAL / CONSOLIDATION", "⚖️ NEUTRAL / KONSOLIDIERUNG");
            res.rationale = I18n::tr(
                std::format("Fiyat ve haber dengesi yatay bantta (%{:.1f}); yön belirleyici bir kırılım öncesi konsolidasyon bekleniyor.", res.expected_return_pct),
                std::format("Price and news balance in horizontal range ({:+.1f}%); consolidation expected prior to decisive breakout.", res.expected_return_pct),
                std::format("Kurs und Nachrichten im Seitwärtsbereich ({:+.1f}%); Konsolidierung vor richtungsweisendem Ausbruch erwartet.", res.expected_return_pct)
            );
        } else if (res.quant_score > -0.38) {
            res.direction = ForecastDirection::Bearish;
            res.direction_label = I18n::tr("📉 AŞAĞI (Bearish)", "📉 BEARISH", "📉 BÄRISCH");
            res.rationale = I18n::tr(
                std::format("Negatif makro rüzgarlar ve satış baskısı altında (%{:.1f} düzeltme beklentisi); destek seviyesi {:.2f} {} izlenmelidir.", res.expected_return_pct, res.bear_support, res.currency),
                std::format("Under negative macro headwinds and selling pressure ({:.1f}% correction outlook); monitor support at {:.2f} {}.", res.expected_return_pct, res.bear_support, res.currency),
                std::format("Unter negativem Gegenwind und Verkaufsdruck ({:.1f}% Korrekturausblick); Unterstützung bei {:.2f} {} beachten.", res.expected_return_pct, res.bear_support, res.currency)
            );
        } else {
            res.direction = ForecastDirection::StrongBearish;
            res.direction_label = I18n::tr("🔻 GÜÇLÜ AŞAĞI (Strong Bearish)", "🔻 STRONG BEARISH", "🔻 STARK BÄRISCH");
            res.rationale = I18n::tr(
                std::format("Kriz / ağır satış baskısı ve negatif haber akışı (%{:.1f} risk); düşen bıçak tutulmamalı, dip taban beklenmelidir.", res.expected_return_pct),
                std::format("Severe selling pressure & negative news flow ({:.1f}% risk); avoid catching falling knives, await bottom base.", res.expected_return_pct),
                std::format("Starker Verkaufsdruck & negative Nachrichten ({:.1f}% Risiko); fallendes Messer meiden, Bodenbildung abwarten.", res.expected_return_pct)
            );
        }

        return res;
    }

    static MultiHorizonForecast generate_multi_horizon(const RealTimeAssetData& data,
                                                       double sentiment_score,
                                                       const std::string& sentiment_label) {
        MultiHorizonForecast multi;
        multi.symbol = data.symbol;
        multi.currency = (data.symbol.ends_with(".IS") || data.symbol.ends_with(".TL")) ? "TL" : (data.symbol.ends_with(".DE") ? "EUR" : "$");
        multi.spot_price = data.current_price;
        multi.vwap = data.vwap;
        multi.annualized_volatility = data.annualized_volatility;
        multi.sentiment_score = sentiment_score;
        multi.sentiment_label = sentiment_label;

        // 3 Standart Vade: 1 Hafta (7 gün), 1 Ay (30 gün), 3 Ay (90 gün)
        multi.horizons.push_back(forecast_for_days(data, sentiment_score, 7, I18n::tr("1 Hafta (Kısa Vade)", "1 Week (Short-Term)", "1 Woche (Kurzfristig)")));
        multi.horizons.push_back(forecast_for_days(data, sentiment_score, 30, I18n::tr("1 Ay (Orta Vade)", "1 Month (Mid-Term)", "1 Monat (Mittelfristig)")));
        multi.horizons.push_back(forecast_for_days(data, sentiment_score, 90, I18n::tr("3 Ay (Çeyreklik / Makro)", "3 Months (Quarterly / Macro)", "3 Monate (Quartal / Makro)")));

        return multi;
    }

    static void render_forecast_matrix(const MultiHorizonForecast& multi) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::string title_tmpl = I18n::tr(
            "🎯 {} HEDEF FİYAT & ZAMAN DİLİMLERİNE GÖRE YÖN TAHMİN MATRİSİ",
            "🎯 {} TARGET PRICE & MULTI-HORIZON DIRECTION FORECAST MATRIX",
            "🎯 {} ZIELKURS & MULTI-HORIZONT-RICHTUNGSPROGNOSE-MATRIX"
        );
        std::string box_title = std::vformat(title_tmpl, std::make_format_args(multi.symbol));

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << std::format("║  {:<76}║\n", box_title);
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        std::cout << std::format("   {:<24}: {}{:.2f} {}{}\n", I18n::tr("Anlık Spot Fiyat", "Current Spot Price", "Aktueller Kassakurs"),
                                 pal.glow_rgb, multi.spot_price, multi.currency, pal.reset_code);
        std::cout << std::format("   {:<24}: {:.2f} {}\n", I18n::tr("30-Günlük Hacim VWAP'ı", "30-Day Volume VWAP", "30-Tage Volumen-VWAP"),
                                 multi.vwap, multi.currency);
        std::cout << std::format("   {:<24}: %{:.2f}\n", I18n::tr("Yıllıklandırılmış Vol.", "Annualized Volatility", "Annualisierte Volatilität"),
                                 multi.annualized_volatility * 100.0);
        std::cout << std::format("   {:<24}: {}{:.2f}{} ({})\n", I18n::tr("Sentimel Puanı & Etki", "Sentiment Score & Impact", "Sentiment-Score & Einfluss"),
                                 pal.glow_rgb, multi.sentiment_score, pal.reset_code, multi.sentiment_label);

        std::cout << pal.glow_rgb;
        std::cout << "╟──────────────────────────────────────────────────────────────────────────────╢\n";
        std::string col_hdr = I18n::tr(
            "║ VADE DİLİMİ         YÖN TAHMİNİ           HEDEF FİYAT    BOĞA (+1σ)   AYI (-1σ)║",
            "║ TIME HORIZON        DIRECTION FORECAST    TARGET PRICE   BULL (+1σ)   BEAR (-1σ)║",
            "║ ZEITHORIZONT        TREND-PROGNOSE        ZIELKURS       BULL (+1σ)   BÄR  (-1σ)║"
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
                dir_short = I18n::tr("📈 YUKARI      ", "📈 BULLISH       ", "📈 BULLISCH      ");
                dir_color = pal.glow_rgb;
            } else if (h.direction == ForecastDirection::Neutral) {
                dir_short = I18n::tr("⚖️ NÖTR / YATAY", "⚖️ NEUTRAL / RANGE", "⚖️ NEUTRAL/SEITW");
                dir_color = pal.dim_rgb;
            } else if (h.direction == ForecastDirection::Bearish) {
                dir_short = I18n::tr("📉 AŞAĞI       ", "📉 BEARISH       ", "📉 BÄRISCH       ");
                dir_color = pal.base_rgb;
            } else {
                dir_short = I18n::tr("🔻 GÜÇLÜ AŞAĞI ", "🔻 STRONG BEARISH", "🔻 STARK BÄRISCH ");
                dir_color = pal.red_rgb;
            }

            std::string target_str = std::format("{:.2f} ({:+4.1f}%)", h.target_price, h.expected_return_pct);
            std::string bull_str   = std::format("{:.2f}", h.bull_target);
            std::string bear_str   = std::format("{:.2f}", h.bear_support);

            std::cout << std::format("  {:<18} {}{:<20}{} {:<14} {:<12} {:<10}\n",
                                     h.horizon_name,
                                     dir_color, dir_short, pal.reset_code,
                                     target_str, bull_str, bear_str);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::string rat_hdr = I18n::tr(
            "║  🔍 VADELERE GÖRE AYRINTILI QUANT GEREKÇELERİ                                ║",
            "║  🔍 DETAILED QUANT RATIONALE BY TIME HORIZON                                ║",
            "║  🔍 DETAILLIERTE QUANT-BEGRÜNDUNG NACH ZEITHORIZONT                          ║"
        );
        std::cout << rat_hdr << "\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        for (const auto& h : multi.horizons) {
            std::cout << std::format("   • {}{}{}: {}\n", pal.glow_rgb, h.horizon_name, pal.reset_code, h.rationale);
        }

        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }
};

} // namespace quant
