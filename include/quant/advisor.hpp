#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "black_scholes.hpp"
#include "news_sentiment.hpp"
#include "fundamentals.hpp"
#include "forecast_engine.hpp"
#include "portfolio_linear_algebra.hpp"
#include "i18n.hpp"
#include <vector>
#include <string>
#include <format>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace quant {

enum class MarketRegion {
    BIST,   // Borsa İstanbul (Türkiye) - TRY
    XETRA,  // Frankfurt Xetra (Almanya) - EUR
    US      // Wall Street (ABD) - USD
};

enum class InvestorRisk {
    DüşükRisk_Koruma,    // Sermaye koruma & Garanti Faiz
    Dengeli_Büyüme,      // Hisse + Faiz + Defansif
    YüksekGetiri_Agresif // Büyüme Hisseleri + Momentum
};

struct StockPositionRecommendation {
    std::string ticker;
    std::string company_name;
    double spot_price{0.0};
    double target_price{0.0};
    double stop_loss{0.0};
    double weight_pct{0.0};
    double allocation_amount{0.0};
    int lot_count{0};
    std::string recommendation;
    std::string rationale;

    // Kurumsal FAVÖK & Bilanço Özeti
    std::string fundamental_summary;
    std::string fundamental_grade;

    // Canlı Sentimel ve Makro Haber Analizi
    double sentiment_score{0.0};
    std::string sentiment_label;
    std::string sentiment_bar;
    std::string macro_catalyst;
    std::string macro_risk;
    std::vector<std::string> headlines;
};

struct RegionalMarketPlan {
    MarketRegion region;
    InvestorRisk risk;
    std::string currency_symbol;
    std::string currency_code;
    double total_capital{100000.0};
    double expected_annual_return_pct{0.0};
    double expected_volatility_pct{0.0};
    double max_drawdown_risk_pct{0.0};
    std::string macro_verdict;

    RegionalMacroSentiment macro_sentiment;

    std::vector<StockPositionRecommendation> positions;
    std::vector<std::string> actionable_steps;
};

class QuantInvestmentAdvisor {
public:
    static RegionalMarketPlan generate_regional_plan(double capital, MarketRegion region, InvestorRisk risk, LiveMarketDataClient& client) {
        RegionalMarketPlan plan;
        plan.total_capital = std::max(100.0, capital);
        plan.region = region;
        plan.risk = risk;

        if (region == MarketRegion::BIST) {
            plan.currency_symbol = "₺";
            plan.currency_code = "TL";
            build_quantitative_plan_(plan, plan.total_capital, risk, client, MarketRegion::BIST);
        } else if (region == MarketRegion::XETRA) {
            plan.currency_symbol = "€";
            plan.currency_code = "EUR";
            build_quantitative_plan_(plan, plan.total_capital, risk, client, MarketRegion::XETRA);
        } else {
            plan.currency_symbol = "$";
            plan.currency_code = "USD";
            build_quantitative_plan_(plan, plan.total_capital, risk, client, MarketRegion::US);
        }

        return plan;
    }

    static void render_plan_cards(const RegionalMarketPlan& plan) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::string region_title;
        if (plan.region == MarketRegion::BIST) {
            region_title = I18n::tr("║  🇹🇷 BORSA İSTANBUL (BIST 100) ─ KOVARYANS & FAVÖK BAZLI SERMAYE PLANI",
                                    "║  🇹🇷 BORSA ISTANBUL (BIST 100) ─ COVARIANCE & EBITDA CAPITAL PLAN",
                                    "║  🇹🇷 BORSA ISTANBUL (BIST 100) ─ KOVARIANZ- & EBITDA-KAPITALPLAN");
        } else if (plan.region == MarketRegion::XETRA) {
            region_title = I18n::tr("║  🇩🇪 FRANKFURT XETRA (DAX 40) ─ KOVARYANS & FAVÖK BAZLI SERMAYE PLANI",
                                    "║  🇩🇪 FRANKFURT XETRA (DAX 40) ─ COVARIANCE & EBITDA CAPITAL PLAN",
                                    "║  🇩🇪 FRANKFURT XETRA (DAX 40) ─ KOVARIANZ- & EBITDA-KAPITALPLAN");
        } else {
            region_title = I18n::tr("║  🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ KOVARYANS & FAVÖK SERMAYE PLANI",
                                    "║  🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ COVARIANCE & EBITDA CAPITAL PLAN",
                                    "║  🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ KOVARIANZ- & EBITDA-KAPITALPLAN");
        }

        std::string risk_title;
        if (plan.risk == InvestorRisk::DüşükRisk_Koruma) {
            risk_title = I18n::tr("Muhafazakar / Sermaye Koruma & Sabit Getiri Ağırlıklı",
                                  "Conservative / Capital Preservation & Fixed Income Focus",
                                  "Konservativ / Kapitalerhalt & Festverzinslicher Fokus");
        } else if (plan.risk == InvestorRisk::Dengeli_Büyüme) {
            risk_title = I18n::tr("Dengeli Büyüme (Ters-Volatilite + FAVÖK Kalitesi + Likidite Tamponu)",
                                  "Balanced Growth (Inverse-Vol + EBITDA Quality + Cash Buffer)",
                                  "Ausgewogenes Wachstum (Inverse-Vol + EBITDA-Qualität + Cash-Puffer)");
        } else {
            risk_title = I18n::tr("Yüksek Getiri & Faktör Momentum (Tam Hisse / Alfa Ağırlıklı)",
                                  "High Growth & Factor Momentum (High Equity Exposure)",
                                  "Hohes Wachstum & Faktor-Momentum (Hohes Aktien-Exposure)");
        }

        // 1. ÖZET PORTFÖY MATEMATİĞİ KARTI
        int hpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(region_title));
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << region_title << std::string(static_cast<size_t>(hpad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << std::format("   {:<27}: {}{}{}\n", I18n::tr("Strateji Profili", "Strategy Profile", "Strategieprofil"),
                                 pal.glow_rgb, risk_title, pal.reset_code);
        std::cout << std::format("   {:<27}: {}{:.2f} {}{}\n", I18n::tr("Toplam Yatırım Bütçesi", "Total Investment Budget", "Gesamtes Anlagebudget"),
                                 pal.glow_rgb, plan.total_capital, plan.currency_code, pal.reset_code);
        std::cout << std::format("   {:<27}: {}%{:.1f}{} (Ağırlıklı Beklenen Getiri)\n",
                                 I18n::tr("Model Yıllık Beklenti", "Model Expected Return", "Erwartete Jahresrendite"),
                                 pal.glow_rgb, plan.expected_annual_return_pct, pal.reset_code);
        std::cout << std::format("   {:<27}: %{:.2f} (Eigen Kovaryans Matrisi √wᵀΣw)\n",
                                 I18n::tr("Portföy Volatilitesi", "Portfolio Volatility", "Portfolio-Volatilität"),
                                 plan.expected_volatility_pct);
        std::cout << std::format("   {:<27}: %{:.1f} (95% VaR / Parametrik Drawdown Sınırı)\n\n",
                                 I18n::tr("Tahmini Maks. Risk (DD)", "Estimated Max Drawdown", "Geschätzter Max Drawdown"),
                                 plan.max_drawdown_risk_pct);

        std::cout << pal.glow_rgb << "   " << I18n::tr("CANLI MAKRO REJİM & FAKTÖR DEĞERLENDİRMESİ:\n", "LIVE MACRO REGIME & FACTOR VERDICT:\n", "LIVE-MAKRO-REGIME & FAKTOR-BEWERTUNG:\n") << pal.reset_code;
        print_wrapped_(plan.macro_verdict, 74, "   ", pal.base_rgb, pal.reset_code);
        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;

        // 2. CANLI MAKRO EKONOMİK VE ENDEKS VERİ AKIŞI KARTI
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::string news_hdr = I18n::tr(
            "║  📰 CANLI ENDEKS REJİMİ, KUR/FAİZ TELEMETRİSİ VE HABER AKIŞI",
            "║  📰 LIVE INDEX REGIME, FX/YIELD TELEMETRY & NEWS FLOW",
            "║  📰 LIVE-INDEX-REGIME, DEVISEN-/ZINSTELEMETRIE & NACHRICHTEN"
        );
        int npad = std::max(0, 79 - ThemeEngine::utf8_visible_length(news_hdr));
        std::cout << news_hdr << std::string(static_cast<size_t>(npad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << std::format("   {:<27}: {}{}{}  {}{}\n",
                                 I18n::tr("Bölgesel Piyasa Sentimenti", "Regional Market Sentiment", "Regionales Markt-Sentiment"),
                                 pal.glow_rgb, plan.macro_sentiment.overall_gauge_bar, pal.reset_code,
                                 pal.glow_rgb, plan.macro_sentiment.overall_sentiment_label, pal.reset_code);
        std::cout << pal.dim_rgb << "   " << I18n::tr("Canlı Makro Sentez:\n", "Live Macro Synthesis:\n", "Live-Makro-Synthese:\n") << pal.reset_code;
        print_wrapped_(plan.macro_sentiment.macro_synthesis, 74, "   ", pal.base_rgb, pal.reset_code);
        std::cout << "\n" << pal.glow_rgb << "   " << I18n::tr("PİYASAYI YÖNLENDİREN CANLI VERİ & HABERLER:\n", "LIVE TELEMETRY & HEADLINES DRIVING THE MARKET:\n", "MARKTRELEVANTE LIVE-DATEN & SCHLAGZEILEN:\n") << pal.reset_code;

        for (size_t k = 0; k < plan.macro_sentiment.macro_developments.size(); ++k) {
            const auto& mdev = plan.macro_sentiment.macro_developments[k];
            std::string header = std::format("[{}] {}", k + 1, mdev.title);
            std::cout << "\n";
            print_wrapped_(header, 74, "   ", pal.glow_rgb, pal.reset_code);
            std::cout << std::format("       {}{:<12}:{} {}{}\n", pal.dim_rgb, I18n::tr("Kaynak", "Source", "Quelle"), pal.reset_code, mdev.source, pal.reset_code);
            print_wrapped_(I18n::tr("Borsa Etkisi: ", "Market Impact: ", "Markt-Einfluss: ") + mdev.market_impact, 74, "       ", pal.base_rgb, pal.reset_code);
        }
        std::cout << pal.glow_rgb;
        std::cout << "\n╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;

        // 3. HER VARLIK İÇİN AYRI AYRI KARTLAR (FAVÖK & Bilanço Özeti Dahil)
        std::cout << "\n" << pal.glow_rgb
                  << I18n::tr("=== OPTİMİZE VARLIK DAĞILIMI, LOT SAYILARI VE FAVÖK BİLANÇO ÖZETİ ===",
                              "=== OPTIMIZED ASSET ALLOCATION, SHARE COUNTS & EBITDA SUMMARY ===",
                              "=== OPTIMIERTE ALLOKATION, STÜCKZAHLEN & EBITDA-ÜBERSICHT ===")
                  << pal.reset_code << "\n";

        for (size_t i = 0; i < plan.positions.size(); ++i) {
            const auto& pos = plan.positions[i];
            bool is_fixed = (pos.lot_count == 0 && (pos.ticker.find("MEVDUAT") != std::string::npos ||
                                                    pos.ticker.find("FON") != std::string::npos ||
                                                    pos.ticker.find("BUND") != std::string::npos ||
                                                    pos.ticker.find("CASH") != std::string::npos ||
                                                    pos.ticker.find("IRX") != std::string::npos));

            std::string card_hdr = std::format("│ [{}] {:<12} ─ {}", i + 1, pos.ticker, pos.company_name);
            int cpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(card_hdr));

            std::cout << "\n" << pal.glow_rgb;
            std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐\n";
            std::cout << card_hdr << std::string(static_cast<size_t>(cpad), ' ') << "│\n";
            std::cout << "├──────────────────────────────────────────────────────────────────────────────┤\n";
            std::cout << pal.reset_code;

            std::string gauge = make_gauge_(pos.weight_pct, 14);
            std::cout << std::format("  {:<20}: {}{}{}  {}%{:.1f}{}\n",
                                     I18n::tr("Portföy Ağırlığı", "Portfolio Weight", "Portfolio-Anteil"),
                                     pal.glow_rgb, gauge, pal.reset_code,
                                     pal.base_rgb, pos.weight_pct, pal.reset_code);
            std::cout << std::format("  {:<20}: {}{:.2f} {}{}\n",
                                     I18n::tr("Tahsis Edilen Tutar", "Allocation Amount", "Anlagebetrag"),
                                     pal.glow_rgb, pos.allocation_amount, plan.currency_code, pal.reset_code);

            if (!is_fixed) {
                std::cout << std::format("  {:<20}: {}{} {}{} ({}: {:.2f} {})\n\n",
                                         I18n::tr("ALINACAK LOT", "SHARES TO BUY", "ZU KAUFENDE STK."),
                                         pal.glow_rgb, pos.lot_count, I18n::tr("ADET", "SHARES", "STÜCK"), pal.reset_code,
                                         I18n::tr("Anlık Spot", "Current Spot", "Aktueller Kurs"), pos.spot_price, plan.currency_code);

                double pot_gain = (pos.spot_price > 0.0) ? ((pos.target_price / pos.spot_price) - 1.0) * 100.0 : 0.0;
                double pot_loss = (pos.spot_price > 0.0) ? ((pos.spot_price - pos.stop_loss) / pos.spot_price) * 100.0 : 0.0;

                std::cout << std::format("  {:<24}: {:.2f} {}  ({}{:+.1f}% {}{})\n",
                                         I18n::tr("3-AYLIK MODEL HEDEFİ", "3-MONTH MODEL TARGET", "3-MONATS-ZIELKURS"),
                                         pos.target_price, plan.currency_code, pal.glow_rgb, pot_gain,
                                         I18n::tr("Potansiyel", "Potential", "Potenzial"), pal.reset_code);
                std::cout << std::format("  {:<24}: {:.2f} {}  ({}-{:.1f}% ATR Stop{})\n",
                                         I18n::tr("ZARAR KES (ATR Stop)", "STOP-LOSS (ATR Stop)", "STOP-LOSS (ATR)"),
                                         pos.stop_loss, plan.currency_code, pal.dim_rgb, pot_loss, pal.reset_code);

                if (!pos.fundamental_summary.empty()) {
                    std::cout << std::format("  {:<24}: {}\n",
                                             I18n::tr("Temel & FAVÖK (Ek Bilgi)", "EBITDA & Fundamentals", "EBITDA & Fundamental"),
                                             pos.fundamental_summary);
                }
                std::cout << "\n";
            } else {
                std::cout << std::format("  {:<20}: {}{}{}\n\n",
                                         I18n::tr("Varlık Türü", "Asset Type", "Anlageklasse"),
                                         pal.dim_rgb,
                                         I18n::tr("Sermaye Güvenceli Likit Nakit / Sabit Getiri Enstrümanı",
                                                  "Capital-Preserving Liquid Cash / Fixed Income",
                                                  "Kapitalerhaltende Liquidität / Festverzinslich"),
                                         pal.reset_code);
            }

            std::cout << std::format("  {:<20}: {}[ {} ]{}\n\n",
                                     I18n::tr("MODEL TAVSİYESİ", "MODEL ACTION", "MODELL-EMPFEHLUNG"),
                                     pal.glow_rgb, pos.recommendation, pal.reset_code);

            if (!pos.sentiment_bar.empty()) {
                std::cout << pal.glow_rgb << "  " << I18n::tr("CANLI HABER & SENTİMENT ANALİZİ:\n", "LIVE NEWS & SENTIMENT ANALYSIS:\n", "LIVE-NEWS- & SENTIMENT-ANALYSE:\n") << pal.reset_code;
                std::cout << std::format("  • {:<18}: {}{}{}  {}{}\n",
                                         I18n::tr("Sentimel Puan", "Sentiment Score", "Sentiment-Score"),
                                         pal.glow_rgb, pos.sentiment_bar, pal.reset_code,
                                         pal.glow_rgb, pos.sentiment_label, pal.reset_code);
                if (!pos.headlines.empty()) {
                    std::cout << pal.dim_rgb << "  • " << I18n::tr("Canlı Manşetler:\n", "Live Headlines:\n", "Live-Schlagzeilen:\n") << pal.reset_code;
                    for (const auto& h : pos.headlines) {
                        print_wrapped_(h, 74, "    ✔ ", pal.base_rgb, pal.reset_code);
                    }
                }
                std::cout << "\n";
            }

            std::cout << pal.dim_rgb << "  " << I18n::tr("Kantitatif Tahsis Gerekçesi:\n", "Quantitative Allocation Rationale:\n", "Quantitative Allokationsbegründung:\n") << pal.reset_code;
            print_wrapped_(pos.rationale, 74, "  • ", pal.base_rgb, pal.reset_code);

            std::cout << pal.glow_rgb;
            std::cout << "└──────────────────────────────────────────────────────────────────────────────┘\n" << pal.reset_code;
        }

        // 4. SOMUT UYGULAMA ADIMLARI KARTI
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << I18n::tr(
            "║  SOMUT UYGULAMA ADIMLARI (PORTFÖY RİSK YÖNETİM PROTOKOLÜ)                    ║\n",
            "║  ACTIONABLE EXECUTION STEPS (PORTFOLIO RISK MANAGEMENT PROTOCOL)             ║\n",
            "║  KONKRETE UMSETZUNGSSCHRITTE (PORTFOLIO-RISIKOMANAGEMENT-PROTOKOLL)          ║\n"
        );
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        for (const auto& step : plan.actionable_steps) {
            print_wrapped_(step, 74, "  ✔ ", pal.base_rgb, pal.reset_code);
        }
        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }

private:
    struct CandidateSlot {
        std::string symbol;
        std::string fallback_name;
        RealTimeAssetData data;
        FundamentalProfile fund;
        AssetSentimentProfile sent;
        ForecastResult fc_63d;
        double raw_factor_weight{0.0};
    };

    static void build_quantitative_plan_(RegionalMarketPlan& plan,
                                         double capital,
                                         InvestorRisk risk,
                                         LiveMarketDataClient& client,
                                         MarketRegion region) {
        std::vector<std::pair<std::string, std::string>> universe;
        std::string cash_ticker;
        std::string cash_name;
        double rf_rate = 0.042;

        if (region == MarketRegion::BIST) {
            plan.macro_sentiment = NewsSentimentEngine::build_bist_macro_sentiment(&client);
            universe = {
                {"THYAO.IS", "Türk Hava Yolları"},
                {"ASELS.IS", "Aselsan Elektronik"},
                {"TUPRS.IS", "Tüpraş Türkiye Petrol Rafinerileri"},
                {"BIMAS.IS", "BİM Birleşik Mağazalar"},
                {"FROTO.IS", "Ford Otomotiv Sanayi"}
            };
            cash_ticker = "TL_PPF_MEVDUAT";
            cash_name = "TCMB Gecelik Para Piyasası Fonu / TL Mevduat";
            rf_rate = 0.425; // Current annual TRY money-market yield baseline
        } else if (region == MarketRegion::XETRA) {
            plan.macro_sentiment = NewsSentimentEngine::build_xetra_macro_sentiment(client);
            universe = {
                {"SAP.DE", "SAP SE (Kurumsal Bulut & AI)"},
                {"SIE.DE", "Siemens AG (Endüstriyel Otomasyon & Enerji)"},
                {"ALV.DE", "Allianz SE (Sigorta & Varlık Yönetimi)"},
                {"RHM.DE", "Rheinmetall AG (Avrupa Savunma Sanayii)"}
            };
            cash_ticker = "DE_BUND_CASH";
            cash_name = "Almanya Kısa Vadeli Devlet Tahvili (Bund) / Euro Para Piyasası";
            rf_rate = 0.0265;
        } else {
            plan.macro_sentiment = NewsSentimentEngine::build_us_macro_sentiment(client);
            auto yc = client.fetch_treasury_yields();
            rf_rate = std::clamp(yc.three_month_yield, 0.01, 0.08);
            universe = {
                {"SPY",  "SPDR S&P 500 ETF Trust (Geniş ABD Endeksi)"},
                {"MSFT", "Microsoft Corporation (Bulut & Kurumsal AI)"},
                {"AAPL", "Apple Inc. (Ekosistem & Serbest Nakit Akışı)"},
                {"NVDA", "NVIDIA Corporation (AI Veri Merkezi & GPU)"}
            };
            cash_ticker = "^IRX / SGOV";
            cash_name = "ABD 3-Aylık Hazine Bonosu (T-Bill / SGOV)";
        }

        const bool is_crisis = plan.macro_sentiment.is_crisis_regime;

        // Target Cash Share determined transparently by Risk Profile & Live Stress Regime
        double cash_share = 0.30;
        if (risk == InvestorRisk::DüşükRisk_Koruma) {
            cash_share = is_crisis ? 0.75 : 0.55;
        } else if (risk == InvestorRisk::Dengeli_Büyüme) {
            cash_share = is_crisis ? 0.55 : 0.30;
        } else {
            cash_share = is_crisis ? 0.30 : 0.05;
        }

        // Fetch live 6-month OHLCV, Fundamentals (FAVÖK), News & 63-Trading-Day Forecast for each candidate
        std::vector<CandidateSlot> slots;
        for (const auto& [sym, fname] : universe) {
            auto d = client.fetch_asset(sym, "6mo");
            if (d.current_price <= 0.0) continue;

            CandidateSlot s;
            s.symbol = sym;
            s.fallback_name = d.long_name.empty() ? fname : d.long_name;
            s.data = std::move(d);
            s.fund = FundamentalEngine::fetch_fundamentals(client, s.data);
            s.sent = NewsSentimentEngine::get_asset_sentiment(sym, s.fallback_name, client);
            s.fc_63d = QuantForecastEngine::forecast_for_trading_days(
                s.data, 63, s.sent.sentiment_score, s.sent.has_live_news, &s.fund
            );

            // Inverse-Volatility x (Momentum + FAVÖK Fundamental Quality) Score
            double vol = std::clamp(s.data.annualized_volatility, 0.10, 0.85);
            double inv_vol = 1.0 / vol;
            double quality_tilt = 1.0
                + 0.35 * (s.fund.is_available ? s.fund.fundamental_score : 0.0)
                + 0.25 * s.fc_63d.quant_score;
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                // Conservative profile prefers lower-vol, strong-balance-sheet names
                s.raw_factor_weight = (inv_vol * inv_vol) * std::clamp(quality_tilt, 0.40, 1.80);
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                s.raw_factor_weight = inv_vol * std::clamp(quality_tilt, 0.45, 1.90);
            } else {
                // Aggressive profile tilts toward higher expected 63d upside + fundamental growth
                s.raw_factor_weight = std::pow(inv_vol, 0.5) * std::clamp(quality_tilt + 0.25 * s.fc_63d.quant_score, 0.35, 2.20);
            }
            slots.push_back(std::move(s));
        }

        // Pick top 3-4 equities and normalize equity weights to (1 - cash_share)
        std::sort(slots.begin(), slots.end(), [](const CandidateSlot& a, const CandidateSlot& b) {
            return a.raw_factor_weight > b.raw_factor_weight;
        });
        size_t max_eq = (risk == InvestorRisk::DüşükRisk_Koruma) ? 3 : 4;
        if (slots.size() > max_eq) slots.resize(max_eq);

        double sum_raw = 0.0;
        for (const auto& s : slots) sum_raw += s.raw_factor_weight;

        double eq_budget_share = (slots.empty()) ? 0.0 : (1.0 - cash_share);
        if (slots.empty()) cash_share = 1.0;

        // Add Cash / Risk-Free Buffer Position if cash_share >= 0.05
        if (cash_share >= 0.049) {
            StockPositionRecommendation cash_pos;
            cash_pos.ticker = cash_ticker;
            cash_pos.company_name = cash_name;
            cash_pos.spot_price = 1.0;
            cash_pos.target_price = 1.0 + rf_rate;
            cash_pos.stop_loss = 1.0;
            cash_pos.weight_pct = cash_share * 100.0;
            cash_pos.allocation_amount = capital * cash_share;
            cash_pos.lot_count = 0;
            cash_pos.recommendation = I18n::tr("SABİT GETİRİ / LİKİDİTE KALKANI", "FIXED YIELD / LIQUIDITY BUFFER", "ZINSPUFFER / LIQUIDITÄT");
            cash_pos.sentiment_score = 0.0;
            cash_pos.sentiment_label = std::format("RİSKSİZ GETİRİ (~%{:.2f}/yıl)", rf_rate * 100.0);
            cash_pos.sentiment_bar = "[──────┼███───]";
            cash_pos.rationale = I18n::tr(
                std::format("Portföy oynaklığını sınırlandıran ve yıllık ~%{:.2f} risksiz getiri üreten nakit/tahvil çıpası.", rf_rate * 100.0),
                std::format("Cash/T-Bill anchor dampening portfolio volatility while earning ~{:.2f}% annual risk-free yield.", rf_rate * 100.0),
                std::format("Liquiditätsanker zur Volatilitätsbegrenzung mit ~{:.2f}% jährlicher Rendite.", rf_rate * 100.0)
            );
            plan.positions.push_back(std::move(cash_pos));
        }

        // Build Equity Positions & Eigen Covariance Matrix Inputs
        const int m = static_cast<int>(slots.size());
        Eigen::VectorXd eq_weights = Eigen::VectorXd::Zero(std::max(1, m));
        Eigen::VectorXd eq_vols    = Eigen::VectorXd::Zero(std::max(1, m));
        double weighted_eq_ann_ret = 0.0;

        for (int i = 0; i < m; ++i) {
            const auto& s = slots[static_cast<size_t>(i)];
            double w_port = (sum_raw > 0.0) ? (eq_budget_share * (s.raw_factor_weight / sum_raw)) : (eq_budget_share / m);
            eq_weights(i) = w_port;
            eq_vols(i) = std::clamp(s.data.annualized_volatility, 0.08, 0.95);

            // Annualize 63-day expected log return conservatively (shrunk toward long-run equity risk premium)
            double r_model_ann = std::log(std::max(0.2, s.fc_63d.target_price / s.data.current_price)) * (252.0 / 63.0);
            double erp_anchor = rf_rate + 0.065;
            double blended_ann_ret = 0.55 * erp_anchor + 0.45 * std::clamp(r_model_ann, -0.25, rf_rate + 0.35);
            weighted_eq_ann_ret += w_port * blended_ann_ret;

            StockPositionRecommendation pos;
            pos.ticker = s.symbol;
            pos.company_name = s.fallback_name;
            pos.spot_price = s.data.current_price;
            pos.weight_pct = w_port * 100.0;
            pos.allocation_amount = capital * w_port;
            pos.lot_count = (pos.spot_price > 0.0) ? static_cast<int>(std::floor(pos.allocation_amount / pos.spot_price)) : 0;

            double atr = (s.data.atr_14 > 0.0) ? s.data.atr_14 : (pos.spot_price * (eq_vols(i) / std::sqrt(252.0)) * 1.35);
            double stop_dist = std::clamp(2.0 * atr, 0.035 * pos.spot_price, 0.12 * pos.spot_price);
            pos.stop_loss = pos.spot_price - stop_dist;
            pos.target_price = std::max(s.fc_63d.target_price, pos.spot_price + 1.8 * atr);

            pos.fundamental_summary = s.fund.summary_line;
            pos.fundamental_grade = s.fund.quality_grade;

            pos.sentiment_score = s.sent.sentiment_score;
            pos.sentiment_label = s.sent.sentiment_label;
            pos.sentiment_bar = s.sent.sentiment_bar;
            pos.macro_catalyst = s.sent.macro_catalyst;
            pos.macro_risk = s.sent.macro_risk;
            for (const auto& h : s.sent.headlines) {
                pos.headlines.push_back(std::format("\"{}\" ({}, {:+.2f})", h.title, h.publisher, h.polarity_score));
            }

            if (is_crisis) {
                pos.recommendation = I18n::tr("KADEMELİ DİP TOPLAMA (DCA)", "STAGED DCA ACCUMULATION", "STUFENWEISER DCA-KAUF");
            } else if (s.fc_63d.quant_score >= 0.25 && s.fund.fundamental_score >= 0.15) {
                pos.recommendation = I18n::tr("GÜÇLÜ AL / ANA ÇEKİRDEK", "STRONG BUY / CORE HOLDING", "STARKER KAUF / KERNPOSITION");
            } else {
                pos.recommendation = I18n::tr("KADEMELİ AL / PORTFÖY DENGESİ", "ACCUMULATE / BALANCED WEIGHT", "AKKUMULIEREN");
            }

            pos.rationale = I18n::tr(
                std::format("Yıllık Vol: %{:.1f} │ 3-Ay Quant Skor: {:+.2f} │ {}. Ters-volatilite ve bilanço kalitesi çarpanıyla %{:.1f} ağırlık atandı.",
                            eq_vols(i) * 100.0, s.fc_63d.quant_score,
                            s.fund.is_available ? ("Temel Not: " + s.fund.quality_grade) : "Endeks Çekirdeği",
                            pos.weight_pct),
                std::format("Annual Vol: {:.1f}% │ 3M Quant Score: {:+.2f} │ {}. Assigned {:.1f}% via inverse-vol & fundamental quality tilt.",
                            eq_vols(i) * 100.0, s.fc_63d.quant_score,
                            s.fund.is_available ? ("Fund. Grade: " + s.fund.quality_grade) : "Core Basket",
                            pos.weight_pct),
                std::format("Volatilität: {:.1f}% │ 3M-Score: {:+.2f} │ Gewicht {:.1f}% nach Inverse-Vol & Bilanzqualität.",
                            eq_vols(i) * 100.0, s.fc_63d.quant_score, pos.weight_pct)
            );

            plan.positions.push_back(std::move(pos));
        }

        // Compute Exact Portfolio Volatility via Eigen Empirical Correlation Matrix
        double port_vol = 0.02;
        if (m > 0) {
            Eigen::MatrixXd corr = Eigen::MatrixXd::Identity(m, m);
            for (int i = 0; i < m; ++i) {
                for (int j = i + 1; j < m; ++j) {
                    double rho = empirical_pair_correlation_(slots[static_cast<size_t>(i)].data,
                                                             slots[static_cast<size_t>(j)].data);
                    corr(i, j) = rho;
                    corr(j, i) = rho;
                }
            }
            port_vol = PortfolioLinearAlgebra::calculate_portfolio_volatility(corr, eq_vols, eq_weights);
        }

        plan.expected_annual_return_pct = (cash_share * rf_rate + weighted_eq_ann_ret) * 100.0;
        plan.expected_volatility_pct = std::max(1.5, port_vol * 100.0);
        plan.max_drawdown_risk_pct = std::max(2.0, 1.65 * plan.expected_volatility_pct);

        if (is_crisis) {
            plan.macro_verdict = I18n::tr(
                std::format("SAVUNMACI STRES REJİMİ (Canlı Endeks 20G: %{:+.1f}, Vol: %{:.1f}): Endeksteki gerçek geri çekilme ve yüksek oynaklık sebebiyle sermayenin %{:.0f}'i risksiz getiri kalkanında tutulmuş, kalan %{:.0f} düşük borçlu / yüksek FAVÖK kaliteli hisselerde kademeli (DCA) alıma ayrılmıştır.",
                            plan.macro_sentiment.benchmark_return_20d_pct, plan.macro_sentiment.benchmark_volatility_pct,
                            cash_share * 100.0, eq_budget_share * 100.0),
                std::format("DEFENSIVE STRESS REGIME (Live Index 20D: {:+.1f}%, Vol: {:.1f}%): Due to real benchmark stress, {:.0f}% of capital is parked in risk-free yield while {:.0f}% targets high-EBITDA-quality equities via staged DCA.",
                            plan.macro_sentiment.benchmark_return_20d_pct, plan.macro_sentiment.benchmark_volatility_pct,
                            cash_share * 100.0, eq_budget_share * 100.0),
                std::format("DEFENSIVES STRESS-REGIME (20T Index: {:+.1f}%): {:.0f}% Zinspuffer und {:.0f}% stufenweiser Einstieg in Qualitätsaktien.",
                            plan.macro_sentiment.benchmark_return_20d_pct, cash_share * 100.0, eq_budget_share * 100.0)
            );
        } else {
            plan.macro_verdict = I18n::tr(
                std::format("AKTİF FAKTÖR TAHSİS REJİMİ (Canlı Endeks 20G: %{:+.1f}, Vol: %{:.1f}): Portföyün %{:.0f}'i (~%{:.2f} yıllık getiri) likidite tamponunda, %{:.0f}'i ise Eigen kovaryans risk dengesi ve kur-uyumlu FAVÖK (FD/FAVÖK, Net Borç/FAVÖK, ROE) kalitesine göre optimize edilen çekirdek hisselerde konumlandırılmıştır.",
                            plan.macro_sentiment.benchmark_return_20d_pct, plan.macro_sentiment.benchmark_volatility_pct,
                            cash_share * 100.0, rf_rate * 100.0, eq_budget_share * 100.0),
                std::format("ACTIVE FACTOR ALLOCATION REGIME (Live Index 20D: {:+.1f}%, Vol: {:.1f}%): {:.0f}% allocated to risk-free buffer (~{:.2f}% yield) and {:.0f}% optimized across core equities via Eigen covariance & FX-aligned EBITDA quality.",
                            plan.macro_sentiment.benchmark_return_20d_pct, plan.macro_sentiment.benchmark_volatility_pct,
                            cash_share * 100.0, rf_rate * 100.0, eq_budget_share * 100.0),
                std::format("AKTIVES FAKTOR-REGIME (20T Index: {:+.1f}%): {:.0f}% Zinspuffer (~{:.2f}%) und {:.0f}% Eigen-Kovarianz- & EBITDA-optimierte Aktien.",
                            plan.macro_sentiment.benchmark_return_20d_pct, cash_share * 100.0, rf_rate * 100.0, eq_budget_share * 100.0)
            );
        }

        plan.actionable_steps = {
            I18n::tr("1. ADIM (KADEMELİ GİRİŞ): Hesaplanan lot adetlerini tek emirde değil, 22-günlük VWAP etrafında 2-3 parçaya bölerek gerçekleştirin.",
                     "STEP 1 (STAGED EXECUTION): Execute share counts in 2-3 tranches around the 22-day institutional VWAP rather than a single market order.",
                     "SCHRITT 1 (GESTAFFELTE ORDER): Stückzahlen in 2-3 Tranchen nahe dem 22-Tage-VWAP ausführen."),
            I18n::tr(std::format("2. ADIM (NAKİT NEMALANDIRMA): Portföyün %{:.0f} nakit/tahvil payını boşta bırakmayıp günlük bileşik getiri (~%{:.2f}/yıl) üreten enstrümanda tutun.",
                                 cash_share * 100.0, rf_rate * 100.0),
                     std::format("STEP 2 (CASH YIELD): Keep the {:.0f}% cash buffer actively earning ~{:.2f}% annual risk-free yield.",
                                 cash_share * 100.0, rf_rate * 100.0),
                     std::format("SCHRITT 2 (ZINSPUFFER): {:.0f}% Liquidität aktiv zu ~{:.2f}% p.a. verzinsen.",
                                 cash_share * 100.0, rf_rate * 100.0)),
            I18n::tr("3. ADIM (ATR ZARAR-KES DİSİPLİNİ): Her kartta verilen 14-günlük OHLC ATR tabanlı Stop-Loss altında günlük kapanış gelirse pozisyonu koruma amaçlı azaltın.",
                     "STEP 3 (ATR STOP DISCIPLINE): Respect the 14-day OHLC ATR Stop-Loss on daily closes to cap portfolio drawdown.",
                     "SCHRITT 3 (ATR-STOP-DISZIPLIN): 14-Tage-ATR-Stop-Loss auf Tagesschlusskursbasis strikt einhalten."),
            I18n::tr("4. ADIM (BİLANÇO TAKİBİ): Çeyreklik bilançolarda Net Borç/FAVÖK oranının 3.5x üzerine çıktığı veya FAVÖK marjının sert bozulduğu hisselerin ağırlığını düşürün.",
                     "STEP 4 (BALANCE SHEET MONITOR): Trim positions if quarterly Net Debt/EBITDA breaches 3.5x or EBITDA margins structurally erode.",
                     "SCHRITT 4 (BILANZKONTROLLE): Gewichtung reduzieren, falls Nettoschuld/EBITDA über 3,5x steigt.")
        };
    }

    static double empirical_pair_correlation_(const RealTimeAssetData& a, const RealTimeAssetData& b) {
        auto aligned = LiveMarketDataClient::align_series_by_date(a, b, false);
        if (aligned.prices_a.size() < 15) return 0.45; // Conservative equity correlation fallback

        std::vector<double> ra, rb;
        for (size_t i = 1; i < aligned.prices_a.size(); ++i) {
            if (aligned.prices_a[i - 1] > 0.0 && aligned.prices_b[i - 1] > 0.0) {
                ra.push_back(std::log(aligned.prices_a[i] / aligned.prices_a[i - 1]));
                rb.push_back(std::log(aligned.prices_b[i] / aligned.prices_b[i - 1]));
            }
        }
        if (ra.size() < 10) return 0.45;

        double ma = std::accumulate(ra.begin(), ra.end(), 0.0) / ra.size();
        double mb = std::accumulate(rb.begin(), rb.end(), 0.0) / rb.size();
        double cov = 0.0, va = 0.0, vb = 0.0;
        for (size_t i = 0; i < ra.size(); ++i) {
            double da = ra[i] - ma;
            double db = rb[i] - mb;
            cov += da * db;
            va += da * da;
            vb += db * db;
        }
        if (va <= 1e-12 || vb <= 1e-12) return 0.45;
        return std::clamp(cov / std::sqrt(va * vb), -0.85, 0.98);
    }

    static void print_wrapped_(const std::string& text, int width, const std::string& indent,
                               const std::string& color_code, const std::string& reset_code) {
        std::istringstream words(text);
        std::string word;
        std::string line = indent;
        int indent_cols = ThemeEngine::utf8_visible_length(indent);
        while (words >> word) {
            int next_len = ThemeEngine::utf8_visible_length(line) + ThemeEngine::utf8_visible_length(word) + 1;
            if (next_len > width && line != indent) {
                std::cout << color_code << line << reset_code << "\n";
                line = std::string(static_cast<size_t>(indent_cols), ' ') + word;
            } else {
                if (line != indent) line += " ";
                line += word;
            }
        }
        if (!line.empty() && line != indent) {
            std::cout << color_code << line << reset_code << "\n";
        }
    }

    static std::string make_gauge_(double pct, int width = 14) {
        int filled = std::clamp(static_cast<int>(std::round((pct / 100.0) * width)), 0, width);
        std::string s = "[";
        for (int i = 0; i < filled; ++i) s += "█";
        for (int i = filled; i < width; ++i) s += "░";
        s += "]";
        return s;
    }
};

} // namespace quant
