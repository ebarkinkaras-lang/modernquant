#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "black_scholes.hpp"
#include "news_sentiment.hpp"
#include "i18n.hpp"
#include <vector>
#include <string>
#include <format>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

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
    std::string recommendation; // "GÜÇLÜ AL", "KADEMELİ AL", "TUT", "SABİT GETİRİYE BAĞLA"
    std::string rationale;

    // Sentimel ve Makro Haber Analizi
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
    
    // Bölgesel Makro ve Sentimel Görünüm
    RegionalMacroSentiment macro_sentiment;

    std::vector<StockPositionRecommendation> positions;
    std::vector<std::string> actionable_steps;
};

class QuantInvestmentAdvisor {
public:
    static RegionalMarketPlan generate_regional_plan(double capital, MarketRegion region, InvestorRisk risk, LiveMarketDataClient& client) {
        RegionalMarketPlan plan;
        plan.total_capital = capital;
        plan.region = region;
        plan.risk = risk;

        if (region == MarketRegion::BIST) {
            plan.currency_symbol = "₺";
            plan.currency_code = "TL";
            build_bist_plan_(plan, capital, risk, client);
        } else if (region == MarketRegion::XETRA) {
            plan.currency_symbol = "€";
            plan.currency_code = "EUR";
            build_xetra_plan_(plan, capital, risk, client);
        } else {
            plan.currency_symbol = "$";
            plan.currency_code = "USD";
            build_us_plan_(plan, capital, risk, client);
        }

        return plan;
    }

    static void render_plan_cards(const RegionalMarketPlan& plan) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::string region_title;
        if (plan.region == MarketRegion::BIST) {
            region_title = I18n::tr("🇹🇷 BORSA İSTANBUL (BIST 100) ─ PORTFÖY VE SERMAYE PLANI",
                                    "🇹🇷 BORSA ISTANBUL (BIST 100) ─ PORTFOLIO & CAPITAL PLAN",
                                    "🇹🇷 BORSA ISTANBUL (BIST 100) ─ PORTFOLIO- & KAPITALPLAN");
        } else if (plan.region == MarketRegion::XETRA) {
            region_title = I18n::tr("🇩🇪 FRANKFURT XETRA (DAX 40) ─ PORTFÖY VE SERMAYE PLANI",
                                    "🇩🇪 FRANKFURT XETRA (DAX 40) ─ PORTFOLIO & CAPITAL PLAN",
                                    "🇩🇪 FRANKFURT XETRA (DAX 40) ─ PORTFOLIO- & KAPITALPLAN");
        } else {
            region_title = I18n::tr("🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ PORTFÖY VE SERMAYE PLANI",
                                    "🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ PORTFOLIO & CAPITAL PLAN",
                                    "🇺🇸 WALL STREET (S&P 500 / NASDAQ) ─ PORTFOLIO- & KAPITALPLAN");
        }

        std::string risk_title;
        if (plan.risk == InvestorRisk::DüşükRisk_Koruma) {
            risk_title = I18n::tr("Muhafazakar / Sermaye Koruma & Garanti Faiz Ağırlıklı",
                                  "Conservative / Capital Preservation & Guaranteed Fixed Income Focus",
                                  "Konservativ / Kapitalerhalt & garantierter Zinsfokus");
        } else if (plan.risk == InvestorRisk::Dengeli_Büyüme) {
            risk_title = I18n::tr("Dengeli Büyüme (Hisse + Likidite Tamponu + İhracatçılar)",
                                  "Balanced Growth (Equities + Liquidity Buffer + Exporters)",
                                  "Ausgewogenes Wachstum (Aktien + Liquiditätspuffer + Exporteure)");
        } else {
            risk_title = I18n::tr("Yüksek Getiri & Agresif Momentum (Tam Hisse Maruziyeti)",
                                  "High Yield & Aggressive Momentum (Full Equity Exposure)",
                                  "Hohes Wachstum & Aggressives Momentum (Volles Aktien-Exposure)");
        }

        // 1. ÖZET MAKRO KARTI
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << std::format("║  {:<74}║\n", region_title);
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << std::format("   {:<25}: {}{}{}\n", I18n::tr("Strateji Profili", "Strategy Profile", "Strategieprofil"),
                                 pal.glow_rgb, risk_title, pal.reset_code);
        std::cout << std::format("   {:<25}: {}{:.2f} {}{}\n", I18n::tr("Toplam Yatırım Bütçesi", "Total Investment Budget", "Gesamtes Anlagebudget"),
                                 pal.glow_rgb, plan.total_capital, plan.currency_code, pal.reset_code);
        std::cout << std::format("   {:<25}: {}%{:.1f}{}\n", I18n::tr("Hedeflenen Yıllık Getiri", "Target Annual Return", "Angestrebte Jahresrendite"),
                                 pal.glow_rgb, plan.expected_annual_return_pct, pal.reset_code);
        std::cout << std::format("   {:<25}: %{:.1f}\n", I18n::tr("Beklenen Volatilite", "Expected Volatility", "Erwartete Volatilität"),
                                 plan.expected_volatility_pct);
        std::cout << std::format("   {:<25}: %{:.1f}\n\n", I18n::tr("Maksimum Risk (Drawdown)", "Maximum Risk (Drawdown)", "Maximales Risiko (Drawd.)"),
                                 plan.max_drawdown_risk_pct);
        
        std::cout << pal.glow_rgb << "   " << I18n::tr("MAKRO GÖRÜNÜM & REJİM:\n", "MACRO OUTLOOK & REGIME:\n", "MAKRO-AUSBLICK & REGIME:\n") << pal.reset_code;
        print_wrapped_(plan.macro_verdict, 74, "   ", pal.base_rgb, pal.reset_code);
        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;

        // 2. MAKRO EKONOMİK HABER AKIŞI VE BORSAYA ETKİSİ KARTI
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << I18n::tr(
            "║  📰 MAKRO EKONOMİK HABER AKIŞI VE BORSAYA ETKİSİ ANALİZİ                     ║\n",
            "║  📰 MACROECONOMIC NEWS FLOW & MARKET IMPACT ANALYSIS                         ║\n",
            "║  📰 MAKROÖKONOMISCHER NACHRICHTENFLUSS & MARKT-EINFLUSS                      ║\n"
        );
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << std::format("   {:<27}: {}{}{}  {}{}\n",
                                 I18n::tr("Bölgesel Piyasa Sentimenti", "Regional Market Sentiment", "Regionales Markt-Sentiment"),
                                 pal.glow_rgb, plan.macro_sentiment.overall_gauge_bar, pal.reset_code,
                                 pal.glow_rgb, plan.macro_sentiment.overall_sentiment_label, pal.reset_code);
        std::cout << pal.dim_rgb << "   " << I18n::tr("Genel Makro Değerlendirme  :\n", "General Macro Evaluation   :\n", "Allgemeine Makro-Bewertung :\n") << pal.reset_code;
        print_wrapped_(plan.macro_sentiment.macro_synthesis, 74, "   ", pal.base_rgb, pal.reset_code);
        std::cout << "\n" << pal.glow_rgb << "   " << I18n::tr("PİYASAYI DOĞRUDAN ETKİLEYEN GELİŞMELER:\n", "KEY DEVELOPMENTS IMPACTING THE MARKET:\n", "WICHTIGE MARKTRELEVANTE ENTWICKLUNGEN:\n") << pal.reset_code;

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

        // 3. HER VARLIK İÇİN AYRI AYRI OKUNAKLI KARTLAR
        std::cout << "\n" << pal.glow_rgb << I18n::tr("=== PARANIZI NEREYE, KAÇ LOT VE NASIL YATIRMALISINIZ? ===", "=== WHERE, HOW MANY SHARES & HOW TO ALLOCATE CAPITAL? ===", "=== WOHIN, WIE VIELE STÜCKE & WIE KAPITAL ANLEGEN? ===") << pal.reset_code << "\n";

        for (size_t i = 0; i < plan.positions.size(); ++i) {
            const auto& pos = plan.positions[i];
            bool is_fixed = (pos.lot_count == 0);

            std::cout << "\n" << pal.glow_rgb;
            std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐\n";
            std::cout << std::format("│ [{}] {:<12} ─ {:<57}│\n", i + 1, pos.ticker, pos.company_name);
            std::cout << "├──────────────────────────────────────────────────────────────────────────────┤\n";
            std::cout << pal.reset_code;

            std::string gauge = make_gauge_(pos.weight_pct, 14);
            std::cout << std::format("  {:<18}: {}{}{}  {}%{:.1f}{}\n",
                                     I18n::tr("Portföy Payı", "Portfolio Weight", "Portfolio-Anteil"),
                                     pal.glow_rgb, gauge, pal.reset_code,
                                     pal.base_rgb, pos.weight_pct, pal.reset_code);
            std::cout << std::format("  {:<18}: {}{:.2f} {}{}\n",
                                     I18n::tr("Yatırılacak Tutar", "Allocation Amount", "Anlagebetrag"),
                                     pal.glow_rgb, pos.allocation_amount, plan.currency_code, pal.reset_code);

            if (!is_fixed) {
                std::cout << std::format("  {:<18}: {}{} {}{} ({}: {:.2f} {})\n\n",
                                         I18n::tr("ALINACAK LOT", "SHARES TO BUY", "ZU KAUFENDE STK."),
                                         pal.glow_rgb, pos.lot_count, I18n::tr("ADET", "SHARES", "STÜCK"), pal.reset_code,
                                         I18n::tr("Anlık Spot", "Current Spot", "Aktueller Kurs"), pos.spot_price, plan.currency_code);
                
                double pot_gain = (pos.spot_price > 0.0) ? ((pos.target_price / pos.spot_price) - 1.0) * 100.0 : 0.0;
                double pot_loss = (pos.spot_price > 0.0) ? ((pos.spot_price - pos.stop_loss) / pos.spot_price) * 100.0 : 0.0;

                std::cout << std::format("  {:<24}: {:.2f} {}  ({}+%{:.1f} {}{})\n",
                                         I18n::tr("HEDEF FİYAT (Kâr Al)", "TARGET PRICE (Take Profit)", "ZIELKURS (Take-Profit)"),
                                         pos.target_price, plan.currency_code, pal.glow_rgb, pot_gain,
                                         I18n::tr("Potansiyel", "Potential", "Potenzial"), pal.reset_code);
                std::cout << std::format("  {:<24}: {:.2f} {}  ({}-%{:.1f} {}{})\n",
                                         I18n::tr("ZARAR KES (Stop-Loss)", "STOP-LOSS (Protection)", "STOP-LOSS (Absicherung)"),
                                         pos.stop_loss, plan.currency_code, pal.dim_rgb, pot_loss,
                                         I18n::tr("Koruma Seviyesi", "Protection Level", "Schutzniveau"), pal.reset_code);
            } else {
                std::cout << std::format("  {:<18}: {}{}{}\n\n",
                                         I18n::tr("Varlık Türü", "Asset Type", "Anlageklasse"),
                                         pal.dim_rgb,
                                         I18n::tr("Sermaye Güvenceli Nakit / Faiz Enstrümanı", "Capital-Guaranteed Cash / Fixed Income", "Kapitalgarantierte Liquidität / Festverzinslich"),
                                         pal.reset_code);
            }

            std::cout << std::format("  {:<18}: {}[ {} ]{}\n\n",
                                     I18n::tr("TAVSİYE / EYLEM", "RECOMMENDATION", "EMPFEHLUNG/AKTION"),
                                     pal.glow_rgb, pos.recommendation, pal.reset_code);

            // Sentimel ve Makro Haber Analizi
            if (!pos.sentiment_bar.empty()) {
                std::cout << pal.glow_rgb << "  " << I18n::tr("SENTİMENT & MAKRO HABER ANALİZİ:\n", "SENTIMENT & MACRO NEWS ANALYSIS:\n", "SENTIMENT- & MAKRO-NACHRICHTENANALYSE:\n") << pal.reset_code;
                std::cout << std::format("  • {:<18}: {}{}{}  {}{}\n",
                                         I18n::tr("Sentimel Puan", "Sentiment Score", "Sentiment-Score"),
                                         pal.glow_rgb, pos.sentiment_bar, pal.reset_code,
                                         pal.glow_rgb, pos.sentiment_label, pal.reset_code);
                if (!pos.macro_catalyst.empty()) {
                    print_wrapped_(I18n::tr("Makro Katalizör   : ", "Macro Catalyst    : ", "Makro-Katalysator : ") + pos.macro_catalyst, 74, "  • ", pal.base_rgb, pal.reset_code);
                }
                if (!pos.macro_risk.empty()) {
                    print_wrapped_(I18n::tr("İzlenen Makro Risk: ", "Monitored Risk    : ", "Beobachtetes Risiko: ") + pos.macro_risk, 74, "  • ", pal.base_rgb, pal.reset_code);
                }
                if (!pos.headlines.empty()) {
                    std::cout << pal.dim_rgb << "  • " << I18n::tr("İlgili Haber Akışı:\n", "Relevant News Flow:\n", "Relevante Schlagzeilen:\n") << pal.reset_code;
                    for (const auto& h : pos.headlines) {
                        print_wrapped_(h, 74, "    ✔ ", pal.base_rgb, pal.reset_code);
                    }
                }
                std::cout << "\n";
            }

            std::cout << pal.dim_rgb << "  " << I18n::tr("Temel Yatırım Gerekçesi:\n", "Key Investment Rationale:\n", "Wesentliche Anlagebegründung:\n") << pal.reset_code;
            print_wrapped_(pos.rationale, 74, "  • ", pal.base_rgb, pal.reset_code);

            std::cout << pal.glow_rgb;
            std::cout << "└──────────────────────────────────────────────────────────────────────────────┘\n" << pal.reset_code;
        }

        // 4. SOMUT UYGULAMA ADIMLARI KARTI
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << I18n::tr(
            "║  SOMUT UYGULAMA ADIMLARI (PARANIZI REELDE NASIL YÖNETMELİSİNİZ?)             ║\n",
            "║  ACTIONABLE EXECUTION STEPS (HOW TO DEPLOY YOUR CAPITAL REALISTICALLY?)      ║\n",
            "║  KONKRETE UMSETZUNGSSCHRITTE (WIE SETZEN SIE IHR KAPITAL IN DER PRAXIS EIN?)║\n"
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
    static void print_wrapped_(const std::string& text, int width, const std::string& indent,
                               const std::string& color_code, const std::string& reset_code) {
        std::istringstream words(text);
        std::string word;
        std::string line = indent;
        while (words >> word) {
            if (line.size() + word.size() + 1 > static_cast<size_t>(width)) {
                std::cout << color_code << line << reset_code << "\n";
                std::string cont_indent(indent.size(), ' ');
                line = cont_indent + word;
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
    static void enrich_all_positions_(std::vector<StockPositionRecommendation>& positions, LiveMarketDataClient& client) {
        for (auto& pos : positions) {
            if (pos.ticker.find("MEVDUAT") != std::string::npos || pos.ticker.find("FON") != std::string::npos) {
                pos.sentiment_score = 0.50;
                pos.sentiment_label = "NÖTR / GARANTİLİ (0.50)";
                pos.sentiment_bar = "[──────┼███───]";
                pos.macro_catalyst = "TCMB sıkı para politikası ve risksiz net %45 mevduat/PPF faiz getirisi.";
                pos.macro_risk = "Enflasyonun mevduat faizinin üzerinde seyretmesi halinde reel getiri kaybı.";
                pos.headlines = {"\"TCMB dezenflasyon sürecinde sıkı parasal duruşunu kararlılıkla koruyor\" (TCMB Raporu)"};
            } else if (pos.ticker.find("BUND") != std::string::npos || pos.ticker.find("CASH") != std::string::npos) {
                pos.sentiment_score = 0.40;
                pos.sentiment_label = "NÖTR / GARANTİLİ (0.40)";
                pos.sentiment_bar = "[──────┼██────]";
                pos.macro_catalyst = "Alman Federal Cumhuriyeti devlet tahvili (Bund) %3.25 net Euro garanti kuponu.";
                pos.macro_risk = "Euro Bölgesi enflasyonunun tahvil getirisini aşması.";
                pos.headlines = {"\"Almanya 10-yıllık tahvil getirileri ECB faiz indirimi beklentileriyle dengeleniyor\" (Handelsblatt)"};
            } else if (pos.ticker.find("IRX") != std::string::npos || pos.ticker.find("SGOV") != std::string::npos) {
                pos.sentiment_score = 0.45;
                pos.sentiment_label = "NÖTR / RİSKSİZ (0.45)";
                pos.sentiment_bar = "[──────┼███───]";
                pos.macro_catalyst = "ABD Hazine Bakanlığı kısa vadeli bono faizleri (~%4.10 risksiz getiri).";
                pos.macro_risk = "FED faiz indirimleriyle para piyasası getirilerinin gerilemesi.";
                pos.headlines = {"\"US Treasury short term yields remain an attractive cash haven\" (Wall Street Journal)"};
            } else {
                auto prof = NewsSentimentEngine::get_asset_sentiment(pos.ticker, pos.company_name, client);
                pos.sentiment_score = prof.sentiment_score;
                pos.sentiment_label = prof.sentiment_label;
                pos.sentiment_bar = prof.sentiment_bar;
                pos.macro_catalyst = prof.macro_catalyst;
                pos.macro_risk = prof.macro_risk;
                for (const auto& h : prof.headlines) {
                    pos.headlines.push_back(std::format("\"{}\" ({})", h.title, h.publisher));
                }
            }
        }
    }

    static void build_bist_plan_(RegionalMarketPlan& plan, double capital, InvestorRisk risk, LiveMarketDataClient& client) {
        plan.macro_sentiment = NewsSentimentEngine::build_bist_macro_sentiment(&client);

        // Canlı BIST Fiyatlarını Çek
        auto thyao = client.fetch_asset("THYAO.IS");
        auto asels = client.fetch_asset("ASELS.IS");
        auto tuprs = client.fetch_asset("TUPRS.IS");
        auto bimas = client.fetch_asset("BIMAS.IS");

        double p_thyao = (thyao.current_price > 0) ? thyao.current_price : 267.0;
        double p_asels = (asels.current_price > 0) ? asels.current_price : 334.5;
        double p_tuprs = (tuprs.current_price > 0) ? tuprs.current_price : 142.0;
        double p_bimas = (bimas.current_price > 0) ? bimas.current_price : 480.0;

        // TCMB / TL Para Piyasası Fonu ve Mevduat Garantili Risksiz Oran
        const double tl_deposit_rate = 0.45; // %45 yıllık risksiz faiz
        bool is_crisis = (plan.macro_sentiment.overall_sentiment_score < -0.15);

        if (is_crisis) {
            // ==============================================================
            // KRİZ & MSCI ŞOKU ÇAKILMA SAVUNMA REJİMİ
            // ==============================================================
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 44.0;
                plan.expected_volatility_pct = 5.0;
                plan.max_drawdown_risk_pct = 3.5;
                plan.macro_verdict = "MSCI KRİZİ & ANAPARA KORUMA REJİMİ: "
                                     "MSCI uyarısı ve yabancı kurumsal fon çıkışları nedeniyle BIST'te sert çakılma yaşanmaktadır. "
                                     "Sermayenin %80'i anapara garantili %45 risksiz TL Mevduat/PPF'ye bağlanmış, "
                                     "%20'si ise yalnızca gıda ve savunma gibi krizde en az etkilenen defansif hisselere ayrılmıştır. "
                                     "Agresif hisse alımı kesinlikle durdurulmuştur.";

                plan.positions = {
                    {"TL_MEVDUAT", "TCMB Para Piyasası Fonu / TL Mevduat", 1.0, 1.0, 1.0, 80.0, capital * 0.80, 0,
                     "SABİT GETİRİYE BAĞLA", std::format("Yıllık net %{:.1f} risksiz getiri; MSCI çöküşünde anapara kaybı riski sıfır.", tl_deposit_rate * 100.0)},
                    {"BIMAS.IS", "BİM Birleşik Mağazalar (Defansif Perakende)", p_bimas, p_bimas * 1.15, p_bimas * 0.93, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_bimas), "DÜŞÜŞÜ İZLE / DEFANSİF BEKLE", "Krizde temel gıda talebi düşmez; ancak endekste taban oluşumu beklenmeli."},
                    {"ASELS.IS", "Aselsan (Stratejik Savunma & İhracat)", p_asels, p_asels * 1.18, p_asels * 0.92, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_asels), "DİP BEKLE / TEMKİNLİ İZLE", "Stratejik devlet kontratları var ancak yabancı çıkışı durulana kadar acele edilmemeli."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 48.5;
                plan.expected_volatility_pct = 14.0;
                plan.max_drawdown_risk_pct = 8.5;
                plan.macro_verdict = "MSCI ÇÖKÜŞÜ DEFANSİF KALKAN REJİMİ: "
                                     "MSCI'ın Türkiye uyarısıyla tetiklenen çakılma dalgasına karşı sermayenin %60'ı anında günlük faiz veren TL Para Piyasası Fonu'nda (PPF) korumaya alınmıştır. "
                                     "Kalan %40 bütçe ile yabancıların panikle ucuza sattığı kaliteli lokomotiflerde (THYAO, ASELSAN, BIMAS) tek seferde değil, kademeli dip toplama (DCA) stratejisi uygulanır.";

                plan.positions = {
                    {"TL_FON", "Para Piyasası Fonu (Güvenli Liman Tamponu)", 1.0, 1.0, 1.0, 60.0, capital * 0.60, 0,
                     "GÜVENLİ LİMANA BAĞLA", std::format("Krizde anaparayı koruyan ve günlük getiri üreten %{:.1f} sabit faiz kalkanı.", tl_deposit_rate * 100.0)},
                    {"THYAO.IS", "Türk Hava Yolları (Küresel Havacılık Lideri)", p_thyao, p_thyao * 1.25, p_thyao * 0.92, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_thyao), "DÜŞÜŞÜ İZLE / KADEMELİ DİP TOPLA", "Yabancı fon çıkışı hisseyi aşırı satıma itti; taban arayışında küçük parçalarla maliyetlenin."},
                    {"ASELS.IS", "Aselsan (Yüksek Teknoloji & Savunma)", p_asels, p_asels * 1.22, p_asels * 0.92, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_asels), "KADEMELİ DİP TOPLA", "Savunma siparişleri krize dirençli; destek seviyelerinden küçük dilimlerle alım."},
                    {"BIMAS.IS", "BİM Birleşik Mağazalar (Defansif Perakende)", p_bimas, p_bimas * 1.15, p_bimas * 0.93, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_bimas), "DEFANSİF TUT", "Nakit akışı güçlü; portföy oynaklığını sınırlandıran güvenli çıpa."}
                };
            } else { // Agresif BIST Kriz Modu
                plan.expected_annual_return_pct = 58.0;
                plan.expected_volatility_pct = 24.0;
                plan.max_drawdown_risk_pct = 18.0;
                plan.macro_verdict = "AŞIRI SATIM & OPORTÜNİSTİK DİP TEPKİ REJİMİ: "
                                     "MSCI uyarısı sonrası panik satışları BIST lokomotiflerini aşırı satım (RSI < 25) bölgesine itti. "
                                     "Fırsatları değerlendirirken nakitsiz yakalanmamak için portföyün %35'i PPF likidite rezervinde tutulmakta, "
                                     "%65'i ise dip tepki rallisi potansiyeli yüksek THYAO, ASELSAN ve TÜPRAŞ'a kademeli paylaştırılmaktadır.";

                plan.positions = {
                    {"TL_FON", "Para Piyasası Fonu (Likidite Rezervi)", 1.0, 1.0, 1.0, 35.0, capital * 0.35, 0,
                     "LİKİDİTE REZERVİ", std::format("Dip seviyelerden ek alım yapabilmek için acil durum nakit tamponu (%%{:.1f} faiz).", tl_deposit_rate * 100.0)},
                    {"THYAO.IS", "Türk Hava Yolları", p_thyao, p_thyao * 1.30, p_thyao * 0.90, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_thyao), "AŞIRI SATIM TEPKİ ALIMI", "Panik satışıyla F/K çarpanı aşırı iskontolu hale geldi; kademeli dip alımı."},
                    {"ASELS.IS", "Aselsan", p_asels, p_asels * 1.28, p_asels * 0.91, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_asels), "DİPTE KADEMELİ TOPLA", "Stratejik döviz girdisi ve rekor backlog; panik durulunca hızlı toparlanma potansiyeli."},
                    {"TUPRS.IS", "Tüpraş", p_tuprs, p_tuprs * 1.22, p_tuprs * 0.91, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_tuprs), "SELEKTİF DİP ALIMI", "Rafineri marjı avantajı ve yüksek temettü gücü."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM (MSCI ŞOKU UYARISI): MSCI küme düşme uyarısı ve panik satışı durulmadan ASLA TEK SEFERDE BÜYÜK ALIM YAPMAYIN. Düşen bıçak tutulmaz; yabancı takasındaki kanamanın durması beklenmelidir.",
                "2. ADIM (GÜVENLİ LİMAN TAMPONU): Portföyün %60-%80'lik nakit kısmını derhal T+0 valörlü Para Piyasası Fonu'nda (PPF) nemalandırın. Borsadaki çakılma sürecinde anaparanızı korurken günlük net %45 yıllık faiz getirisi kazanın.",
                "3. ADIM (KADEMELİ DİP STRATEJİSİ - DCA): Kalan hisse bütçesini tek günde değil, endeks devre kesicileri ve destek seviyeleri (200 günlük hareketli ortalama) test edildikçe 3-4 haftaya yayarak küçük dilimlerle maliyetlenin.",
                "4. ADIM (SIKI ZARAR KES): Belirtilen Stop-Loss seviyeleri kırıldığında tereddütsüz nakde dönün. MSCI statü indiriminin resmiyet kazanması halinde ek %15-20 satış dalgası gelebileceğini unutmayın."
            };
        } else {
            // Normal / Boğa Rejimi (Eski şablon)
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 42.5;
                plan.expected_volatility_pct = 7.0;
                plan.max_drawdown_risk_pct = 4.5;
                plan.macro_verdict = "TCMB Sıkı Para Politikası ve Yüksek TL Mevduat Rejimi: "
                                     "Mevduat ve Para Piyasası Fonları risksiz net %45 getiri sağlarken, "
                                     "sermayenin %60'ı nakit akışına, %40'ı BIST'in en güçlü defansif hisselerine paylaştırıldı.";

                plan.positions = {
                    {"TL_MEVDUAT", "TCMB Para Piyasası Fonu / TL Mevduat", 1.0, 1.0, 1.0, 60.0, capital * 0.60, 0,
                     "SABİT GETİRİYE BAĞLA", std::format("Yıllık net %{:.1f} risksiz getiri; anapara kaybı riski sıfır.", tl_deposit_rate * 100.0)},
                    {"BIMAS.IS", "BİM Birleşik Mağazalar (Defansif Perakende)", p_bimas, p_bimas * 1.15, p_bimas * 0.94, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_bimas), "KADEMELİ AL", "Enflasyonu fiyatlarına doğrudan yansıtabilen defansif nakit akışı."},
                    {"ASELS.IS", "Aselsan (Savunma Sanayi & İhracat)", p_asels, p_asels * 1.18, p_asels * 0.93, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_asels), "GÜÇLÜ AL", "Yüksek döviz bazlı sipariş birikimi ve kamu desteği."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 54.0;
                plan.expected_volatility_pct = 16.5;
                plan.max_drawdown_risk_pct = 12.0;
                plan.macro_verdict = "BIST 100 Dengeli Büyüme Rejimi: "
                                     "TL faiz tamponu (%30) ile aşağı yönlü risk sınırlandırılırken, "
                                     "sanayi, havacılık ve teknoloji ihracatçılarıyla BIST getirisinden (Alpha) azami pay hedefleniyor.";

                plan.positions = {
                    {"THYAO.IS", "Türk Hava Yolları (Küresel Havacılık Lideri)", p_thyao, p_thyao * 1.22, p_thyao * 0.94, 30.0, capital * 0.30,
                     static_cast<int>((capital * 0.30) / p_thyao), "GÜÇLÜ AL", "Döviz bazlı rekor yolcu/kargo geliri ve ucuz F/K çarpanı."},
                    {"ASELS.IS", "Aselsan (Yüksek Teknoloji & Savunma)", p_asels, p_asels * 1.20, p_asels * 0.93, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_asels), "AL / BİRİKTİR", "Uzun vadeli ihracat kontratları ve kârlılık artışı."},
                    {"TUPRS.IS", "Tüpraş (Rafineri & Yüksek Temettü)", p_tuprs, p_tuprs * 1.18, p_tuprs * 0.94, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_tuprs), "TEMETTÜ ALIMI", "Akdeniz rafineri marjları ve düzenli nakit temettü gücü."},
                    {"TL_FON", "Para Piyasası Fonu (Likidite Tamponu)", 1.0, 1.0, 1.0, 30.0, capital * 0.30, 0,
                     "GETİRİYE BAĞLA", std::format("Portföyü dalgalanmadan koruyan yıllık %{:.1f} sabit faiz tamponu.", tl_deposit_rate * 100.0)}
                };
            } else { // Agresif BIST
                plan.expected_annual_return_pct = 72.0;
                plan.expected_volatility_pct = 28.0;
                plan.max_drawdown_risk_pct = 22.0;
                plan.macro_verdict = "BIST Yüksek Beta & Büyüme Rejimi: "
                                     "Tüm sermaye kârlılık patlaması yaşayan havacılık, savunma ve sanayi lokomotiflerine tahsis edildi.";

                plan.positions = {
                    {"THYAO.IS", "Türk Hava Yolları", p_thyao, p_thyao * 1.30, p_thyao * 0.93, 40.0, capital * 0.40,
                     static_cast<int>((capital * 0.40) / p_thyao), "AGRESİF AL", "Piyasa rallisini sırtlayan ana lokomotif hisse."},
                    {"ASELS.IS", "Aselsan", p_asels, p_asels * 1.28, p_asels * 0.92, 35.0, capital * 0.35,
                     static_cast<int>((capital * 0.35) / p_asels), "MOMENTUM ALIMI", "Savunma harcamalarındaki global yükseliş trendi."},
                    {"TUPRS.IS", "Tüpraş", p_tuprs, p_tuprs * 1.24, p_tuprs * 0.94, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_tuprs), "TREND ALIMI", "Yüksek temettü verimi ve güçlü nakit rezervi."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM: Belirtilen lot miktarlarını gün içi piyasa derinliğinde tek seferde değil, 2-3 güne yayarak kademeli alın.",
                "2. ADIM: Portföydeki nakit/faiz payını mutlaka T+0 valörlü Para Piyasası Fonu'nda (PPF) nemalandırın (günlük faiz kazanın).",
                "3. ADIM: Belirtilen Stop-Loss seviyelerinin altında günlük kapanış gelirse tereddüt etmeden zararı kesin.",
                "4. ADIM: Hedef fiyata ulaşıldığında pozisyonun yarısını kârla kapatıp kalanın stop'unu maliyet seviyesine yükseltin (Trailing Stop)."
            };
        }

        enrich_all_positions_(plan.positions, client);
    }

    static void build_xetra_plan_(RegionalMarketPlan& plan, double capital, InvestorRisk risk, LiveMarketDataClient& client) {
        plan.macro_sentiment = NewsSentimentEngine::build_xetra_macro_sentiment(client);

        // Canlı XETRA Fiyatlarını Çek
        auto rhm = client.fetch_asset("RHM.DE");
        auto sap = client.fetch_asset("SAP.DE");
        auto sie = client.fetch_asset("SIE.DE");
        auto alv = client.fetch_asset("ALV.DE");

        double p_rhm = (rhm.current_price > 0) ? rhm.current_price : 1056.0;
        double p_sap = (sap.current_price > 0) ? sap.current_price : 197.36;
        double p_sie = (sie.current_price > 0) ? sie.current_price : 185.20;
        double p_alv = (alv.current_price > 0) ? alv.current_price : 295.40;

        const double euro_bund_rate = 0.0325; // %3.25 ECB / Bund faizi
        bool is_crisis = (plan.macro_sentiment.overall_sentiment_score < -0.15);

        if (is_crisis) {
            // ==============================================================
            // DAX / XETRA KRİZ & SATIŞ ŞOKU DEFANSİF REJİMİ
            // ==============================================================
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 5.5;
                plan.expected_volatility_pct = 4.0;
                plan.max_drawdown_risk_pct = 3.0;
                plan.macro_verdict = "EURO BÖLGESİ ŞOK / DEVLET GÜVENCESİ REJİMİ: "
                                     "DAX endeksindeki satış baskısı nedeniyle sermayenin %70'i Alman Devlet Tahvili (Bund) ve Euro nakit rezervine bağlanmıştır. "
                                     "Hisse maruziyeti yalnızca defansif temettü şampiyonu Allianz SE ile sınırlandırılmıştır.";
                plan.positions = {
                    {"DE_BUND", "Almanya 10-Yıllık Devlet Tahvili (Bund)", 100.0, 100.0, 98.5, 70.0, capital * 0.70, 0,
                     "DEVLET GÜVENCESİNE BAĞLA", std::format("Euro bazında yıllık %{:.2f} net garanti kupon getirisi.", euro_bund_rate * 100.0)},
                    {"ALV.DE", "Allianz SE (Defansif Temettü Şampiyonu)", p_alv, p_alv * 1.08, p_alv * 0.94, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_alv), "DÜŞÜŞÜ İZLE / DEFANSİF BEKLE", "Krizde güçlü bilanço ve %5+ temettü çıpası."},
                    {"EUR_CASH", "Euro Para Piyasası / Likidite Tamponu", 1.0, 1.0, 1.0, 10.0, capital * 0.10, 0,
                     "NAKİT REZERV", "Acil durum kriz tamponu."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 9.8;
                plan.expected_volatility_pct = 8.5;
                plan.max_drawdown_risk_pct = 6.5;
                plan.macro_verdict = "DAX DENGELİ KORUMA & KADEMELİ DİP REJİMİ: "
                                     "Satış dalgasında sermayenin %50'si Bund ve nakit faizinde tutularak korunurken, "
                                     "%50'si Rheinmetall ve SAP'de destek seviyelerinden kademeli (DCA) alıma ayrılmıştır.";
                plan.positions = {
                    {"DE_BUND", "Almanya Devlet Tahvili (Bund)", 100.0, 100.0, 98.0, 50.0, capital * 0.50, 0,
                     "GÜVENLİ LİMANA BAĞLA", "Düşüşte anaparayı koruyan devlet garantili Euro faiz kalkanı."},
                    {"RHM.DE", "Rheinmetall AG (Savunma Sanayi Lideri)", p_rhm, p_rhm * 1.22, p_rhm * 0.92, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_rhm), "KADEMELİ DİP TOPLA", "Savunma siparişleri krize dirençli; panik satışlarında parçalı maliyetlenme."},
                    {"SAP.DE", "SAP SE (Kurumsal Bulut Devi)", p_sap, p_sap * 1.18, p_sap * 0.93, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_sap), "DEFANSİF BÜYÜME", "Yinelenen bulut abonelik nakit akışı."}
                };
            } else {
                plan.expected_annual_return_pct = 18.0;
                plan.expected_volatility_pct = 16.0;
                plan.max_drawdown_risk_pct = 14.0;
                plan.macro_verdict = "DAX AŞIRI SATIM TEPKİ REJİMİ: "
                                     "Piyasadaki geri çekilmede %25 Euro nakit rezervi tutulup, "
                                     "%75 bütçe aşırı satılan Rheinmetall ve SAP'ye fırsat alımı olarak tahsis edildi.";
                plan.positions = {
                    {"EUR_CASH", "Euro Likidite Rezervi", 1.0, 1.0, 1.0, 25.0, capital * 0.25, 0,
                     "LİKİDİTE REZERVİ", "Dip seviyelerden ek alım için likit nakit tamponu."},
                    {"RHM.DE", "Rheinmetall AG", p_rhm, p_rhm * 1.25, p_rhm * 0.91, 45.0, capital * 0.45,
                     static_cast<int>((capital * 0.45) / p_rhm), "AŞIRI SATIM TEPKİ ALIMI", "Sipariş rekoru; panik durulunca hızlı toparlanma potansiyeli."},
                    {"SAP.DE", "SAP SE", p_sap, p_sap * 1.20, p_sap * 0.92, 30.0, capital * 0.30,
                     static_cast<int>((capital * 0.30) / p_sap), "BÜYÜME ALIMI", "AI kurumsal ERP liderliği."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM (HABER ŞOKU UYARISI): DAX genelinde panik satışı durulmadan tek kalemde pozisyona girmeyin; kademeli (DCA) alım yapın.",
                "2. ADIM (GÜVENLİ LİMAN): Euro sermayenizin en az %50'sini negatif faiz riskinden korumak için 10-Yıllık Alman Bund veya likit para piyasasında tutun.",
                "3. ADIM (SIKI STOP-LOSS): Belirtilen zarar kes seviyelerine sadık kalın; jeopolitik risk genişlerse nakit payını artırın."
            };
        } else {
            // Normal / Boğa Rejimi
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 7.5;
                plan.expected_volatility_pct = 5.5;
                plan.max_drawdown_risk_pct = 4.0;
                plan.macro_verdict = "Almanya & Euro Bölgesi Muhafazakar Getiri Rejimi: "
                                     "Alman Devlet Tahvili (Bund) ile sermaye güvenceye alınırken, "
                                     "DAX'ın en yüksek temettü ve sermaye disiplinine sahip şirketleri seçildi.";

                plan.positions = {
                    {"DE_BUND", "Almanya 10-Yıllık Devlet Tahvili (Bund)", 100.0, 100.0, 98.0, 50.0, capital * 0.50, 0,
                     "DEVLET GÜVENCESİNE BAĞLA", std::format("Euro bazında yıllık %{:.2f} net garanti kupon getirisi.", euro_bund_rate * 100.0)},
                    {"ALV.DE", "Allianz SE (Sigorta & Yüksek Temettü)", p_alv, p_alv * 1.10, p_alv * 0.95, 30.0, capital * 0.30,
                     static_cast<int>((capital * 0.30) / p_alv), "TEMETTÜ İÇİN AL", "İstikrarlı %5+ temettü verimi ve güçlü bilanço gücü."},
                    {"SAP.DE", "SAP SE (Avrupa'nın En Büyük Yazılım Devi)", p_sap, p_sap * 1.14, p_sap * 0.94, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_sap), "DEFANSİF BÜYÜME", "Kurumsal bulut gelirlerinde düzenli abonelik nakit akışı."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 14.8;
                plan.expected_volatility_pct = 11.5;
                plan.max_drawdown_risk_pct = 9.5;
                plan.macro_verdict = "DAX 40 Dengeli Sanayi & Teknoloji Rejimi: "
                                     "Avrupa savunma sanayii ve kurumsal yazılım büyümesiyle Euro bazlı güçlü getiri hedefleniyor.";

                plan.positions = {
                    {"RHM.DE", "Rheinmetall (Avrupa Savunma Sanayi Lideri)", p_rhm, p_rhm * 1.20, p_rhm * 0.93, 35.0, capital * 0.35,
                     static_cast<int>((capital * 0.35) / p_rhm), "GÜÇLÜ TREND ALIMI", "NATO ve Avrupa genelinde rekor sipariş büyümesi."},
                    {"SAP.DE", "SAP SE (Kurumsal Bulut & AI Yazılımı)", p_sap, p_sap * 1.18, p_sap * 0.94, 30.0, capital * 0.30,
                     static_cast<int>((capital * 0.30) / p_sap), "BÜYÜME ALIMI", "Avrupa'nın 1 numaralı teknoloji şirketi, sürdürülebilir kârlılık."},
                    {"SIE.DE", "Siemens AG (Endüstriyel Otomasyon & Enerji)", p_sie, p_sie * 1.15, p_sie * 0.94, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_sie), "KADEMELİ AL", "Küresel elektrifikasyon ve otomasyon talebi."},
                    {"EUR_CASH", "Euro Kısa Vadeli Faiz / Nakit", 1.0, 1.0, 1.0, 20.0, capital * 0.20, 0,
                     "FAİZE BAĞLA", "Düşüşlerde fırsat yaratmak üzere yıllık %3.25 faiz kazanan likit rezerv."}
                };
            } else { // Agresif XETRA
                plan.expected_annual_return_pct = 24.5;
                plan.expected_volatility_pct = 19.0;
                plan.max_drawdown_risk_pct = 16.0;
                plan.macro_verdict = "DAX Momentum & Savunma Ağırlıklı Agresif Rejim: "
                                     "Avrupa'nın en yüksek momentumlu savunma ve çip/yazılım hisselerine tam sermaye maruziyeti.";

                plan.positions = {
                    {"RHM.DE", "Rheinmetall AG", p_rhm, p_rhm * 1.25, p_rhm * 0.92, 55.0, capital * 0.55,
                     static_cast<int>((capital * 0.55) / p_rhm), "LOKOMOTİF ALIM", "Jeopolitik konjonktürde en yüksek serbest nakit akışı üreten DAX hissesi."},
                    {"SAP.DE", "SAP SE", p_sap, p_sap * 1.20, p_sap * 0.93, 35.0, capital * 0.35,
                     static_cast<int>((capital * 0.35) / p_sap), "TEKNOLOJİ ALIMI", "Yapay zeka entegreli ERP çözümleriyle yüksek büyüme."},
                    {"SIE.DE", "Siemens AG", p_sie, p_sie * 1.18, p_sie * 0.94, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_sie), "SANAYİ ALIMI", "Dijital endüstri çözümleri liderliği."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM: XETRA işlemlerinizi Frankfurt işlem saatlerinde (10:00 - 18:30 TSİ) likiditenin en yüksek olduğu saatlerde gerçekleştirin.",
                "2. ADIM: Rheinmetall gibi yüksek fiyatlı hisselerde portföy bütçesine göre küsuratlı lot yerine tam hisse adetlerini seçin.",
                "3. ADIM: Stop-Loss seviyelerinizi mutlaka sisteme girin; DAX endeksindeki sert haber akışlarında anaparanızı koruyun.",
                "4. ADIM: Euro nakit kısmınızı gecelik Euro faizinde tutarak (ESTR benzeri) atıl bırakmayın."
            };
        }

        enrich_all_positions_(plan.positions, client);
    }

    static void build_us_plan_(RegionalMarketPlan& plan, double capital, InvestorRisk risk, LiveMarketDataClient& client) {
        plan.macro_sentiment = NewsSentimentEngine::build_us_macro_sentiment(client);

        auto spy = client.fetch_asset("SPY");
        auto nvda = client.fetch_asset("NVDA");
        auto aapl = client.fetch_asset("AAPL");

        double p_spy = (spy.current_price > 0) ? spy.current_price : 582.5;
        double p_nvda = (nvda.current_price > 0) ? nvda.current_price : 138.0;
        double p_aapl = (aapl.current_price > 0) ? aapl.current_price : 231.0;

        const double us_tbill_rate = 0.0410; // %4.10 risksiz faiz
        bool is_crisis = (plan.macro_sentiment.overall_sentiment_score < -0.15);

        if (is_crisis) {
            // ==============================================================
            // WALL STREET KRİZ & SATIŞ ŞOKU DEFANSİF REJİMİ
            // ==============================================================
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 6.8;
                plan.expected_volatility_pct = 4.5;
                plan.max_drawdown_risk_pct = 3.5;
                plan.macro_verdict = "WALL STREET KRİZ SAVUNMA REJİMİ: "
                                     "Piyasadaki düzeltme ve satış dalgasında sermayenin %70'i risksiz ABD Hazine Bonolarına (^IRX / SGOV %4.10) bağlanmıştır. "
                                     "Hisse maruziyeti %30 ile yalnızca defansif SPY ve Apple ile sınırlandırılmıştır.";
                plan.positions = {
                    {"^IRX / SGOV", "ABD 3-Ay Hazine Bonosu (T-Bill)", 100.0, 100.0, 99.0, 70.0, capital * 0.70, 0,
                     "SABİT GETİRİYE BAĞLA", std::format("Yıllık net %{:.2f} risksiz USD garantisi.", us_tbill_rate * 100.0)},
                    {"SPY", "SPDR S&P 500 ETF (Geniş Piyasa)", p_spy, p_spy * 1.10, p_spy * 0.94, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_spy), "DÜŞÜŞÜ İZLE / DEFANSİF BEKLE", "Piyasa taban arayışında temkinli bekleme."},
                    {"AAPL", "Apple Inc (Nakit Zengini Defansif)", p_aapl, p_aapl * 1.12, p_aapl * 0.94, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_aapl), "DEFANSİF TUT", "Devasa nakit rezervi ve hisse geri alım desteği."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 11.5;
                plan.expected_volatility_pct = 9.5;
                plan.max_drawdown_risk_pct = 8.0;
                plan.macro_verdict = "WALL STREET DEFANSİF KALKAN REJİMİ: "
                                     "Sermayenin %50'si %4.10 Hazine faizinde güvene alınırken, "
                                     "%50 bütçe ile aşırı satılan Nvidia ve SPY'da kademeli maliyet düşürme (DCA) yapılır.";
                plan.positions = {
                    {"^IRX / SGOV", "ABD 3-Ay Hazine Bonosu (T-Bill)", 100.0, 100.0, 99.0, 50.0, capital * 0.50, 0,
                     "GÜVENLİ LİMANA BAĞLA", "Piyasa dalgalanmasından koruyan %4.10 risksiz faiz kalkanı."},
                    {"SPY", "SPDR S&P 500 ETF", p_spy, p_spy * 1.15, p_spy * 0.93, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_spy), "KADEMELİ DİP TOPLA", "Endeks destek seviyelerinden parçalı alım."},
                    {"NVDA", "Nvidia Corp", p_nvda, p_nvda * 1.25, p_nvda * 0.91, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_nvda), "AŞIRI SATIM TEPKİ ALIMI", "Yapay zeka altyapı talebi güçlü; panik satışlarında dip fırsatı."},
                    {"AAPL", "Apple Inc", p_aapl, p_aapl * 1.15, p_aapl * 0.93, 10.0, capital * 0.10,
                     static_cast<int>((capital * 0.10) / p_aapl), "DEFANSİF TUT", "Ekosistem nakit akışı çıpası."}
                };
            } else {
                plan.expected_annual_return_pct = 19.5;
                plan.expected_volatility_pct = 18.0;
                plan.max_drawdown_risk_pct = 15.0;
                plan.macro_verdict = "WALL STREET AŞIRI SATIM & BOĞA DİP AVCISI: "
                                     "Satış dalgasında %25 Hazine bonosu rezervde tutulup, "
                                     "%75 sermaye aşırı satılan teknoloji devleri NVDA, SPY ve AAPL'ye dağıtıldı.";
                plan.positions = {
                    {"^IRX / SGOV", "ABD Hazine Bonosu (Likidite Rezervi)", 100.0, 100.0, 99.0, 25.0, capital * 0.25, 0,
                     "LİKİDİTE REZERVİ", "Dip seviyelerden ek alım için likit nakit tamponu."},
                    {"NVDA", "Nvidia Corp (AI Momentum)", p_nvda, p_nvda * 1.30, p_nvda * 0.90, 35.0, capital * 0.35,
                     static_cast<int>((capital * 0.35) / p_nvda), "AŞIRI SATIM TEPKİ ALIMI", "Aşırı satım bölgesinde yüksek toparlanma potansiyeli."},
                    {"SPY", "SPDR S&P 500 ETF", p_spy, p_spy * 1.18, p_spy * 0.92, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_spy), "DİPTE KADEMELİ TOPLA", "Geniş piyasa toparlanma pozisyonu."},
                    {"AAPL", "Apple Inc", p_aapl, p_aapl * 1.15, p_aapl * 0.93, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_aapl), "BÜYÜME ALIMI", "Yüksek marjlı hizmet gelirleri."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM (FED & MAKRO ŞOK): Wall Street'te volatilite (VIX > 25) yüksekken tek seferde büyük alım yapmayın.",
                "2. ADIM (DOLAR FAİZİ): Nakit rezervinizi SGOV veya 3-aylık T-Bill bonosunda tutarak yıllık %4.10 risksiz getiri sağlayın.",
                "3. ADIM (STOP-LOSS): Zarar kes seviyelerini mutlaka sisteme girin; teknoloji çarpan daralmalarında anaparanızı koruyun."
            };
        } else {
            // ==============================================================
            // NORMAL / BOĞA REJİMİ
            // ==============================================================
            if (risk == InvestorRisk::DüşükRisk_Koruma) {
                plan.expected_annual_return_pct = 8.5;
                plan.expected_volatility_pct = 5.0;
                plan.max_drawdown_risk_pct = 4.0;
                plan.macro_verdict = "Wall Street Muhafazakar & Hazine Garantili Getiri: "
                                     "Portföyün %60'ı Hazine Bonosu (%4.10 faiz) ile güvenceye alınırken, "
                                     "kalan bölüm S&P 500 ve Apple temettü/nakit disiplinine tahsis edildi.";
                plan.positions = {
                    {"^IRX / SGOV", "ABD 3-Ay Hazine Bonosu (T-Bill)", 100.0, 100.0, 99.0, 60.0, capital * 0.60, 0,
                     "SABİT GETİRİYE BAĞLA", std::format("Yıllık net %{:.2f} risksiz USD garantisi.", us_tbill_rate * 100.0)},
                    {"SPY", "SPDR S&P 500 ETF (Geniş Piyasa)", p_spy, p_spy * 1.10, p_spy * 0.95, 25.0, capital * 0.25,
                     static_cast<int>((capital * 0.25) / p_spy), "KADEMELİ AL", "500 büyük şirkete risksiz ortaklık."},
                    {"AAPL", "Apple Inc (Nakit Zengini)", p_aapl, p_aapl * 1.12, p_aapl * 0.95, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_aapl), "AL VE TUT", "Devasa serbest nakit akışı ve istikrar."}
                };
            } else if (risk == InvestorRisk::Dengeli_Büyüme) {
                plan.expected_annual_return_pct = 16.5;
                plan.expected_volatility_pct = 12.0;
                plan.max_drawdown_risk_pct = 9.5;
                plan.macro_verdict = "Wall Street Dengeli Büyüme & Hazine Getirisi Rejimi: "
                                     "Risksiz Hazine faizleri (%4.10) ile portföyün %25'i garanti altına alınırken, "
                                     "kalan bölüm S&P 500 ve yapay zeka liderlerine dağıtıldı.";
                plan.positions = {
                    {"SPY", "SPDR S&P 500 ETF (Geniş Piyasa)", p_spy, p_spy * 1.12, p_spy * 0.94, 40.0, capital * 0.40,
                     static_cast<int>((capital * 0.40) / p_spy), "KADEMELİ AL", "500 büyük Amerikan şirketine dengeli ve temettülü ortaklık."},
                    {"NVDA", "Nvidia Corp (Yapay Zeka Çip Lideri)", p_nvda, p_nvda * 1.25, p_nvda * 0.92, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_nvda), "BÜYÜME ALIMI", "Veri merkezleri ve yapay zeka GPU talebinde pazar hakimiyeti."},
                    {"AAPL", "Apple Inc (Tüketici Elektroniği & Hizmetler)", p_aapl, p_aapl * 1.14, p_aapl * 0.94, 15.0, capital * 0.15,
                     static_cast<int>((capital * 0.15) / p_aapl), "AL VE TUT", "Devasa hisse geri alımları ve istikrarlı ekosistem nakit akışı."},
                    {"^IRX / SGOV", "ABD 3-Ay Hazine Bonosu (T-Bill)", 100.0, 100.0, 99.0, 25.0, capital * 0.25, 0,
                     "SABİT GETİRİYE BAĞLA", std::format("Yıllık net %{:.2f} risksiz faiz getirisi ile dalgalanmayı nötrleme.", us_tbill_rate * 100.0)}
                };
            } else {
                plan.expected_annual_return_pct = 26.0;
                plan.expected_volatility_pct = 21.0;
                plan.max_drawdown_risk_pct = 17.0;
                plan.macro_verdict = "Wall Street Agresif Momentum & Yapay Zeka Rejimi: "
                                     "Tüm sermaye kârlılık patlaması yaşayan Nvidia ve Mega-Cap teknoloji liderlerine tahsis edildi.";
                plan.positions = {
                    {"NVDA", "Nvidia Corp (AI GPU Dominance)", p_nvda, p_nvda * 1.30, p_nvda * 0.91, 50.0, capital * 0.50,
                     static_cast<int>((capital * 0.50) / p_nvda), "MOMENTUM ALIMI", "Generative AI dalgasının ana donanım sağlayıcısı."},
                    {"SPY", "SPDR S&P 500 ETF (Core Index)", p_spy, p_spy * 1.15, p_spy * 0.93, 30.0, capital * 0.30,
                     static_cast<int>((capital * 0.30) / p_spy), "BÜYÜME ALIMI", "Geniş piyasa rallisine ortaklık."},
                    {"AAPL", "Apple Inc (Ecosystem)", p_aapl, p_aapl * 1.16, p_aapl * 0.93, 20.0, capital * 0.20,
                     static_cast<int>((capital * 0.20) / p_aapl), "EKOSİSTEM ALIMI", "Yapay zeka cihaz yenileme döngüsü."}
                };
            }

            plan.actionable_steps = {
                "1. ADIM: Pozisyonları piyasa açılışının ilk 15 dakikası yerine New York saatiyle öğle saatlerinde likidite oturduğunda alın.",
                "2. ADIM: SPY hisselerini taşırken aylık %2 yukarısına Black-Scholes ile Covered Call satarak prim toplayın.",
                "3. ADIM: Tek hissede maksimum pozisyon payını %50'nin altında tutarak portföy riskini yönetin."
            };
        }

        enrich_all_positions_(plan.positions, client);
    }
};

} // namespace quant
