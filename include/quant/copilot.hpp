#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "black_scholes.hpp"
#include "news_sentiment.hpp"
#include "forecast_engine.hpp"
#include "forecast_backtest.hpp"
#include "alpha_screener.hpp"
#include "i18n.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <format>

namespace quant {

class QuantCopilot {
public:
    static void handle_query(const std::string& input, LiveMarketDataClient& client) {
        const auto& pal = ThemeEngine::instance().palette();
        std::string lower_input = input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

        std::cout << "\n" << pal.glow_rgb << "[COPILOT AI]: " << pal.reset_code;

        if (lower_input.find("merhaba") != std::string::npos || lower_input.find("selam") != std::string::npos ||
            lower_input.find("hello") != std::string::npos || lower_input.find("hi") != std::string::npos ||
            lower_input.find("hallo") != std::string::npos || lower_input.find("moin") != std::string::npos ||
            lower_input.find("guten tag") != std::string::npos) {
            std::cout << I18n::tr(
                "Merhaba! Ben 1980'ler Unix stili Modern C++ Quant Trading Copilot'ınızım.\n"
                "  Borsa İstanbul (BIST), Frankfurt (XETRA) ve Wall Street için canlı piyasa analizleri,\n"
                "  makro haber akışı, sentimel puanlama, Black-Scholes opsiyon modellemesi ve sermaye planı sunabilirim.\n"
                "  Örnek: 'haberler', 'sentiment', 'BIST stratejisi', 'XETRA analiz', 'THYAO fiyat', 'RHM Rheinmetall'.\n",

                "Hello! I am your 1980s Unix-style Modern C++ Quant Trading Copilot.\n"
                "  I provide live market analyses, macro news flow, sentiment scoring, Black-Scholes option modeling, and portfolio capital plans for Borsa Istanbul (BIST), Frankfurt (XETRA), and Wall Street.\n"
                "  Examples: 'news', 'sentiment', 'BIST strategy', 'XETRA analysis', 'AAPL price', 'RHM target'.\n",

                "Hallo! Ich bin Ihr Modern C++ Quant Trading Copilot im Unix-Stil der 1980er Jahre.\n"
                "  Ich biete Live-Marktanalysen, Makro-Nachrichtenfluss, Sentiment-Scoring, Black-Scholes-Optionsbewertung und Kapitalpläne für Borsa Istanbul (BIST), Frankfurt (XETRA) und Wall Street.\n"
                "  Beispiele: 'nachrichten', 'sentiment', 'DAX analyse', 'RHM zielkurs', 'AAPL kurs'.\n"
            );
            return;
        }

        if (lower_input.find("haber") != std::string::npos || lower_input.find("sentiment") != std::string::npos ||
            lower_input.find("makro") != std::string::npos || lower_input.find("duygu") != std::string::npos ||
            lower_input.find("news") != std::string::npos || lower_input.find("nachricht") != std::string::npos ||
            lower_input.find("stimmung") != std::string::npos) {
            std::cout << I18n::tr(
                "📰 Finansal Makro Haber Akışı ve Sentimel Analiz Raporu:\n",
                "📰 Financial Macro News Flow & Sentiment Analysis Report:\n",
                "📰 Finanzieller Makro-Nachrichtenfluss & Sentiment-Analysebericht:\n"
            );
            auto bist_macro = NewsSentimentEngine::build_bist_macro_sentiment(&client);
            auto xetra_macro = NewsSentimentEngine::build_xetra_macro_sentiment(client);

            std::cout << std::format("\n  🇹🇷 BIST 100 {}: {}  {}\n",
                                     I18n::tr("Sentimenti", "Sentiment", "Sentiment"),
                                     bist_macro.overall_gauge_bar, bist_macro.overall_sentiment_label);
            std::cout << "  • " << I18n::tr("Öne Çıkan Makro Gelişme: ", "Key Macro Development   : ", "Wichtige Makro-Entwickl.: ") << bist_macro.macro_developments[0].title << "\n"
                      << "    " << I18n::tr("Borsa Etkisi           : ", "Market Impact          : ", "Markt-Auswirkung       : ") << bist_macro.macro_developments[0].market_impact << "\n";

            std::cout << std::format("\n  🇩🇪 XETRA DAX {}: {}  {}\n",
                                     I18n::tr("Sentimenti", "Sentiment", "Sentiment"),
                                     xetra_macro.overall_gauge_bar, xetra_macro.overall_sentiment_label);
            std::cout << "  • " << I18n::tr("Öne Çıkan Makro Gelişme: ", "Key Macro Development   : ", "Wichtige Makro-Entwickl.: ") << xetra_macro.macro_developments[0].title << "\n"
                      << "    " << I18n::tr("Borsa Etkisi           : ", "Market Impact          : ", "Markt-Auswirkung       : ") << xetra_macro.macro_developments[0].market_impact << "\n";

            std::cout << "\n  👉 " << I18n::tr(
                "Hisse bazlı detaylı sentimel puanlar, makro katalizörler ve riskler için menüden [A] seçiniz!\n",
                "For detailed stock sentiment scores, catalysts and risk profiles, select [A] from the menu!\n",
                "Für aktienspezifische Sentiment-Scores, Katalysatoren und Risikoprofile wählen Sie [A] im Menü!\n"
            );
            return;
        }

        if (lower_input.find("msci") != std::string::npos || lower_input.find("çakıl") != std::string::npos ||
            lower_input.find("çöküş") != std::string::npos || lower_input.find("statü") != std::string::npos ||
            lower_input.find("crash") != std::string::npos || lower_input.find("absturz") != std::string::npos ||
            lower_input.find("plunge") != std::string::npos) {
            std::cout << I18n::tr(
                "🚨 MSCI Türkiye Uyarısı ve BIST Çakılma Analizi:\n"
                "  • Şok Kaynağı : MSCI'ın Türkiye'yi Gelişmekte Olan Ülke (EM) statüsünden düşürme (Frontier) uyarısı.\n"
                "  • Piyasa Etkisi: Pasif yabancı fonların BIST 30 hisselerinde zorunlu satışları ve devre kesiciler.\n"
                "  • Sentimel Puan: -0.73 (GÜÇLÜ AYI / KRİZ ŞOKU) ─ [██████┼──────]\n"
                "  • Acil Eylem   : Düşen bıçak tutulmaz! Sermayenin %60-%80'i derhal %45 risksiz TL PPF'ye çekilmeli,\n"
                "                   kalan bütçeyle yabancı satışının durulması ve kademeli dip beklenmelidir.\n"
                "  • Detaylı savunma portföyü ve lot planı için menüden [A] -> [1] BIST seçiniz!\n",

                "🚨 MSCI Turkey Warning & BIST Market Plunge Analysis:\n"
                "  • Catalyst     : MSCI consultation warning regarding possible downgrade from Emerging to Frontier Market.\n"
                "  • Market Impact: Forced foreign institutional outflows across BIST 30 equities triggering circuit breakers.\n"
                "  • Sentiment    : -0.73 (STRONG BEARISH / CRISIS SHOCK) ─ [██████┼──────]\n"
                "  • Urgent Action: Do not catch falling knives! Allocate 60-80% of capital into 45% risk-free TRY cash funds,\n"
                "                   reserving remainder for staged DCA bottom-fishing once foreign outflow subsides.\n"
                "  • For concrete defensive portfolio & allocation cards, choose [A] -> [1] BIST from the menu!\n",

                "🚨 MSCI Türkei-Warnung & BIST-Marktabsturz-Analyse:\n"
                "  • Schockursache: MSCI Konsultationswarnung vor möglicher Herabstufung von Emerging auf Frontier Market.\n"
                "  • Markteffekt  : Erzwungene Abflüsse passiver ausländischer Fonds bei BIST-30-Werten und Handelsunterbrechungen.\n"
                "  • Sentiment    : -0,73 (STARK BÄRISCH / KRISENSCHOCK) ─ [██████┼──────]\n"
                "  • Sofortaktion : Fallendes Messer meiden! 60-80% des Kapitals in risikofreie 45% TRY-Geldmarktfonds parken,\n"
                "                   Rest für schrittweisen DCA-Einstieg nach Versiegen der Verkäufe bereithalten.\n"
                "  • Für konkretes Defensiv-Portfolio und Stückzahlplan wählen Sie [A] -> [1] BIST im Menü!\n"
            );
            return;
        }

        if (lower_input.find("bist") != std::string::npos || lower_input.find("borsa") != std::string::npos ||
            lower_input.find("istanbul") != std::string::npos || lower_input.find("turkey") != std::string::npos ||
            lower_input.find("türkei") != std::string::npos) {
            std::cout << I18n::tr(
                "🇹🇷 Borsa İstanbul (BIST 100) Piyasa Değerlendirmesi:\n"
                "  • Makro Durum : MSCI gelişmekte olan ülke (EM) statü uyarısı ve yabancı sermaye çıkışı baskısı.\n"
                "  • Sentimel Puan: GÜÇLÜ AYI / KRİZ REJİMİ (-0.73) ─ [██████┼──────]\n"
                "  • BIST Strateji: Sermayenin %60-%80'i yıllık %45 risksiz TL Para Piyasası Fonu'nda tutulmalı,\n"
                "                   kalan bütçe dip seviyelerden kademeli (DCA) toplanmalıdır (asla tek seferde değil!).\n"
                "  • Detaylı hisse lotları, haber akışı ve makro kartlar için menüden [A] -> [1] BIST seçiniz!\n",

                "🇹🇷 Borsa Istanbul (BIST 100) Market Assessment:\n"
                "  • Macro Regime : MSCI Emerging Market status consultation and foreign capital outflow headwind.\n"
                "  • Sentiment    : STRONG BEARISH / CRISIS REGIME (-0.73) ─ [██████┼──────]\n"
                "  • BIST Strategy: Hold 60-80% of capital in 45% risk-free TRY Money Market Funds,\n"
                "                   accumulating selective high-quality exporters (THYAO, ASELSAN) via staged DCA.\n"
                "  • For detailed share allocations & macro analysis cards, select [A] -> [1] BIST from menu!\n",

                "🇹🇷 Borsa Istanbul (BIST 100) Marktbewertung:\n"
                "  • Makrolage    : MSCI Emerging-Market-Konsultation und Druck durch ausländische Kapitalabflüsse.\n"
                "  • Sentiment    : STARK BÄRISCH / KRISENREGIME (-0,73) ─ [██████┼──────]\n"
                "  • BIST-Strategie: 60-80% des Kapitals in risikofreien 45% TRY-Geldmarktfonds halten,\n"
                "                    den Rest stufenweise (DCA) in fundamentale Exportchampions investieren.\n"
                "  • Für detaillierte Stückzahlen und Makrokarten wählen Sie [A] -> [1] BIST im Menü!\n"
            );
            return;
        }

        if (lower_input.find("xetra") != std::string::npos || lower_input.find("dax") != std::string::npos ||
            lower_input.find("frankfurt") != std::string::npos || lower_input.find("almanya") != std::string::npos ||
            lower_input.find("germany") != std::string::npos || lower_input.find("deutschland") != std::string::npos) {
            std::cout << I18n::tr(
                "🇩🇪 Frankfurt XETRA (DAX 40) Piyasa Değerlendirmesi:\n"
                "  • Makro Durum : ECB faiz indirim sürecinde Alman Devlet Tahvili (Bund) %3.25 net garanti sunuyor.\n"
                "  • XETRA Strateji: Avrupa savunma sanayii süper döngüsünün lokomotifi Rheinmetall (RHM.DE) ve\n"
                "                    kurumsal bulut lideri SAP (SAP.DE) Euro bazında üstün büyüme sunmaktadır.\n"
                "  • Detaylı Euro bazlı lot, haber akışı ve zarar kes kartları için menüden [A] -> [2] XETRA seçiniz!\n",

                "🇩🇪 Frankfurt XETRA (DAX 40) Market Assessment:\n"
                "  • Macro Regime  : ECB rate-cut cycle; German Federal Bonds (Bunds) offer ~3.25% net guaranteed yield.\n"
                "  • XETRA Strategy: European defense supercycle leader Rheinmetall (RHM.DE) and enterprise cloud\n"
                "                    behemoth SAP (SAP.DE) provide superior Euro-denominated growth.\n"
                "  • For concrete EUR share targets, stop-loss and macro cards, select [A] -> [2] XETRA from menu!\n",

                "🇩🇪 Frankfurt XETRA (DAX 40) Marktbewertung:\n"
                "  • Makrolage     : EZB-Zinssenkungszyklus; Deutsche Bundesanleihen bieten ~3,25% garantierte Rendite.\n"
                "  • XETRA-Strategie: Europäischer Rüstungs-Superzyklus mit Rheinmetall (RHM.DE) sowie Cloud-Gigant\n"
                "                    SAP (SAP.DE) bieten herausragendes Euro-Wachstumspotenzial.\n"
                "  • Für konkrete Euro-Stückzahlen, Stop-Loss und Makrokarten wählen Sie [A] -> [2] XETRA im Menü!\n"
            );
            return;
        }

        if (lower_input.find("para") != std::string::npos || lower_input.find("yatır") != std::string::npos ||
            lower_input.find("tavsiye") != std::string::npos || lower_input.find("strateji") != std::string::npos ||
            lower_input.find("kazan") != std::string::npos || lower_input.find("invest") != std::string::npos ||
            lower_input.find("anlegen") != std::string::npos || lower_input.find("strategy") != std::string::npos ||
            lower_input.find("strategie") != std::string::npos) {
            std::cout << I18n::tr(
                "Sermaye Tahsis & Para Kazanma Stratejisi:\n"
                "  1. 🇹🇷 BIST (TL): Yıllık %45 risksiz TL mevduat/PPF tamponu + İhracatçı THYAO / ASELS büyümesi.\n"
                "  2. 🇩🇪 XETRA (EUR): %3.25 Bund faiz tamponu + Savunma devi Rheinmetall ve SAP büyümesi.\n"
                "  3. 🇺🇸 US (USD): %4.10 Hazine Bonosu + S&P 500 ve Nvidia büyümesi.\n"
                "  👉 Menüden [A] tuşlayarak bütçenize göre kaç lot alacağınızı, haber ve makro analiz kartlarını açın!\n",

                "Capital Allocation & Investment Strategy Overview:\n"
                "  1. 🇹🇷 BIST (TRY): 45% annual risk-free TRY cash buffer + Export growth via THYAO / ASELSAN.\n"
                "  2. 🇩🇪 XETRA (EUR): 3.25% Bund fixed yield buffer + European defense leader Rheinmetall & SAP.\n"
                "  3. 🇺🇸 US (USD): 4.10% US Treasury T-Bill haven + S&P 500 & Nvidia tech growth.\n"
                "  👉 Press [A] from the main menu to calculate exact share counts, stops, and macro cards!\n",

                "Kapitalallokation & Anlagestrategie-Übersicht:\n"
                "  1. 🇹🇷 BIST (TRY): 45% jährlicher risikofreier TRY-Puffer + Exportwachstum via THYAO / ASELSAN.\n"
                "  2. 🇩🇪 XETRA (EUR): 3,25% Bundesanleihen-Puffer + europäischer Verteidigungsführer Rheinmetall & SAP.\n"
                "  3. 🇺🇸 US (USD): 4,10% US-Treasury T-Bills + S&P 500 & Nvidia KI-Wachstum.\n"
                "  👉 Wählen Sie [A] im Hauptmenü für exakte Stückzahlen, Stop-Loss und Makro-Analysekarten!\n"
            );
            return;
        }

        if (lower_input.find("faiz") != std::string::npos || lower_input.find("hazine") != std::string::npos ||
            lower_input.find("rate") != std::string::npos || lower_input.find("yield") != std::string::npos ||
            lower_input.find("zins") != std::string::npos || lower_input.find("anleihe") != std::string::npos) {
            std::cout << I18n::tr(
                "Canlı ABD Hazine Tahvili Getiri Eğrisi çekiliyor...\n",
                "Fetching live US Treasury Yield Curve...\n",
                "Live US-Treasury-Zinsstrukturkurve wird abgerufen...\n"
            );
            auto curve = client.fetch_treasury_yields();
            std::string shape = (curve.ten_year_yield > curve.three_month_yield) ?
                I18n::tr("normal pozitif eğimli", "normal upward-sloping", "normal positiv geneigt") :
                I18n::tr("ters (inverted / resesyon sinyali)", "inverted (recession signal)", "invers (Rezessionssignal)");

            std::cout << std::format("  • 3-{} T-Bill (^IRX): {:.2f}%\n"
                                     "  • 5-{} Note   (^FVX): {:.2f}%\n"
                                     "  • 10-{} Bond  (^TNX): {:.2f}%\n"
                                     "  • 30-{} Bond  (^TYX): {:.2f}%\n"
                                     "  {}: {} {}.\n"
                                     "  {}: 🇹🇷 TRY ~%45.0, 🇩🇪 Bund ~%3.25, 🇺🇸 US ~%4.10.\n",
                                     I18n::tr("Ay", "Mo", "M"), curve.three_month_yield * 100.0,
                                     I18n::tr("Yıl", "Yr", "J"), curve.five_year_yield * 100.0,
                                     I18n::tr("Yıl", "Yr", "J"), curve.ten_year_yield * 100.0,
                                     I18n::tr("Yıl", "Yr", "J"), curve.thirty_year_yield * 100.0,
                                     I18n::tr("Piyasa Durumu", "Market State", "Marktlage"),
                                     I18n::tr("Getiri eğrisi", "Yield curve is", "Zinskurve ist"), shape,
                                     I18n::tr("Risksiz Göstergeler", "Risk-free benchmarks", "Risikofreie Benchmarks"));
            return;
        }

        if (lower_input.find("opsiyon") != std::string::npos || lower_input.find("option") != std::string::npos || lower_input.find("black") != std::string::npos) {
            std::cout << I18n::tr(
                "Analitik Black-Scholes & Monte Carlo Çekirdeği devrede.\n  Standart varsayım (S=100, K=100, T=0.5y, vol=20%, r=4.5%):\n",
                "Analytical Black-Scholes & Monte Carlo Engine online.\n  Standard baseline (S=100, K=100, T=0.5y, vol=20%, r=4.5%):\n",
                "Analytische Black-Scholes- & Monte-Carlo-Engine aktiv.\n  Standard-Parameter (S=100, K=100, T=0.5J, Vol=20%, r=4.5%):\n"
            );
            BlackScholes bs(100.0, 100.0, 0.5, PayoffType::Call, 0.045, 0.015);
            double call_p = bs(0.20);
            auto greeks = bs.risk_values(0.20);
            std::cout << std::format("  • {}: {:.2f}  |  Delta: {:.4f}  |  Gamma: {:.4f}\n"
                                     "  • {}\n",
                                     I18n::tr("Call Primi", "Call Premium", "Call-Prämie"),
                                     call_p, greeks[RiskValues::Delta], greeks[RiskValues::Gamma],
                                     I18n::tr("Menüden [4] tuşlayarak canlı piyasa verisiyle tam parametrik analiz yapabilirsiniz.",
                                              "Select [4] from the menu for full parametric analysis with live market quotes.",
                                              "Wählen Sie [4] im Menü für vollständige parametrische Analysen mit Live-Daten."));
            return;
        }

        // 1. Genel Piyasa Özeti & Küresel Durum Sorgusu
        if (lower_input.find("piyasa") != std::string::npos || lower_input.find("durum") != std::string::npos ||
            lower_input.find("özet") != std::string::npos || lower_input.find("genel") != std::string::npos ||
            lower_input.find("endeks") != std::string::npos || lower_input.find("market") != std::string::npos ||
            lower_input.find("summary") != std::string::npos || lower_input.find("overview") != std::string::npos ||
            lower_input.find("übersicht") != std::string::npos) {
            std::cout << I18n::tr(
                "🌐 Küresel Piyasa Durumu & Sentimel Özeti (Canlı Makro Görünüm):\n\n",
                "🌐 Global Market Overview & Macro Sentiment Summary (Live):\n\n",
                "🌐 Globale Marktübersicht & Makro-Sentiment-Zusammenfassung (Live):\n\n"
            );
            auto bist = NewsSentimentEngine::build_bist_macro_sentiment(&client);
            auto xetra = NewsSentimentEngine::build_xetra_macro_sentiment(client);
            auto us = NewsSentimentEngine::build_us_macro_sentiment(client);

            std::cout << std::format("  🇹🇷 BIST 100 (Turkey)   : {}  {}\n"
                                     "     • {}: {}\n\n",
                                     bist.overall_gauge_bar, bist.overall_sentiment_label,
                                     I18n::tr("Görünüm", "Outlook", "Ausblick"), bist.macro_synthesis);
            std::cout << std::format("  🇩🇪 DAX 40 (Germany)    : {}  {}\n"
                                     "     • {}: {}\n\n",
                                     xetra.overall_gauge_bar, xetra.overall_sentiment_label,
                                     I18n::tr("Görünüm", "Outlook", "Ausblick"), xetra.macro_synthesis);
            std::cout << std::format("  🇺🇸 S&P 500 (Wall St.)  : {}  {}\n"
                                     "     • {}: {}\n\n",
                                     us.overall_gauge_bar, us.overall_sentiment_label,
                                     I18n::tr("Görünüm", "Outlook", "Ausblick"), us.macro_synthesis);
            std::cout << "  👉 " << I18n::tr(
                "Portföyünüz için somut sermaye tahsisi ve lot planı almak için menüden [A] seçiniz!\n",
                "Select [A] from the menu to generate a concrete capital allocation and share sizing plan!\n",
                "Wählen Sie [A] im Menü für konkrete Kapitalallokation und Stückzahl-Pläne!\n"
            );
            return;
        }

        // 2. Asimetrik Alfa / Radar-Altı Fırsat / Ne Almalıyım / Hisse Önerisi Sorgusu
        if (lower_input.find("fırsat") != std::string::npos || lower_input.find("ne al") != std::string::npos ||
            lower_input.find("öneri") != std::string::npos || lower_input.find("hangi") != std::string::npos ||
            lower_input.find("hisseler") != std::string::npos || lower_input.find("radar") != std::string::npos ||
            lower_input.find("asimetrik") != std::string::npos || lower_input.find("gizli") != std::string::npos ||
            lower_input.find("sıkışma") != std::string::npos || lower_input.find("şişmiş") != std::string::npos ||
            lower_input.find("sürü") != std::string::npos || lower_input.find("kalabalık") != std::string::npos ||
            lower_input.find("opportunity") != std::string::npos || lower_input.find("alpha") != std::string::npos ||
            lower_input.find("screener") != std::string::npos || lower_input.find("what to buy") != std::string::npos ||
            lower_input.find("was kaufen") != std::string::npos || lower_input.find("chance") != std::string::npos) {
            
            std::string scan_reg = "BIST";
            std::string reg_title = "🇹🇷 Borsa İstanbul (BIST)";
            if (lower_input.find("xetra") != std::string::npos || lower_input.find("dax") != std::string::npos || 
                lower_input.find("almanya") != std::string::npos || lower_input.find("frankfurt") != std::string::npos ||
                lower_input.find("germany") != std::string::npos || lower_input.find("deutschland") != std::string::npos) {
                scan_reg = "XETRA";
                reg_title = "🇩🇪 Frankfurt XETRA (DAX)";
            } else if (lower_input.find("us") != std::string::npos || lower_input.find("abd") != std::string::npos || 
                       lower_input.find("wall") != std::string::npos || lower_input.find("nasdaq") != std::string::npos ||
                       lower_input.find("sp500") != std::string::npos) {
                scan_reg = "US";
                reg_title = "🇺🇸 Wall Street (S&P 500 / NASDAQ)";
            }

            std::cout << I18n::tr(
                std::format("💡 Asimetrik Alfa & Radar-Altı Analiz Raporu ({}):\n", reg_title),
                std::format("💡 Asymmetric Alpha & Off-Radar Opportunity Report ({}):\n", reg_title),
                std::format("💡 Asymmetrischer Alpha & Unter-dem-Radar-Report ({}):\n", reg_title)
            );
            std::cout << I18n::tr(
                "  • Geleneksel Yanılgı: Herkesin konuştuğu ünlü devler (THYAO, ASELSAN, NVDA, Rheinmetall)\n"
                "    zaten kitleler tarafından fiyatlanmış 'Crowded Beta'dır. Bu hisselerde tepe riski yüksek,\n"
                "    kalan kâr potansiyeli düşüktür (Negatif Asimetri: 1 Risk için 0.35 Kazanç).\n"
                "  • Gerçek Quant Kazancı: Düşük kalabalık skoruna sahip, sessizce kurumsal akümülasyon yaşayan\n"
                "    veya volatilite sıkışması (Coiled Spring) içinde patlama bekleyen radar-altı varlıklardadır.\n\n",

                "  • Conventional Fallacy: Consensus giants discussed by everyone (NVDA, Apple, Rheinmetall)\n"
                "    represent already priced-in 'Crowded Beta'. At peaks, downside risk is high while remaining\n"
                "    upside is constrained (Negative Asymmetry: only ~0.35 reward per 1 unit of risk).\n"
                "  • Institutional Edge: True asymmetric edge lies in off-radar assets with low crowdedness scores,\n"
                "    stealth institutional accumulation, or coiling volatility squeezes awaiting breakout expansion.\n\n",

                "  • Konventioneller Trugschluss: Konsens-Giganten (NVDA, Apple, Rheinmetall) stellen bereits\n"
                "    eingepreiste 'Crowded Beta' dar. Am Hoch ist das Rückschlagsrisiko hoch bei geringem Restpotenzial\n"
                "    (Negative Asymmetrie: nur ca. 0,35 Ertrag pro 1 Einheit Risiko).\n"
                "  • Quant-Vorteil: Der asymmetrische Vorteil liegt in Werten unter dem Radar mit geringem Herdenscore,\n"
                "    diskreter Akkumulation oder Volatilitätskompression vor dem dynamischen Ausbruch.\n\n"
            );

            std::cout << pal.dim_rgb << I18n::tr("Canlı piyasa taranıyor ve asimetrik risk/kazanç modelleri hesaplanıyor...",
                                                "Scanning live market and calculating asymmetric risk/reward profiles...",
                                                "Live-Markt wird gescannt und asymmetrische Chance/Risiko-Profile berechnet...")
                      << pal.reset_code << "\n";

            auto candidates = QuantOpportunityScanner::scan_market(scan_reg, client);
            QuantOpportunityScanner::render_screener_report(candidates, reg_title);
            std::cout << "  👉 " << I18n::tr(
                "Menüden [S] tuşlayarak tüm piyasalar için interaktif filtreli tarama yapabilirsiniz!\n",
                "Press [S] from the main menu for interactive multi-market opportunity screening!\n",
                "Wählen Sie [S] im Hauptmenü für interaktive Markt-Screenings mit Filtern!\n"
            );
            return;
        }

        // 3. Evrensel Dinamik Ticker Tanıma (NYSE, NASDAQ, BIST, XETRA)
        std::string found_ticker = extract_ticker_symbol_(input, lower_input);

        // 3.5. Backtest / Geçmiş Verilerde Doğrulama Sorgusu
        if (lower_input.find("backtest") != std::string::npos || lower_input.find("test et") != std::string::npos ||
            lower_input.find("doğru mu") != std::string::npos || lower_input.find("isabet") != std::string::npos ||
            lower_input.find("başarı") != std::string::npos || lower_input.find("geçmiş") != std::string::npos ||
            lower_input.find("accuracy") != std::string::npos || lower_input.find("verify") != std::string::npos ||
            lower_input.find("trefferquote") != std::string::npos) {
            
            std::string test_sym = found_ticker.empty() ? "THYAO.IS" : found_ticker;
            int horizon = 5; // 5 gün (1 hafta) varsayılan
            if (lower_input.find("1 ay") != std::string::npos || lower_input.find("20 gün") != std::string::npos ||
                lower_input.find("aylık") != std::string::npos || lower_input.find("1 month") != std::string::npos ||
                lower_input.find("1 monat") != std::string::npos) {
                horizon = 20;
            } else if (lower_input.find("10 gün") != std::string::npos || lower_input.find("2 hafta") != std::string::npos ||
                       lower_input.find("2 weeks") != std::string::npos || lower_input.find("2 wochen") != std::string::npos) {
                horizon = 10;
            }

            std::cout << I18n::tr(
                std::format("🔬 Quant Forecast Engine için '{}' üzerinde 1-Yıllık Walk-Forward Backtest çalıştırılıyor (Ufuk: {} Gün)...\n", test_sym, horizon),
                std::format("🔬 Running 1-Year Walk-Forward Backtest for Quant Forecast Engine on '{}' (Horizon: {} Days)...\n", test_sym, horizon),
                std::format("🔬 1-Jahres-Walk-Forward-Backtest für Quant Forecast Engine auf '{}' läuft (Zeithorizont: {} Tage)...\n", test_sym, horizon)
            );
            auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, test_sym, horizon, "1y");
            ForecastBacktestEngine::render_backtest_report(rep);

            std::cout << pal.glow_rgb << "\n  💡 " << I18n::tr("QUANT MODEL DEĞERLENDİRMESİ:\n", "QUANT MODEL ASSESSMENT:\n", "QUANT-MODELL-BEWERTUNG:\n") << pal.reset_code
                      << I18n::tr(
                            std::format("  • Olasılık Bandı Güveni: Gerçek fiyat %{:.1f} oranında modelin belirlediği ±1σ bandı içinde kalmıştır (teorik beklenti: %68.3).\n", rep.in_band_coverage_pct),
                            std::format("  • Probability Band Fit: Actual prices stayed within ±1σ bounds in {:.1f}% of bars (theoretical benchmark: 68.3%).\n", rep.in_band_coverage_pct),
                            std::format("  • Wahrscheinlichkeitsband: Realer Kurs lag in {:.1f}% der Fälle im ±1σ-Band (theoretische Norm: 68,3%).\n", rep.in_band_coverage_pct)
                         )
                      << I18n::tr(
                            std::format("  • Hedef Fiyat İsabeti  : Modelin {} gün önceden hesapladığı hedef fiyat, ortalama yalnızca %{:.2f} sapma (MAPE) ile gerçekleşmiştir.\n", rep.horizon_days, rep.mean_absolute_error_pct),
                            std::format("  • Target Price Accuracy: Model target projected {} days ahead realized with an average of only {:.2f}% error (MAPE).\n", rep.horizon_days, rep.mean_absolute_error_pct),
                            std::format("  • Zielkurs-Genauigkeit : Die {} Tage vorausberechnete Zielprognose wich im Schnitt nur um {:.2f}% ab (MAPE).\n", rep.horizon_days, rep.mean_absolute_error_pct)
                         )
                      << I18n::tr(
                            std::format("  • Sermaye Koruma Etkisi: Model kriz ve ayı dönemlerinde nakite geçerek Max Drawdown'ı %{:.1f}'den %{:.1f}'e indirmiş, strateji getirisi %{:+.2f} olmuştur.\n",
                                        rep.buy_and_hold_max_drawdown_pct, rep.strategy_max_drawdown_pct, rep.strategy_return_pct),
                            std::format("  • Capital Protection   : Stepping into cash during adverse regimes reduced Max Drawdown from {:.1f}% to {:.1f}%, strategy yield: {:+.2f}%.\n",
                                        rep.buy_and_hold_max_drawdown_pct, rep.strategy_max_drawdown_pct, rep.strategy_return_pct),
                            std::format("  • Kapitalschutz-Effekt : Cash-Quote in Krisen senkte den Max Drawdown von {:.1f}% auf {:.1f}%, Strategierendite: {:+.2f}%.\n",
                                        rep.buy_and_hold_max_drawdown_pct, rep.strategy_max_drawdown_pct, rep.strategy_return_pct)
                         );
            return;
        }

        // Eğer kullanıcı belirli bir hisse belirtmediyse rastgele SPY'a düşmek yerine akıllı yönlendirme yap
        if (found_ticker.empty()) {
            if (lower_input.find("hedef") != std::string::npos || lower_input.find("tahmin") != std::string::npos ||
                lower_input.find("yön") != std::string::npos || lower_input.find("yukarı") != std::string::npos ||
                lower_input.find("aşağı") != std::string::npos || lower_input.find("target") != std::string::npos ||
                lower_input.find("forecast") != std::string::npos || lower_input.find("direction") != std::string::npos ||
                lower_input.find("prognose") != std::string::npos || lower_input.find("zielkurs") != std::string::npos) {
                std::cout << I18n::tr(
                    "🎯 Quant Hedef Fiyat & Zaman Dilimi Yön Tahmin Motoru Devrede:\n\n"
                    "  Sistemimiz hisseler için Geometric Brownian Motion (GBM) drift'i, kurumsal VWAP\n"
                    "  ortalamaya dönüş kuvveti, volatilite bantları (±1σ) ve anlık haber sentimelini birleştirerek:\n"
                    "  • 1 Hafta (Kısa Vade), 1 Ay (Orta Vade) ve 3 Ay (Çeyreklik) Hedef Fiyatlarını (TL, $, €)\n"
                    "  • 5 Seviyeli Yön Tahminini hesaplar:\n"
                    "    1. 🚀 GÜÇLÜ YUKARI  (Strong Bullish / Yüksek Prim Beklentisi)\n"
                    "    2. 📈 YUKARI        (Bullish / Pozitif Trend)\n"
                    "    3. ⚖️ NÖTR          (Neutral / Yatay Konsolidasyon)\n"
                    "    4. 📉 AŞAĞI         (Bearish / Düzeltme & Kâr Satışı)\n"
                    "    5. 🔻 GÜÇLÜ AŞAĞI   (Strong Bearish / Kriz & Ağır Satış Baskısı)\n\n"
                    "  👉 Belirli bir hissenin hedef fiyat ve yönünü görmek için sembolle birlikte yazın:\n"
                    "     Örnek: 'THYAO hedef fiyat', 'SPY 1 ay yön', 'NVDA tahmin', 'Rheinmetall 3 ay'.\n",

                    "🎯 Quant Target Price & Multi-Horizon Direction Forecast Engine:\n\n"
                    "  Combines Geometric Brownian Motion (GBM) drift, institutional VWAP mean reversion,\n"
                    "  stochastic volatility bands (±1σ), and real-time news sentiment to calculate:\n"
                    "  • 1 Week (Short-Term), 1 Month (Mid-Term) and 3 Months (Quarterly) Target Prices\n"
                    "  • 5-Level Directional Regimes:\n"
                    "    1. 🚀 STRONG BULLISH (Superior Upside Momentum)\n"
                    "    2. 📈 BULLISH        (Positive Trend & Value Support)\n"
                    "    3. ⚖️ NEUTRAL        (Range Consolidation)\n"
                    "    4. 📉 BEARISH        (Correction & Profit Taking)\n"
                    "    5. 🔻 STRONG BEARISH (Severe Selling / Crisis Mode)\n\n"
                    "  👉 Mention a symbol to inspect targets and directional paths:\n"
                    "     Examples: 'SPY target price', 'AAPL 1 month direction', 'NVDA forecast', 'RHM 3 months'.\n",

                    "🎯 Quant Zielkurs- & Zeithorizont-Richtungsprognose-Engine:\n\n"
                    "  Kombiniert geometrische Brownsche Bewegung (GBM), institutionelle VWAP-Rückkehr,\n"
                    "  stochastische Volatilitätsbänder (±1σ) und Echtzeit-News-Sentiment zur Berechnung von:\n"
                    "  • 1 Woche (Kurzfristig), 1 Monat (Mittelfristig) und 3 Monate (Quartal) Zielkursen\n"
                    "  • 5-Stufen Trendregimen:\n"
                    "    1. 🚀 STARK BULLISCH (Hohes Kurspotenzial & Momentum)\n"
                    "    2. 📈 BULLISCH       (Solider Aufwärtstrend)\n"
                    "    3. ⚖️ NEUTRAL        (Seitwärtskonsolidierung)\n"
                    "    4. 📉 BÄRISCH        (Korrektur & Gewinnmitnahmen)\n"
                    "    5. 🔻 STARK BÄRISCH  (Starker Abgabedruck / Krise)\n\n"
                    "  👉 Geben Sie ein Tickersymbol für konkrete Kursziele ein:\n"
                    "     Beispiele: 'DAX zielkurs', 'SAP 1 monat', 'NVDA prognose', 'RHM 3 monate'.\n"
                );
                return;
            }

            std::cout << I18n::tr(
                "Sorunuzda belirli bir hisse sembolü tespit edilemedi.\n"
                "  Canlı piyasa ve duygu analizi için bir hisse adı veya finansal konu yazabilirsiniz:\n"
                "  • Hedef & Yön     : 'THYAO hedef fiyat', 'SPY 1 ay yön', 'NVDA tahmin', 'RHM 3 ay'\n"
                "  • Hisse Analizleri: 'SPY analiz', 'THYAO fiyat', 'NVDA', 'Rheinmetall', 'ASELSAN'\n"
                "  • Piyasa Raporu   : 'piyasalar nasıl', 'haberler', 'faizler', 'MSCI çakılması', 'BIST stratejisi'\n"
                "  • Algoritmik Plan : Sermayenize göre tam bütçe ve lot planı için menüden [A] seçiniz.\n",

                "No specific asset ticker detected in your query.\n"
                "  You can type an asset ticker or financial query for live analysis:\n"
                "  • Target & Direction: 'SPY target price', 'NVDA 1 month direction', 'RHM 3 months'\n"
                "  • Asset Diagnostics : 'AAPL analysis', 'TSLA price', 'NVDA', 'Rheinmetall', 'GRAM-ALTIN'\n"
                "  • Market Reports    : 'market overview', 'news flow', 'treasury yields', 'BIST strategy'\n"
                "  • Algorithmic Plan  : Choose [A] from the menu for tailored capital and share sizing.\n",

                "Kein bestimmtes Tickersymbol in Ihrer Eingabe erkannt.\n"
                "  Geben Sie einen Ticker oder ein Finanzthema für Live-Analysen ein:\n"
                "  • Zielkurs & Trend  : 'SAP zielkurs', 'NVDA 1 monat prognose', 'RHM 3 monate'\n"
                "  • Aktienanalyse     : 'AAPL analyse', 'BMW kurs', 'Rheinmetall', 'GRAM-ALTIN'\n"
                "  • Marktberichte     : 'marktübersicht', 'nachrichten', 'anleiherenditen', 'DAX strategie'\n"
                "  • Kapitalplan       : Wählen Sie [A] im Menü für maßgeschneiderte Portfolio-Allokationen.\n"
            );
            return;
        }

        std::string currency = "$";
        if (found_ticker.ends_with(".IS") || found_ticker.ends_with(".TL")) currency = "TL";
        else if (found_ticker.ends_with(".DE")) currency = "EUR";

        std::cout << I18n::tr(
            std::format("Canlı piyasa motoru '{}' için sorgulanıyor...\n", found_ticker),
            std::format("Querying live market engine for '{}'...\n", found_ticker),
            std::format("Live-Markt-Engine wird für '{}' abgefragt...\n", found_ticker)
        );
        auto data = client.fetch_asset(found_ticker);

        // Akıllı fallback: Suffix girilmemişse ve bulunamadıysa BIST veya XETRA dene
        if (data.current_price <= 0.0 && found_ticker.find('.') == std::string::npos && found_ticker.find('-') == std::string::npos) {
            auto bist_try = client.fetch_asset(found_ticker + ".IS");
            if (bist_try.current_price > 0.0) {
                data = std::move(bist_try);
                found_ticker += ".IS";
                currency = "TL";
            } else {
                auto xetra_try = client.fetch_asset(found_ticker + ".DE");
                if (xetra_try.current_price > 0.0) {
                    data = std::move(xetra_try);
                    found_ticker += ".DE";
                    currency = "EUR";
                }
            }
        }

        if (data.current_price <= 0.0) {
            std::cout << I18n::tr(
                std::format("\n  ❌ '{}' sembolü için canlı piyasa kotasyonu çekilemedi.\n"
                            "     Lütfen borsa kodunun doğruluğunu kontrol edin:\n"
                            "     • ABD Hisseleri (NYSE/NASDAQ) : DE, CAT, NVDA, AAPL, MSFT, PLTR, SPY\n"
                            "     • BIST Hisseleri (Borsa İst.) : THYAO.IS, ASELS.IS, EREGL.IS, TUPRS.IS\n"
                            "     • Frankfurt (XETRA)           : RHM.DE, SAP.DE, SIE.DE, BMW.DE\n", found_ticker),
                std::format("\n  ❌ Could not retrieve live market quote for '{}'.\n"
                            "     Please verify the ticker symbol:\n"
                            "     • US Equities (NYSE/NASDAQ)  : DE, CAT, NVDA, AAPL, MSFT, PLTR, SPY\n"
                            "     • BIST Equities (Istanbul)   : THYAO.IS, ASELS.IS, EREGL.IS, TUPRS.IS\n"
                            "     • Frankfurt (XETRA)          : RHM.DE, SAP.DE, SIE.DE, BMW.DE\n", found_ticker),
                std::format("\n  ❌ Live-Kurs für '{}' konnte nicht abgerufen werden.\n"
                            "     Bitte überprüfen Sie das Tickersymbol:\n"
                            "     • US-Aktien (NYSE/NASDAQ)    : DE, CAT, NVDA, AAPL, MSFT, PLTR, SPY\n"
                            "     • BIST-Aktien (Istanbul)     : THYAO.IS, ASELS.IS, EREGL.IS, TUPRS.IS\n"
                            "     • Frankfurt (XETRA)          : RHM.DE, SAP.DE, SIE.DE, BMW.DE\n", found_ticker)
            );
            return;
        }

        auto sent = NewsSentimentEngine::get_asset_sentiment(found_ticker, found_ticker, client);

        double vwap_dev_pct = (data.vwap > 0.0) ? ((data.current_price / data.vwap) - 1.0) * 100.0 : 0.0;
        double vol_pct = data.annualized_volatility * 100.0;

        std::string vol_desc = (vol_pct < 12.0) ?
            I18n::tr("Düşük Oynaklık / Sıkışma", "Low Volatility / Squeeze", "Geringe Volatilität / Squeeze") :
            ((vol_pct < 28.0) ?
                I18n::tr("Normal / Dengeli", "Normal / Balanced", "Normal / Ausgewogen") :
                I18n::tr("Yüksek Volatilite", "High Volatility", "Hohe Volatilität"));

        std::cout << std::format("  • {:<24}: {}\n"
                                 "  • {:<24}: {:.2f} {}\n"
                                 "  • {:<24}: {:.2f} {} ({}: %{:+.2f})\n"
                                 "  • {:<24}: %{:.2f} ({})\n"
                                 "  • {:<24}: {}  {}\n"
                                 "  • {:<24}: {}\n"
                                 "  • {:<24}: {}\n",
                                 I18n::tr("Sembol", "Symbol", "Symbol"), data.symbol,
                                 I18n::tr("Son Fiyat", "Last Price", "Letzter Kurs"), data.current_price, currency,
                                 I18n::tr("30-Günlük VWAP", "30-Day VWAP", "30-Tage-VWAP"), data.vwap, currency,
                                 I18n::tr("Sapma", "Dev", "Abw"), vwap_dev_pct,
                                 I18n::tr("Yıllıklandırılmış Vol.", "Annualized Vol.", "Annualisierte Vol."), vol_pct, vol_desc,
                                 I18n::tr("Sentimel Puan", "Sentiment Score", "Sentiment-Score"), sent.sentiment_bar, sent.sentiment_label,
                                 I18n::tr("Makro Katalizör", "Macro Catalyst", "Makro-Katalysator"), sent.macro_catalyst,
                                 I18n::tr("İzlenen Makro Risk", "Monitored Risk", "Beobachtetes Risiko"), sent.macro_risk);

        std::cout << pal.glow_rgb << "\n  📊 " << I18n::tr("ÇOK BOYUTLU QUANT VE YORUMLAMA RAPORU:\n", "MULTI-DIMENSIONAL QUANT DIAGNOSTIC REPORT:\n", "MULTIDIMENSIONALER QUANT-DIAGNOSEBERICHT:\n") << pal.reset_code;

        // 1. Fiyat & VWAP Konum Değerlendirmesi
        if (std::abs(vwap_dev_pct) <= 0.50) {
            std::cout << I18n::tr(
                std::format("  1. Fiyat & Hacim Dengesi: Fiyat ({:.2f} {}), 30-günlük hacim ağırlıklı ortalamaya (VWAP: {:.2f} {}) başa baş (%{:.2f}) seyrediyor.\n"
                            "     Kurumsal alıcıların maliyet seviyesinde adil değerleme (Fair Value) dengesi korunuyor; derin bir satış baskısı değil, konsolidasyon mevcut.\n",
                            data.current_price, currency, data.vwap, currency, vwap_dev_pct),
                std::format("  1. Price & Volume Balance: Price ({:.2f} {}) trades at par ({:+.2f}%) with 30-day Volume Weighted Average Price (VWAP: {:.2f} {}).\n"
                            "     Fair Value equilibrium is preserved at institutional cost basis; consolidation rather than distribution.\n",
                            data.current_price, currency, vwap_dev_pct, data.vwap, currency),
                std::format("  1. Kurs- & Volumengleichgewicht: Kurs ({:.2f} {}) notiert pari ({:+.2f}%) zum 30-Tage-VWAP ({:.2f} {}).\n"
                            "     Fair-Value-Gleichgewicht auf institutioneller Einstandsebene bleibt gewahrt; Konsolidierung.\n",
                            data.current_price, currency, vwap_dev_pct, data.vwap, currency)
            );
        } else if (vwap_dev_pct > 0.50) {
            std::cout << I18n::tr(
                std::format("  1. Fiyat & Hacim Dengesi: Fiyat ({:.2f} {}), 30-günlük VWAP'ın %{:.2f} üzerinde işlem görüyor.\n"
                            "     Kurumsal alıcı maliyetinin üzerinde pozitif momentum ve boğa trendi korunuyor.\n",
                            data.current_price, currency, vwap_dev_pct),
                std::format("  1. Price & Volume Balance: Price ({:.2f} {}) trades +{:.2f}% above 30-day VWAP.\n"
                            "     Positive upward momentum and bullish trend persist above institutional accumulation levels.\n",
                            data.current_price, currency, vwap_dev_pct),
                std::format("  1. Kurs- & Volumengleichgewicht: Kurs ({:.2f} {}) notiert +{:.2f}% über dem 30-Tage-VWAP.\n"
                            "     Positives Aufwärtsmomentum und intakter Aufwärtstrend über institutionellem Niveau.\n",
                            data.current_price, currency, vwap_dev_pct)
            );
        } else {
            std::cout << I18n::tr(
                std::format("  1. Fiyat & Hacim Dengesi: Fiyat ({:.2f} {}), 30-günlük VWAP'ın %{:.2f} altında seyrediyor.\n"
                            "     Son 30 günün ortalama alıcı maliyetinin altında bir düzeltme/kâr satışı baskısı mevcut; destek seviyelerinden ortalamaya dönüş (mean-reversion) izlenmelidir.\n",
                            data.current_price, currency, std::abs(vwap_dev_pct)),
                std::format("  1. Price & Volume Balance: Price ({:.2f} {}) trades {:.2f}% below 30-day VWAP.\n"
                            "     Corrective profit-taking below 30-day buyer cost basis; watch support for potential mean-reversion.\n",
                            data.current_price, currency, std::abs(vwap_dev_pct)),
                std::format("  1. Kurs- & Volumengleichgewicht: Kurs ({:.2f} {}) notiert {:.2f}% unter dem 30-Tage-VWAP.\n"
                            "     Korrektur unter durchschnittlichem Einstandskurs; Unterstützungen für Mean-Reversion beachten.\n",
                            data.current_price, currency, std::abs(vwap_dev_pct))
            );
        }

        // 2. Volatilite ve Rejim Değerlendirmesi
        if (vol_pct < 12.0) {
            std::cout << I18n::tr(
                std::format("  2. Volatilite Rejimi: Yıllıklandırılmış volatilite %{:.1f} ile düşük bantta (Sıkışma).\n"
                            "     Panik veya kriz emareleri yok; kurumsal sermaye kontrollü pozisyon taşıyor. Düşük volatilite genellikle yön belirleyici bir kırılım öncesi görülür.\n", vol_pct),
                std::format("  2. Volatility Regime: Annualized volatility at {:.1f}% in tight band (Compression/Squeeze).\n"
                            "     No panic signs; institutional inventory is stable. Low volatility often precedes directional breakouts.\n", vol_pct),
                std::format("  2. Volatilitätsregime: Annualisierte Volatilität bei {:.1f}% im engen Bereich (Kompression).\n"
                            "     Keine Panikspuren; ruhige Bestände. Niedrige Volatilität geht oft dynamischen Ausbrüchen voraus.\n", vol_pct)
            );
        } else if (vol_pct <= 28.0) {
            std::cout << I18n::tr(
                std::format("  2. Volatilite Rejimi: Yıllıklandırılmış volatilite %{:.1f} ile sağlıklı ve dengeli bir seviyede.\n"
                            "     Piyasa likiditesi güçlü ve kurumsal emir akışı düzenli seyrediyor.\n", vol_pct),
                std::format("  2. Volatility Regime: Annualized volatility at {:.1f}% in balanced, healthy range.\n"
                            "     Market liquidity remains robust and institutional order book flows smoothly.\n", vol_pct),
                std::format("  2. Volatilitätsregime: Annualisierte Volatilität bei {:.1f}% auf ausgewogenem, gesundem Niveau.\n"
                            "     Marktliquidität ist solide und das Orderbuch verläuft geordnet.\n", vol_pct)
            );
        } else {
            std::cout << I18n::tr(
                std::format("  2. Volatilite Rejimi: Yıllıklandırılmış volatilite %{:.1f} ile yüksek risk rejiminde.\n"
                            "     Haber akışına duyarlı geniş dalga boyları mevcut; sermaye korumak için stop-loss mesafeleri genişletilmelidir.\n", vol_pct),
                std::format("  2. Volatility Regime: Annualized volatility at {:.1f}% in high-risk expanded regime.\n"
                            "     Wide swings sensitive to news flow; stop-loss buffers should be appropriately widened.\n", vol_pct),
                std::format("  2. Volatilitätsregime: Annualisierte Volatilität bei {:.1f}% im Bereich erhöhter Schwankungen.\n"
                            "     Nachrichtengetriebene Ausschläge; Stop-Loss-Abstände sollten entsprechend angepasst werden.\n", vol_pct)
            );
        }

        // 3. Sentimel & Teknik Uyuşmazlık (Quant Divergence)
        if (sent.sentiment_score >= 0.25 && vwap_dev_pct < -0.30) {
            std::cout << I18n::tr(
                std::format("  3. Sentimel-Fiyat Uyuşmazlığı: ⭐ BOĞA UYUŞMAZLIĞI (Bullish Divergence) tespit edildi!\n"
                            "     Makro ve kurumsal haber sentimeni pozitif ({}) iken teknik fiyatın kısa vadeli VWAP altında kalması,\n"
                            "     kısa vadeli dalgalanmaların yarattığı bir İSKONTOLU DİP TOPLAMA fırsatına işaret ediyor.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Price Divergence: ⭐ BULLISH DIVERGENCE detected!\n"
                            "     Macro & institutional sentiment is positive ({}) while price remains temporarily below VWAP,\n"
                            "     signaling a DISCOUNT ACCUMULATION opportunity created by transient market noise.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Kurs-Divergenz: ⭐ BULLISCHE DIVERGENZ erkannt!\n"
                            "     Makro- und Nachrichtensentiment positiv ({}), während der Kurs temporär unter dem VWAP notiert;\n"
                            "     signalisiert eine ATTRAKTIVE EINSTIEGSCHANCE bei kurzfristigen Übertreibungen.\n", sent.sentiment_label)
            );
        } else if (sent.sentiment_score <= -0.20 && vwap_dev_pct < -0.30) {
            std::cout << I18n::tr(
                std::format("  3. Sentimel-Fiyat Uyuşmazlığı: ⚠️ AYI TEYİDİ (Bearish Confirmation)!\n"
                            "     Hem makro haber akışı negatif ({}) hem de fiyatlama VWAP altında zayıf seyrediyor.\n"
                            "     Aceleci alım yapılmamalı, destek testi ve taban oluşumu beklenmelidir.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Price Divergence: ⚠️ BEARISH CONFIRMATION!\n"
                            "     Both news sentiment is negative ({}) and technical price trades weak below VWAP.\n"
                            "     Avoid premature buying; await base stabilization and support confirmation.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Kurs-Divergenz: ⚠️ BÄRISCHE BESTÄTIGUNG!\n"
                            "     Sowohl Nachrichtensentiment ist negativ ({}) als auch der Kurs schwach unter dem VWAP.\n"
                            "     Verfrühte Käufe vermeiden; Bodenbildung und Unterstützungszonen abwarten.\n", sent.sentiment_label)
            );
        } else if (sent.sentiment_score >= 0.25 && vwap_dev_pct >= -0.30) {
            std::cout << I18n::tr(
                std::format("  3. Sentimel-Fiyat Uyuşmazlığı: 🚀 TREND TEYİDİ!\n"
                            "     Güçlü pozitif haber akışı ({}) ile kurumsal fiyat dengesi yukarı yönlü hareketi destekliyor.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Price Divergence: 🚀 TREND CONFIRMATION!\n"
                            "     Constructive positive news flow ({}) aligns with price equilibrium to fuel upside.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Kurs-Divergenz: 🚀 TREND-BESTÄTIGUNG!\n"
                            "     Positives Sentiment ({}) bestätigt die konstruktive Kursdynamik nach oben.\n", sent.sentiment_label)
            );
        } else {
            std::cout << I18n::tr(
                std::format("  3. Sentimel-Fiyat Uyuşmazlığı: ⚖️ DENGELİ SEYİR.\n"
                            "     Duygu analizi ({}) ile teknik fiyatlama uyumlu seyrediyor.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Price Divergence: ⚖️ BALANCED REGIME.\n"
                            "     Sentiment analysis ({}) and technical price action move in synchrony.\n", sent.sentiment_label),
                std::format("  3. Sentiment-Kurs-Divergenz: ⚖️ AUSGEWOGENES REGIME.\n"
                            "     Sentiment ({}) und technische Kursentwicklung laufen im Einklang.\n", sent.sentiment_label)
            );
        }

        // 4. Somut Eylem
        std::cout << I18n::tr(
            "  4. Eylem Planı: Bütçenize göre kaç lot alacağınızı, hedef kâr ve kesin zarar-kes (stop) seviyelerini görmek için ana menüden [A] seçiniz.\n",
            "  4. Action Plan: Select [A] from the main menu for precise share sizing, take-profit targets and stop-loss levels.\n",
            "  4. Aktionsplan: Wählen Sie [A] im Hauptmenü für genaue Stückzahlen, Gewinnziele und Stop-Loss-Marken.\n"
        );

        // 5. Hedef Fiyat & Zaman Dilimlerine Göre Yön Tahmin Matrisi
        auto multi_forecast = QuantForecastEngine::generate_multi_horizon(data, sent.sentiment_score, sent.sentiment_label);
        QuantForecastEngine::render_forecast_matrix(multi_forecast);

        // 6. Kullanıcının Belirttiği Özel Vade Tespiti
        int custom_days = 0;
        std::string custom_vade_label = "";
        if (lower_input.find("1 hafta") != std::string::npos || lower_input.find("bir hafta") != std::string::npos ||
            lower_input.find("7 gün") != std::string::npos || lower_input.find("1 week") != std::string::npos ||
            lower_input.find("1 woche") != std::string::npos) {
            custom_days = 7;
            custom_vade_label = I18n::tr("1 Hafta (Kısa Vade)", "1 Week (Short-Term)", "1 Woche (Kurzfristig)");
        } else if (lower_input.find("2 hafta") != std::string::npos || lower_input.find("14 gün") != std::string::npos ||
                   lower_input.find("2 weeks") != std::string::npos || lower_input.find("2 wochen") != std::string::npos) {
            custom_days = 14;
            custom_vade_label = I18n::tr("2 Hafta (14 Gün)", "2 Weeks (14 Days)", "2 Wochen (14 Tage)");
        } else if (lower_input.find("1 ay") != std::string::npos || lower_input.find("bir ay") != std::string::npos ||
                   lower_input.find("30 gün") != std::string::npos || lower_input.find("1 month") != std::string::npos ||
                   lower_input.find("1 monat") != std::string::npos) {
            custom_days = 30;
            custom_vade_label = I18n::tr("1 Ay (30 Gün)", "1 Month (30 Days)", "1 Monat (30 Tage)");
        } else if (lower_input.find("2 ay") != std::string::npos || lower_input.find("60 gün") != std::string::npos ||
                   lower_input.find("2 months") != std::string::npos || lower_input.find("2 monate") != std::string::npos) {
            custom_days = 60;
            custom_vade_label = I18n::tr("2 Ay (60 Gün)", "2 Months (60 Days)", "2 Monate (60 Tage)");
        } else if (lower_input.find("3 ay") != std::string::npos || lower_input.find("üç ay") != std::string::npos ||
                   lower_input.find("90 gün") != std::string::npos || lower_input.find("3 months") != std::string::npos ||
                   lower_input.find("3 monate") != std::string::npos) {
            custom_days = 90;
            custom_vade_label = I18n::tr("3 Ay (Çeyreklik / 90 Gün)", "3 Months (Quarterly / 90 Days)", "3 Monate (Quartal / 90 Tage)");
        } else if (lower_input.find("6 ay") != std::string::npos || lower_input.find("altı ay") != std::string::npos ||
                   lower_input.find("180 gün") != std::string::npos || lower_input.find("6 months") != std::string::npos ||
                   lower_input.find("6 monate") != std::string::npos) {
            custom_days = 180;
            custom_vade_label = I18n::tr("6 Ay (Yarı Yıl)", "6 Months (Half Year)", "6 Monate (Halbjahr)");
        } else if (lower_input.find("1 yıl") != std::string::npos || lower_input.find("bir yıl") != std::string::npos ||
                   lower_input.find("sene") != std::string::npos || lower_input.find("1 year") != std::string::npos ||
                   lower_input.find("1 jahr") != std::string::npos) {
            custom_days = 365;
            custom_vade_label = I18n::tr("1 Yıl (365 Gün)", "1 Year (365 Days)", "1 Jahr (365 Tage)");
        }

        if (custom_days > 0) {
            auto custom_res = QuantForecastEngine::forecast_for_days(data, sent.sentiment_score, custom_days, custom_vade_label);
            std::cout << "\n" << pal.glow_rgb;
            std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
            std::string custom_hdr = I18n::tr(
                std::format("║  🎯 TALEP ETTİĞİNİZ ÖZEL VADE TAHMİN KARTI: {:<33}║", custom_res.horizon_name),
                std::format("║  🎯 REQUESTED CUSTOM HORIZON FORECAST CARD: {:<33}║", custom_res.horizon_name),
                std::format("║  🎯 SPEZIFISCHE ZEITHORIZONT-PROGNOSE: {:<38}║", custom_res.horizon_name)
            );
            std::cout << custom_hdr << "\n";
            std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
            std::cout << pal.reset_code;
            std::cout << std::format("   • {:<24}: {}{}{}\n", I18n::tr("Tahmini Yön Durumu", "Direction Outlook", "Trend-Prognose"), pal.glow_rgb, custom_res.direction_label, pal.reset_code);
            std::cout << std::format("   • {:<24}: {}{:.2f} {} ({:+.2f}%){}\n",
                                     I18n::tr("Beklenen Hedef Fiyat", "Expected Target Price", "Erwarteter Zielkurs"),
                                     pal.glow_rgb, custom_res.target_price, custom_res.currency, custom_res.expected_return_pct, pal.reset_code);
            std::cout << std::format("   • {:<24}: {:.2f} {} ({:+.2f}%)\n",
                                     I18n::tr("Boğa Hedefi (+1σ)", "Bull Target (+1σ)", "Bull-Ziel (+1σ)"),
                                     custom_res.bull_target, custom_res.currency, custom_res.bull_return_pct);
            std::cout << std::format("   • {:<24}: {:.2f} {} ({:+.2f}%)\n",
                                     I18n::tr("Ayı Tabanı  (-1σ)", "Bear Support (-1σ)", "Bär-Boden (-1σ)"),
                                     custom_res.bear_support, custom_res.currency, custom_res.bear_return_pct);
            std::cout << std::format("   • {:<24}: {:+.2f} / 1.00\n", I18n::tr("Quant Skoru", "Quant Score", "Quant-Score"), custom_res.quant_score);
            std::cout << std::format("   • {:<24}: {}\n", I18n::tr("Model Değerlendirmesi", "Model Assessment", "Modellbewertung"), custom_res.rationale);
            std::cout << pal.glow_rgb;
            std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
        }
    }

private:
    static std::string extract_ticker_symbol_(const std::string& input, const std::string& lower_input) {
        // 1. Doğrudan Şirket / Emtia Adı Eşleşmesi (TR, EN, DE)
        if (lower_input.find("ons gümüş") != std::string::npos || lower_input.find("ons gumus") != std::string::npos ||
            lower_input.find("silver ounce") != std::string::npos || lower_input.find("unze silber") != std::string::npos) return "SI=F";
        if (lower_input.find("gram gümüş") != std::string::npos || lower_input.find("gram gumus") != std::string::npos ||
            lower_input.find("gümüş") != std::string::npos || lower_input.find("gumus") != std::string::npos ||
            lower_input.find("silver") != std::string::npos || lower_input.find("silber") != std::string::npos) return "GRAM-GUMUS.TL";
        if (lower_input.find("ons altın") != std::string::npos || lower_input.find("ons altin") != std::string::npos ||
            lower_input.find("gold ounce") != std::string::npos || lower_input.find("unze gold") != std::string::npos || lower_input == "ons") return "GC=F";
        if (lower_input.find("gram altın") != std::string::npos || lower_input.find("gram altin") != std::string::npos ||
            lower_input.find("altın") != std::string::npos || lower_input.find("altin") != std::string::npos ||
            lower_input.find("gold") != std::string::npos || lower_input == "gram") return "GRAM-ALTIN.TL";
        if (lower_input.find("deere") != std::string::npos || lower_input.find("john deere") != std::string::npos) return "DE";
        if (lower_input.find("caterpillar") != std::string::npos) return "CAT";
        if (lower_input.find("rheinmetall") != std::string::npos) return "RHM.DE";
        if (lower_input.find("aselsan") != std::string::npos) return "ASELS.IS";
        if (lower_input.find("türk hava yolları") != std::string::npos || lower_input.find("turk hava yollari") != std::string::npos || lower_input.find(" thy") != std::string::npos || lower_input == "thy") return "THYAO.IS";
        if (lower_input.find("tüpraş") != std::string::npos || lower_input.find("tupras") != std::string::npos) return "TUPRS.IS";
        if (lower_input.find("ereğli") != std::string::npos || lower_input.find("eregli") != std::string::npos) return "EREGL.IS";
        if (lower_input.find("şişecam") != std::string::npos || lower_input.find("sisecam") != std::string::npos) return "SISE.IS";
        if (lower_input.find("koç holding") != std::string::npos || lower_input.find("koc holding") != std::string::npos) return "KCHOL.IS";
        if (lower_input.find("sabancı") != std::string::npos || lower_input.find("sabanci") != std::string::npos) return "SAHOL.IS";
        if (lower_input.find("migros") != std::string::npos) return "MGROS.IS";
        if (lower_input.find("pegasus") != std::string::npos) return "PGSUS.IS";
        if (lower_input.find("palantir") != std::string::npos) return "PLTR";
        if (lower_input.find("nvidia") != std::string::npos) return "NVDA";
        if (lower_input.find("apple") != std::string::npos) return "AAPL";
        if (lower_input.find("microsoft") != std::string::npos) return "MSFT";
        if (lower_input.find("tesla") != std::string::npos) return "TSLA";
        if (lower_input.find("amazon") != std::string::npos) return "AMZN";
        if (lower_input.find("google") != std::string::npos) return "GOOGL";
        if (lower_input.find("bitcoin") != std::string::npos) return "BTC-USD";
        if (lower_input.find("ethereum") != std::string::npos) return "ETH-USD";

        // 2. Metni tokenize edip olası borsa sembollerini ayıkla
        std::vector<std::string> raw_tokens;
        std::string cur = "";
        for (char ch : input) {
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '-' || ch == '^') {
                cur.push_back(ch);
            } else {
                if (!cur.empty()) {
                    raw_tokens.push_back(cur);
                    cur.clear();
                }
            }
        }
        if (!cur.empty()) raw_tokens.push_back(cur);

        // Türkçe, İngilizce & Almanca Durak Kelimeler (Stopwords)
        static const std::unordered_set<std::string> stopwords = {
            "analiz", "et", "fiyat", "fiyati", "fiyatı", "hedef", "tahmin", "yon", "yön", "durum", "durumu",
            "rapor", "nasil", "nasıl", "nedir", "kac", "kaç", "grafik", "grafigi", "grafiği", "al", "sat",
            "hakkinda", "hakkında", "yorum", "goster", "göster", "test", "backtest", "canli", "canlı",
            "piyasa", "borsa", "hisse", "hisseler", "tavsiye", "nerede", "para", "kazan", "1", "2", "3", "4", "5",
            "6", "7", "8", "9", "10", "12", "14", "15", "20", "30", "45", "60", "90", "120", "180", "365",
            "ay", "aylik", "aylık", "hafta", "haftalik", "haftalık", "gun", "gün", "gunluk", "günlük",
            "sene", "yil", "yıl", "yillik", "yıllık", "vade", "vadeli", "bir", "iki", "uc", "üç", "dort", "dört",
            "bes", "beş", "on", "ve", "ile", "icin", "için", "mi", "mı", "mu", "mü", "lutfen", "lütfen",
            "copilot", "quant", "ai", "merhaba", "selam", "hello", "hi", "hey", "analyze", "forecast", "price",
            "target", "stock", "stocks", "market", "markets", "show", "view", "check", "get", "what", "is", "the",
            "of", "in", "to", "for", "on", "me", "tell", "predict", "direction", "trend", "bilgi",
            "ver", "bak", "incele",
            // German stopwords
            "aktie", "aktien", "kurs", "kurse", "ziel", "zielkurs", "prognose", "vorhersage",
            "analyse", "analysiere", "zeige", "zeigen", "bericht", "markt", "märkte", "nachricht",
            "nachrichten", "zins", "zinsen", "anleihe", "anleihen", "deutschland", "türkei",
            "geld", "anlegen", "investieren", "strategie", "empfehlung", "chance", "unter", "dem",
            "radar", "wie", "was", "wo", "wer", "warum", "bitte", "danke", "guten", "tag",
            "woche", "wochen", "monat", "monate", "jahr", "jahre", "tage", "kurzfristig", "mittelfristig",
            "langfristig", "hoch", "tief", "kauf", "verkauf", "kaufen", "verkaufen", "haben", "sein",
            "und", "auf", "mit", "von", "aus", "bei", "nach", "über", "durch",
            // English additional
            "invest", "investing", "investment", "portfolio", "advice", "advise", "recommendation",
            "recommend", "yield", "yields", "bond", "bonds", "treasury", "treasuries", "crash",
            "plunge", "squeeze", "crowded", "hidden", "alpha", "screener", "scan", "scanner",
            "day", "days", "week", "weeks", "month", "months", "year", "years", "short", "mid",
            "long", "term", "buy", "sell", "holding", "give", "thanks", "thank"
        };

        static const std::unordered_set<std::string> bist_names = {
            "THYAO", "ASELS", "TUPRS", "BIMAS", "GARAN", "EREGL", "AKBNK", "KCHOL", "SISE", "SAHOL",
            "FROTO", "PGSUS", "MGROS", "ENKAI", "TAVHL", "CCOLA", "ALARK", "ASTOR", "KRDMD", "PETKM",
            "TCELL", "TTKOM", "ISCTR", "YKBNK", "HALKB", "VAKBN", "TOASO", "ARCLK", "SOKM", "EKGYO",
            "SASA", "HEKTS", "GUBRF", "KOZAL", "KOZAA", "IPEKE", "ODAS", "CANTE", "VESTL", "VESBE",
            "ISGYO", "CIMSA", "AKSEN", "OYAKC", "KONTR", "EUPWR", "GESAN", "SMRTG", "ALFAS"
        };

        static const std::unordered_set<std::string> xetra_names = {
            "RHM", "SAP", "SIE", "ALV", "BMW", "MBG", "VOW3", "AIR", "HNR", "AIXA", "NEM", "BC8",
            "CON", "HEI", "BAS", "BAYN", "DTE", "P911", "ADS", "IFX", "DBK", "DPW", "MUV2", "ZAL", "QIA"
        };

        for (const auto& tok : raw_tokens) {
            std::string lower_tok = tok;
            std::transform(lower_tok.begin(), lower_tok.end(), lower_tok.begin(), ::tolower);
            if (stopwords.find(lower_tok) != stopwords.end()) continue;
            if (tok.length() > 10) continue;

            std::string upper_tok = tok;
            std::transform(upper_tok.begin(), upper_tok.end(), upper_tok.begin(), ::toupper);

            if (upper_tok == "BTC") return "BTC-USD";
            if (upper_tok == "ETH") return "ETH-USD";
            if (upper_tok == "SOL") return "SOL-USD";

            if (upper_tok.find('.') != std::string::npos || upper_tok.find('-') != std::string::npos || upper_tok.starts_with("^")) {
                return upper_tok;
            }

            if (bist_names.find(upper_tok) != bist_names.end()) {
                return upper_tok + ".IS";
            }

            if (xetra_names.find(upper_tok) != xetra_names.end()) {
                return upper_tok + ".DE";
            }

            // 1-5 karakterli her borsa sembolü (DE, CAT, NVDA, AAPL, MSFT, INTC, AMD, PLTR, vb.)
            if (upper_tok.length() >= 1 && upper_tok.length() <= 6) {
                return upper_tok;
            }
        }

        return "";
    }
};

} // namespace quant
