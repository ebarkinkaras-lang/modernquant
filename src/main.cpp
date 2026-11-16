#ifndef BOOST_JSON_NO_LIB
#define BOOST_JSON_NO_LIB
#endif
#include <boost/json/src.hpp>

#include "quant/concepts.hpp"
#include "quant/dates.hpp"
#include "quant/daycount.hpp"
#include "quant/payoff.hpp"
#include "quant/option_info.hpp"
#include "quant/black_scholes.hpp"
#include "quant/monte_carlo.hpp"
#include "quant/binomial_lattice.hpp"
#include "quant/yield_curve.hpp"
#include "quant/bond.hpp"
#include "quant/portfolio_linear_algebra.hpp"
#include "quant/trading_signals.hpp"
#include "quant/live_market_data.hpp"
#include "quant/fundamentals.hpp"
#include "quant/news_sentiment.hpp"
#include "quant/test_suite.hpp"
#include "quant/theme_engine.hpp"
#include "quant/terminal_chart.hpp"
#include "quant/backtester.hpp"
#include "quant/pairs_trading.hpp"
#include "quant/copilot.hpp"
#include "quant/advisor.hpp"
#include "quant/forecast_engine.hpp"
#include "quant/forecast_backtest.hpp"
#include "quant/alpha_screener.hpp"
#include "quant/data_quality.hpp"
#include "quant/hft_lob_engine.hpp"
#include "quant/cross_sectional_lab.hpp"
#include "quant/global_report_pdf.hpp"
#include "quant/i18n.hpp"
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <iostream>
#include <string>
#include <iomanip>
#include <format>
#include <chrono>
#include <cmath>
#include <numeric>
#include <array>
#include <map>
#include <sstream>
#include <unordered_map>
#include <ranges>
#include <thread>

using namespace quant;

const std::string RETRO_BANNER = R"(
   ___    __           _____              __   _          
  / _ |  / /__ ____   /_  _/______ _____/ /__(_)__  ___ _ 
 / __ | / / _ `/ _ \   / / / __/ _ `/ _  / _  / _ \/ _ `/ 
/_/ |_|/_/\_, /\___/  /_/ /_/  \_,_/\_,_/\_,_/_//_/\_, /  
          /___/                  C++20 Terminal   /___/   
)";

void print_banner(bool warmup = false) {
    auto& te = ThemeEngine::instance();
    if (warmup) {
        te.play_crt_warmup(RETRO_BANNER);
    } else {
        std::cout << "\033[2J\033[H";
        std::cout << te.palette().glow_rgb << RETRO_BANNER << te.palette().reset_code << "\n";
    }

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::string time_str = std::format("{:02d}-{:02d}-{:04d} {:02d}:{:02d}:{:02d}",
                                       now_tm.tm_mday, now_tm.tm_mon + 1, now_tm.tm_year + 1900,
                                       now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);

    std::cout << std::format("  {}  │  {}  │  {}{} {}  │  {}{}\n\n",
                             time_str, I18n::subtitle_terminal(),
                             I18n::theme_label(), te.palette().icon, te.palette().name,
                             I18n::language_label(), I18n::lang_flag() + " " + I18n::lang_name());
}

void print_status_bar() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    te.print_rule();
    std::cout << std::format("  {}{}{}\n", pal.dim_rgb, I18n::status_sources(), pal.reset_code);
    std::cout << std::format("  {}{}{}\n", pal.dim_rgb, I18n::status_standards(), pal.reset_code);
    te.print_rule();
    std::cout << "\n";
}

void print_menu() {
    auto& te = ThemeEngine::instance();
    const auto& p = te.palette();

    std::string menu_hdr = std::format("┌─────────────────────────────────[ {} ]──────────────────────────────────┐\n", I18n::menu_title());
    std::cout << p.glow_rgb << menu_hdr << p.reset_code;

    struct MenuItem {
        std::string key;
        std::string tag;
        std::string title;
        std::string desc;
    };

    std::vector<MenuItem> menu = {
        {"A",  "[ADVISOR]",
         I18n::tr("Sermaye Dağıtım & Tavsiye", "Portfolio Allocation & Advice", "Kapitalallokation & Beratung"),
         I18n::tr("Paramı nereye, nasıl yatırayım? Somut Quant Strateji", "Concrete Quant allocation, risk & share sizing", "Konkrete Quant-Allokation, Risiko & Stückzahlen")},
        {"S",  "[SCREEN]",
         I18n::tr("Asimetrik Alfa & FAVÖK Tarama", "Asymmetric Alpha & EBITDA Scan", "Alpha- & EBITDA-Screener"),
         I18n::tr("OHLCV kanal, sıkışma & kur-uyumlu FAVÖK çarpanları", "OHLCV channel, squeeze & FX-aligned EBITDA multiples", "OHLCV-Kanal, Squeeze & EBITDA-Multiples")},
        {"F",  "[FORECAST]",
         I18n::tr("Hedef Fiyat & FAVÖK Bilanço", "Target Price & EBITDA Card", "Zielkurs & EBITDA-Karte"),
         I18n::tr("252-İşG kalibre ±1σ tahmin + FAVÖK/Emtia ek bilgisi", "252-TrdD ±1σ forecast + EBITDA/Commodity card", "252-HT ±1σ-Prognose + EBITDA-/Rohstoffkarte")},
        {"C",  "[COPILOT]",
         I18n::tr("AI Quant Copilot", "AI Quant Copilot", "KI-Quant-Copilot"),
         I18n::tr("Doğal dille canlı analiz, Black-Scholes & makro soruları", "Natural language market intelligence & macro Q&A", "Natürlichsprachige Markt-Intelligenz & Makro Q&A")},
        {"G",  "[CHART]",
         I18n::tr("Terminal Mum Grafiği", "Terminal Candlestick Chart", "Terminal-Kerzenchart"),
         I18n::tr("100% Terminal içi Bloomberg stili grafik, Hacim, SMA & RSI", "In-terminal Bloomberg-style chart, Volume, SMA & RSI", "Bloomberg-artiger Terminal-Chart, Volumen, SMA & RSI")},
        {"1",  "[DATA]",
         I18n::tr("Canlı Piyasa & Veri Motoru", "Live Market & Data Engine", "Live-Markt & Daten-Engine"),
         I18n::tr("Yahoo Finance API: Hisse, Kripto, VWAP, Volatilite", "Yahoo Finance API: Equities, Crypto, VWAP, Volatility", "Yahoo Finance API: Aktien, Krypto, VWAP, Volatilität")},
        {"2",  "[BACK]",
         I18n::tr("Algoritmik Backtest", "Algorithmic Backtester", "Algorithmischer Backtester"),
         I18n::tr("SMA, Bollinger, RSI, RL & OFI Stratejileri", "SMA, Bollinger, RSI, RL & OFI Strategies", "SMA, Bollinger, RSI, RL & OFI-Strategien")},
        {"3",  "[LIVE]",
         I18n::tr("Gerçek Zamanlı İzleyici", "Real-Time Streaming Signals", "Echtzeit-Signal-Streamer"),
         I18n::tr("Boost Accumulators akışkan sinyalleri (BBands & Crossover)", "Boost Accumulators rolling signals (BBands & Crossover)", "Boost Accumulators rollierende Signale")},
        {"4",  "[QUANT]",
         I18n::tr("Türev & Opsiyon Motoru", "Derivatives & Options Engine", "Derivate- & Options-Engine"),
         I18n::tr("Black-Scholes, Heston, Merton, IV & MC", "Black-Scholes, Heston, Merton, IV & MC", "Black-Scholes, Heston, Merton, IV & MC")},
        {"5",  "[PORT]",
         I18n::tr("Portföy Risk & Faktör Analizi", "Portfolio Risk & Factor Model", "Portfolio-Risiko & Faktormodell"),
         I18n::tr("Eigen ω^TΣω, Cholesky, DPO (PSO/RL) & QR", "Eigen ω^TΣω, Cholesky, DPO (PSO/RL) & QR", "Eigen ω^TΣω, Cholesky, DPO (PSO/RL) & QR")},
        {"6",  "[MACRO]",
         I18n::tr("Sabit Getiri & Hazine Eğrisi", "Fixed Income & Yield Curve", "Festverzinsliche & Zinskurve"),
         I18n::tr("C++20 ChronoDate, Bootstrapping, Düzensiz Tahviller, PCA", "C++20 ChronoDate, Bootstrapping, Stub Bonds, PCA", "C++20 ChronoDate, Bootstrapping, Stub-Bonds, PCA")},
        {"7",  "[STAT]",
         I18n::tr("Pairs Trading & Arbitraj", "Statistical Arbitrage / Pairs", "Statistische Arbitrage / Pairs"),
         I18n::tr("Kointegrasyon, Spread Z-Score, Ornstein-Uhlenbeck Half-Life", "Cointegration, Spread Z-Score, OU Mean Reversion", "Kointegration, Spread Z-Score, OU Mean Reversion")},
        {"8",  "[RISK]",
         I18n::tr("Drawdown & VaR Analizi", "Drawdown & VaR Risk Analytics", "Drawdown- & VaR-Risikoanalyse"),
         I18n::tr("Monte Carlo Shuffle, Parametrik & Nonparametrik Max DD", "Monte Carlo Shuffle, Parametric & Historical Max DD", "Monte-Carlo-Shuffle, Parametrischer & Hist. Max DD")},
        {"9",  "[HFT/ML]",
         I18n::tr("HFT Emir Defteri, IOR & Cache", "HFT LOB, IOR Router & Cache", "HFT-Orderbuch, IOR & Cache"),
         I18n::tr("O(1) LOB, RMS, DRL IOR & Cache Bench", "O(1) LOB, RMS, DRL IOR & Cache Bench", "O(1)-LOB, RMS, DRL-IOR & Cache-Bench")},
        {"D",  "[QUALITY]",
         I18n::tr("Veri Kalite & Kaynak Sağlık Panosu", "Data Quality & Source Health Dashboard", "Datenqualitäts- & Quellen-Gesundheitspanel"),
         I18n::tr("API önbellek, FX kuru yaşı, veri bütünlüğü ve tamlık metrikleri", "API cache, FX rate age, data integrity & completeness metrics", "API-Cache, FX-Kursalter, Datenintegrität & Vollständigkeitsmetriken")},
        {"L",  "[LANG]",
         I18n::tr("Dil / Language / Sprache", "Language / Dil / Sprache", "Sprache / Language / Dil"),
         I18n::tr("Türkçe 🇹🇷 / English 🇬🇧 / Deutsch 🇩🇪", "Switch interface language", "Aktive Oberflächensprache umschalten")},
        {"T",  "[TEMA]",
         I18n::tr("Renk & 80s CRT Teması", "CRT Theme & Color Palette", "CRT-Design & Farbpalette"),
         I18n::tr("Amber VT220, Yeşil VT100, VAX Cyan, Neon, CRT Titreşim", "Amber VT220, Green VT100, VAX Cyan, Neon Glow", "Bernstein VT220, Grün VT100, VAX Cyan, Neon-Glow")},
        {"V",  "[TEST]",
         I18n::tr("Otomatik Doğrulama Testi", "Automated Verification Suite", "Automatische Testsuite"),
         I18n::tr("Tüm kitap formülleri ve ground-truth numerik doğrulaması", "Full textbook numerical & invariant verification", "Vollständige numerische Lehrbuch-Verifikation")},
        {"W",  "[WEB-UI]",
         I18n::tr("Görsel Araştırma Panosu (Web)", "Visual Research Dashboard (Web)", "Visuelles Forschungs-Dashboard"),
         I18n::tr("React + FastAPI interaktif IC, ısı haritası & portföy UI", "React + FastAPI interactive IC, heatmap & portfolio UI", "React + FastAPI interaktive IC-, Heatmap- & Trade-UI")},
        {"R",  "[REPORT]",
         I18n::tr("Küresel 1Ay Makro & Borsa PDF", "Global 1M Macro & Market PDF", "Globaler 1M-Makrobericht PDF"),
         I18n::tr("ABD, XETRA, BIST teknik/temel analiz + Dünya Ekonomisi PDF", "US, XETRA, BIST tech/fund analysis + World Macro 1M PDF", "USA, XETRA, BIST Analyse + Weltwirtschaft 1M-PDF")},
        {"11", "[INFO]",
         I18n::tr("Hakkında & Mimari", "About & Architecture", "Über das Terminal & Architektur"),
         I18n::tr("Kitap kaynakları, C++ standartları ve proje detayları", "Book sources, ISO C++ standards & project overview", "Buchquellen, ISO C++ Standards & Projektübersicht")},
        {"0",  "[EXIT]",
         I18n::tr("Çıkış", "Exit", "Beenden"),
         I18n::tr("Terminal uygulamasını kapat", "Shut down terminal session", "Terminal-Sitzung beenden")}
    };

    for (const auto& item : menu) {
        std::string key_col   = ThemeEngine::pad_right(item.key, 3);
        std::string tag_col   = ThemeEngine::pad_right(item.tag, 10);
        std::string title_col = ThemeEngine::pad_right(item.title, 29);
        std::string desc_col  = ThemeEngine::pad_right(item.desc, 33);
        std::cout << "│ " << p.glow_rgb << key_col << p.reset_code << " "
                  << tag_col << " "
                  << p.base_rgb << title_col << p.reset_code << " "
                  << p.dim_rgb << desc_col << p.reset_code << " │\n";
    }
    std::cout << p.glow_rgb << "└───────────────────────────────────────────────────────────────────────────────┘\n" << p.reset_code;
    std::cout << p.glow_rgb << "\n" << I18n::select_prompt() << p.reset_code;
}

void run_language_selector() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb;
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  🌍 DİL SEÇİMİ / LANGUAGE SELECTION / SPRACHAUSWAHL                          ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << pal.reset_code;
    std::cout << "  [1] 🇹🇷 Türkçe  (Varsayılan / Default)\n"
              << "  [2] 🇬🇧 English  (International)\n"
              << "  [3] 🇩🇪 Deutsch  (German)\n\n"
              << I18n::select_prompt();
    std::string choice;
    std::getline(std::cin, choice);
    if (choice == "1") {
        I18n::set_lang(Language::Turkish);
        std::cout << pal.green_rgb << "\n✓ Dil başarıyla 'Türkçe' olarak ayarlandı.\n" << pal.reset_code;
    } else if (choice == "2") {
        I18n::set_lang(Language::English);
        std::cout << pal.green_rgb << "\n✓ Language successfully switched to 'English'.\n" << pal.reset_code;
    } else if (choice == "3") {
        I18n::set_lang(Language::German);
        std::cout << pal.green_rgb << "\n✓ Sprache erfolgreich auf 'Deutsch' umgestellt.\n" << pal.reset_code;
    }
}

void run_forecast_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [F] HEDEF FİYAT, YÖN TAHMİNİ & FAVÖK BİLANÇO EK BİLGİ MOTORU ---",
                          "--- [F] TARGET PRICE, DIRECTION FORECAST & EBITDA FUNDAMENTAL CARD ---",
                          "--- [F] ZIELKURS, RICHTUNGSPROGNOSE & EBITDA-FUNDAMENTALKARTE ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Analiz edilecek hisse/emtia sembolünü girin\n  (örn: THYAO.IS, ASTOR.IS, GARAN.IS, AAPL, RHM.DE, GRAM-ALTIN, GRAM-GUMUS, GC=F, SPY) [Default: THYAO.IS]: ",
                          "Enter equity/commodity symbol to analyze\n  (e.g., THYAO.IS, ASTOR.IS, AAPL, RHM.DE, GRAM-ALTIN, GRAM-GUMUS, GC=F, SPY) [Default: THYAO.IS]: ",
                          "Zu analysierendes Aktien-/Rohstoffsymbol eingeben\n  (z.B. THYAO.IS, AAPL, RHM.DE, GRAM-ALTIN, GC=F, SPY) [Default: THYAO.IS]: ");
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "THYAO.IS";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    std::cout << I18n::tr(std::format("Canlı 1-yıllık OHLCV, haber akışı ve TTM FAVÖK/bilanço verileri çekiliyor ({})... ", sym),
                          std::format("Fetching live 1-year OHLCV, news & TTM EBITDA/fundamentals ({})... ", sym),
                          std::format("Live 1-Jahres-OHLCV, News & TTM-EBITDA werden geladen ({})... ", sym))
              << std::flush;

    auto data = client.fetch_asset_smart(sym, "1y");
    sym = data.symbol;

    if (data.current_price <= 0.0) {
        std::cout << pal.red_rgb << "\n  ❌ "
                  << I18n::tr(std::format("'{}' sembolü için geçerli bir borsa kotasyonu bulunamadı!\n", sym),
                              std::format("Could not find a valid market quote for '{}'!\n", sym),
                              std::format("Kein gültiger Börsenkurs für '{}' gefunden!\n", sym))
                  << pal.reset_code;
        return;
    }

    auto sent = NewsSentimentEngine::get_asset_sentiment(data.symbol, data.long_name, client);
    auto fund = FundamentalEngine::fetch_fundamentals(client, data);
    DataQualityDashboard::record_asset_quality(data, &fund);
    DataQualityDashboard::record_news_fetch(data.symbol, sent.has_live_news, sent.headlines.size());
    TreasuryYieldCurveData yc;
    bool has_yc = false;
    if (fund.asset_class == AssetClassType::CommodityOrFuture || data.currency_code == "USD") {
        yc = client.fetch_treasury_yields();
        has_yc = true;
    }

    std::cout << pal.green_rgb
              << I18n::tr("TAMAMLANDI!\n", "COMPLETED!\n", "ABGESCHLOSSEN!\n")
              << pal.reset_code;

    auto multi = QuantForecastEngine::generate_multi_horizon(
        data, sent.sentiment_score, sent.sentiment_label, sent.has_live_news,
        &fund, has_yc ? &yc : nullptr, &sent
    );
    QuantForecastEngine::render_forecast_matrix(multi);

    std::cout << I18n::tr("\nÖzel bir gün sayısı (takvim günü) sorgulamak ister misiniz? (Örn: 14, 45, 180 veya Enter'a basıp geçin): ",
                          "\nQuery a custom forecast horizon in calendar days? (e.g., 14, 45, 180 or press Enter to skip): ",
                          "\nSpezifischen Zeithorizont in Kalendertagen abfragen? (z.B. 14, 45, 180 oder Enter): ");
    std::string days_str;
    std::getline(std::cin, days_str);
    if (!days_str.empty()) {
        try {
            int d = std::stoi(days_str);
            if (d > 0) {
                auto custom_res = QuantForecastEngine::forecast_for_days(
                    data, sent.sentiment_score, d, "", sent.has_live_news, &fund, has_yc ? &yc : nullptr, &sent
                );
                std::string card_hdr = I18n::tr(
                    std::format("║  🎯 {} TAKVİM GÜNÜ ({} İŞ GÜNÜ) ÖZEL VADE TAHMİN KARTI", d, custom_res.trading_days),
                    std::format("║  🎯 {}-CALENDAR-DAY ({}-TRADING-DAY) CUSTOM HORIZON CARD", d, custom_res.trading_days),
                    std::format("║  🎯 {}-KALENDERTAGE ({}-HANDELSTAGE) SPEZIAL-PROGNOSE", d, custom_res.trading_days)
                );
                int cpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(card_hdr));
                std::cout << "\n" << pal.glow_rgb;
                std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
                std::cout << card_hdr << std::string(static_cast<size_t>(cpad), ' ') << "║\n";
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
        } catch (...) {}
    }
}

void run_chart_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [G] TERMINAL MUM GRAFİK MOTORU ---",
                          "--- [G] TERMINAL CANDLESTICK CHART ENGINE ---",
                          "--- [G] TERMINAL-KERZENCHART-ENGINE ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Grafik için sembol girin (örn: SPY, AAPL, NVDA, GRAM-ALTIN, THYAO.IS) [Default: SPY]: ",
                          "Enter symbol for chart (e.g., SPY, AAPL, NVDA, GRAM-ALTIN, THYAO.IS) [Default: SPY]: ",
                          "Symbol für Chart eingeben (z.B. SPY, AAPL, NVDA, GRAM-ALTIN, THYAO.IS) [Default: SPY]: ");
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    std::cout << I18n::tr(std::format("Piyasa verileri alınıyor ({})... ", sym),
                          std::format("Fetching market data ({})... ", sym),
                          std::format("Marktdaten werden geladen ({})... ", sym))
              << std::flush;
    auto data = client.fetch_asset_smart(sym, "6mo");
    if (data.current_price <= 0.0 || data.historical_prices.empty()) {
        std::cout << pal.red_rgb << "\n  ❌ Geçerli fiyat serisi bulunamadı!\n" << pal.reset_code;
        return;
    }
    auto fund = FundamentalEngine::fetch_fundamentals(client, data);
    auto curve = client.fetch_treasury_yields();
    DataQualityDashboard::record_asset_quality(data, &fund);
    std::cout << pal.green_rgb << I18n::tr("TAMAMLANDI!\n", "COMPLETED!\n", "ABGESCHLOSSEN!\n") << pal.reset_code;

    TerminalChartEngine::render_asset(data, 12, 28);
    FundamentalEngine::render_fundamental_card(fund, data, &curve);
}

void run_live_data_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [1] CANLI PİYASA, OHLCV ANALİTİĞİ & TEMEL VERİ YÖNETİCİSİ ---",
                          "--- [1] LIVE MARKET, OHLCV ANALYTICS & FUNDAMENTAL DATA MANAGER ---",
                          "--- [1] LIVE-MARKT, OHLCV-ANALYTIK & FUNDAMENTALDATEN-MANAGER ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Sembol girin (örn: SPY, AAPL, THYAO.IS, ASTOR.IS, GRAM-ALTIN, BTC-USD) [Default: SPY]: ",
                          "Enter symbol (e.g., SPY, AAPL, THYAO.IS, ASTOR.IS, GRAM-ALTIN, BTC-USD) [Default: SPY]: ",
                          "Symbol eingeben (z.B. SPY, AAPL, THYAO.IS, GRAM-ALTIN, BTC-USD) [Default: SPY]: ");
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    std::cout << I18n::tr(std::format("Canlı REST API & Bilanço sorgusu yapılıyor: {}... ", sym),
                          std::format("Querying live REST API & Fundamentals: {}... ", sym),
                          std::format("Live-REST-API & Bilanzdaten werden geladen: {}... ", sym))
              << std::flush;
    auto data = client.fetch_asset_smart(sym, "6mo");
    if (data.current_price <= 0.0) {
        std::cout << pal.red_rgb << "\n  ❌ Sembol bulunamadı!\n" << pal.reset_code;
        return;
    }
    auto fund = FundamentalEngine::fetch_fundamentals(client, data);
    auto curve = client.fetch_treasury_yields();
    DataQualityDashboard::record_asset_quality(data, &fund);
    std::cout << pal.green_rgb << "OK!\n" << pal.reset_code;

    std::vector<std::string> lines = {
        std::format("{:<26}: {}{}{} ({})", I18n::tr("Varlık Sembolü", "Asset Symbol", "Anlagesymbol"),
                    pal.glow_rgb, data.symbol, pal.reset_code, data.long_name.empty() ? data.instrument_type : data.long_name),
        std::format("{:<26}: {}", I18n::tr("Veri Akış Durumu", "Data Stream State", "Datenstrom-Status"),
                    data.is_live ? (pal.green_rgb + I18n::tr("CANLI PİYASA AKIŞI AKTİF", "LIVE MARKET FEED ACTIVE", "LIVE-MARKTFEED AKTIV") + pal.reset_code) :
                                   (pal.red_rgb + I18n::tr("ÇEVRİMDIŞI SİMÜLASYON", "OFFLINE SIMULATION", "OFFLINE-SIMULATION") + pal.reset_code)),
        std::format("{:<26}: {}{:.2f} {}{}", I18n::tr("Anlık Spot Fiyat", "Current Spot Price", "Aktueller Kassakurs"),
                    pal.glow_rgb, data.current_price, data.currency_display, pal.reset_code),
        std::format("{:<26}: {:.2f} {} (Tipik Fiyat HLC/3 × Hacim)", I18n::tr("22-İşG Kurumsal VWAP", "22-Day Institutional VWAP", "22-Tage-VWAP"),
                    data.vwap, data.currency_display),
        std::format("{:<26}: {:.2f}% (EWMA λ=0.94: {:.2f}% │ Örneklem: {:.2f}%)",
                    I18n::tr("Yıllıklandırılmış Vol.", "Annualized Volatility", "Annualisierte Volatilität"),
                    data.annualized_volatility * 100.0, data.ewma_volatility * 100.0, data.sample_volatility * 100.0),
        std::format("{:<26}: {:.2f} {}  │  Senkronize OHLCV Barı: {} gün",
                    I18n::tr("14-Gün Wilder OHLC ATR", "14-Day Wilder OHLC ATR", "14-Tage-ATR"),
                    data.atr_14, data.currency_display, data.historical_prices.size())
    };

    te.print_box(I18n::tr("PİYASA & MİKROYAPI ÖZETİ", "MARKET & MICROSTRUCTURE SUMMARY", "MARKT- & MIKROSTRUKTUR-ÜBERSICHT"), lines);

    // Render Fundamental / FAVÖK (or Commodity/ETF Macro) Additional Card
    FundamentalEngine::render_fundamental_card(fund, data, &curve);

    // US Treasury Yields
    std::cout << "\n";
    std::string yc_status = curve.is_live ? "[CANLI]" : "[VARSAYILAN]";
    std::vector<std::string> yc_lines = {
        std::format("{}: {:.3f}% {}", I18n::tr("3-Ay Hazine Bonosu (^IRX)", "3-Month T-Bill (^IRX)", "3-Monats T-Bill (^IRX)"), curve.three_month_yield * 100.0, yc_status),
        std::format("{}: {:.3f}%", I18n::tr("5-Yıl Hazine Tahvili (^FVX)", "5-Year T-Note (^FVX)", "5-Jahres T-Note (^FVX)"), curve.five_year_yield * 100.0),
        std::format("{}: {:.3f}%", I18n::tr("10-Yıl Gösterge Tahvil (^TNX)", "10-Year Benchmark (^TNX)", "10-Jahres Benchmark (^TNX)"), curve.ten_year_yield * 100.0),
        std::format("{}: {:.3f}%", I18n::tr("30-Yıl Uzun Vade (^TYX)", "30-Year Long Bond (^TYX)", "30-Jahres Long Bond (^TYX)"), curve.thirty_year_yield * 100.0)
    };
    te.print_box(I18n::tr("ABD HAZİNE GETİRİ EĞRİSİ ORANLARI", "US TREASURY YIELD CURVE RATES", "US-TREASURY ZINSKURVE"), yc_lines);
}

void run_backtest_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [2] ALGORİTMİK BACKTEST & WALK-FORWARD DOĞRULAMA MOTORU ---",
                          "--- [2] ALGORITHMIC BACKTEST & WALK-FORWARD VALIDATION ENGINE ---",
                          "--- [2] ALGORITHMISCHER BACKTEST & WALK-FORWARD-VALIDIERUNG ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Backtest için sembol (örn: THYAO.IS, SPY, NVDA, AAPL, RHM.DE, GRAM-ALTIN) [Default: THYAO.IS]: ",
                          "Enter symbol for backtest (e.g., THYAO.IS, SPY, NVDA, AAPL, RHM.DE, GRAM-ALTIN) [Default: THYAO.IS]: ",
                          "Symbol für Backtest eingeben (z.B. THYAO.IS, SPY, NVDA, AAPL, RHM.DE) [Default: THYAO.IS]: ");
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "THYAO.IS";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    std::cout << I18n::tr(
        "\nBacktest Modeli / Strateji Türü Seçin:\n"
        "  [1] 🎯 Quant Forecast Engine Walk-Forward Doğrulama (2-Yıllık Out-of-Sample Kalibrasyon & Komisyonlu PnL)\n"
        "  [2] Dual SMA Kesişimi (5 / 20 Gün)\n"
        "  [3] Bollinger Bantları Ortalamaya Dönüş (20 Gün, 2.0 Std)\n"
        "  [4] RSI Momentum (14 Gün, 30/70 Sınırları)\n"
        "  [5] 🤖 RL Q-Learning & Experience Replay Ajanı (Silahian Ch.4 Walk-Forward)\n"
        "  [6] ♟  Game-Theoretic Mikro-Yapı Emir Akışı Dengesizliği (Silahian Ch.3/7 OFI)\n"
        "  [7] 🌐 Yatay Kesit Portföy, Sahte-Alfa & Tahmin Edilebilirlik Labı (BIST / US / XETRA + Web Raporu)\n"
        "  [8] 🔍 Sızıntı & Ön-Doğrulama Teşhis Testi (Look-Ahead Seal, Overfit & Sentiment Variance)\n"
        "Seçiminiz [1]: ",

        "\nSelect Backtest Model / Strategy Type:\n"
        "  [1] 🎯 Quant Forecast Engine Walk-Forward Validation (2-Year Out-of-Sample Calibration & Net PnL)\n"
        "  [2] Dual SMA Crossover (5 / 20 Days)\n"
        "  [3] Bollinger Bands Mean Reversion (20 Days, 2.0 Std)\n"
        "  [4] RSI Momentum (14 Days, 30/70 Bounds)\n"
        "  [5] 🤖 RL Q-Learning & Experience Replay Agent (Silahian Ch.4 Walk-Forward)\n"
        "  [6] ♟  Game-Theoretic Microstructure Order Flow Imbalance (Silahian Ch.3/7 OFI)\n"
        "  [7] 🌐 Cross-Sectional Portfolio, Fake-Alpha & Predictability Lab (BIST / US / XETRA + Export)\n"
        "  [8] 🔍 Leakage & Pre-Deployment Sanity Suite (Look-Ahead Seal, Overfit & Sentiment Var)\n"
        "Your Choice [1]: ",

        "\nBacktest-Modell / Strategietyp wählen:\n"
        "  [1] 🎯 Quant Forecast Engine Walk-Forward-Validierung (2-Jahres Out-of-Sample & Netto-PnL)\n"
        "  [2] Dual-SMA-Kreuzung (5 / 20 Tage)\n"
        "  [3] Bollinger-Bänder Mean-Reversion (20 Tage, 2.0 Std)\n"
        "  [4] RSI Momentum (14 Tage, 30/70 Schwellen)\n"
        "  [5] 🤖 RL Q-Learning & Experience Replay Agent (Silahian Kap.4 Walk-Forward)\n"
        "  [6] ♟  Spieltheoretische Order-Flow-Imbalance (Silahian Kap.3/7 OFI)\n"
        "  [7] 🌐 Querschnitts-Portfolio, Fake-Alpha & Prognostizierbarkeit (BIST / US / XETRA + Export)\n"
        "  [8] 🔍 Look-Ahead Leakage & Pre-Deployment-Diagnose (Kausalität, Overfit, Sentiment-Var)\n"
        "Ihre Auswahl [1]: "
    );
    std::string s_choice;
    std::getline(std::cin, s_choice);

    if (s_choice == "7") {
        std::string default_reg = sym.ends_with(".IS") ? "BIST" : (sym.ends_with(".DE") ? "XETRA" : "US");
        std::cout << I18n::tr(
            std::format("\nEvren Seçin (BIST / US / XETRA) [Default: {}]: ", default_reg),
            std::format("\nSelect Equity Universe (BIST / US / XETRA) [Default: {}]: ", default_reg),
            std::format("\nAktienuniversum wählen (BIST / US / XETRA) [Default: {}]: ", default_reg)
        );
        std::string reg_in;
        std::getline(std::cin, reg_in);
        if (reg_in.empty()) reg_in = default_reg;
        std::transform(reg_in.begin(), reg_in.end(), reg_in.begin(), ::toupper);

        std::cout << I18n::tr("Tahmin Ufku (İş Günü: 1, 5, 21) [Default: 5]: ",
                              "Forecast Horizon (Trading Days: 1, 5, 21) [Default: 5]: ",
                              "Prognosehorizont (Handelstage: 1, 5, 21) [Default: 5]: ");
        std::string h_in;
        std::getline(std::cin, h_in);
        int horizon = 5;
        if (!h_in.empty()) { try { horizon = std::clamp(std::stoi(h_in), 1, 21); } catch (...) {} }

        std::cout << pal.glow_rgb << I18n::tr(
            std::format("\n🌐 {} evreni yükleniyor ve yatay kesit + sahte-alfa + tahmin edilebilirlik analizi çalışıyor...\n", reg_in),
            std::format("\n🌐 Fetching {} universe and running cross-sectional + fake-alpha + predictability lab...\n", reg_in),
            std::format("\n🌐 {} Universum wird geladen und Querschnitts-/Fake-Alpha-Analyse läuft...\n", reg_in))
                  << pal.reset_code;
        auto rep = CrossSectionalBacktestEngine::run_regional_study(client, reg_in, horizon, 4, "2y", true);
        CrossSectionalBacktestEngine::render_report(rep);
        return;
    }

    if (s_choice == "8") {
        auto snap = client.fetch_asset_smart(sym, "1y");
        auto diag = CrossSectionalBacktestEngine::run_diagnostic_sanity_checks(
            snap.historical_prices.size() >= 80 ? &snap : nullptr);
        std::vector<std::string> dlines = {
            std::format("{:<28}: {} ({})", I18n::tr("Look-Ahead Nedensellik", "Look-Ahead Causality", "Look-Ahead-Kausalität"),
                        diag.lookahead_audit.passed ? (pal.green_rgb + "PASS ✔" + pal.reset_code) : (pal.red_rgb + "FAIL ✘" + pal.reset_code),
                        diag.lookahead_audit.detail),
            std::format("{:<28}: {} (Yön: %{:.1f})", I18n::tr("Synthetic Overfit Sanity", "Synthetic Overfit Sanity", "Synthetischer Overfit-Test"),
                        diag.synthetic_overfit_passed ? "PASS ✔" : "FAIL ✘", diag.synthetic_overfit_dir_acc * 100.0),
            std::format("{:<28}: {} (σ={:.3f})", I18n::tr("Temporal Sentiment Var.", "Temporal Sentiment Var.", "Sentiment-Zeitvarianz"),
                        diag.temporal_sentiment_var_passed ? "PASS ✔" : "FAIL ✘", diag.temporal_sentiment_std),
            std::format("{:<28}: Pearson r={:+.4f} │ Spearman={:+.4f} │ t={:+.2f} │ p={:.4f}",
                        I18n::tr("Sinyal Kalitesi (p-val)", "Signal Quality (p-val)", "Signalqualität (p-Wert)"),
                        diag.signal_quality.pearson_r, diag.signal_quality.spearman_ic,
                        diag.signal_quality.t_stat, diag.signal_quality.p_value)
        };
        te.print_box(I18n::tr("SIZINTI & ÖN-DOĞRULAMA TEŞHİSİ", "LEAKAGE & DIAGNOSTIC SUITE", "LEAKAGE- & DIAGNOSE-SUITE"), dlines);
        return;
    }

    if (s_choice == "1" || s_choice.empty()) {
        std::cout << I18n::tr(
            "\nTahmin Ufku / Vade Seçin:\n"
            "  [1] 1 Hafta (5 İş Günü - Kısa Vade)\n"
            "  [2] 2 Hafta (10 İş Günü)\n"
            "  [3] 1 Ay    (21 İş Günü - Orta Vade)\n"
            "Seçiminiz [1]: ",

            "\nSelect Forecast Horizon / Period:\n"
            "  [1] 1 Week  (5 Trading Days - Short-Term)\n"
            "  [2] 2 Weeks (10 Trading Days)\n"
            "  [3] 1 Month (21 Trading Days - Mid-Term)\n"
            "Your Choice [1]: ",

            "\nPrognosehorizont / Zeitraum wählen:\n"
            "  [1] 1 Woche  (5 Handelstage - Kurzfristig)\n"
            "  [2] 2 Wochen (10 Handelstage)\n"
            "  [3] 1 Monat  (21 Handelstage - Mittelfristig)\n"
            "Ihre Auswahl [1]: "
        );
        std::string h_choice;
        std::getline(std::cin, h_choice);
        int horizon = 5;
        if (h_choice == "2") horizon = 10;
        else if (h_choice == "3") horizon = 21;

        std::cout << I18n::tr(
            std::format("\n'{}' için 2-Yıllık tarihsel pencereler taranıyor ve birebir canlı motorla out-of-sample doğrulama yapılıyor...\n", sym),
            std::format("\nScanning 2-year historical rolling windows and running exact out-of-sample validation for '{}'...\n", sym),
            std::format("\n2-Jahres-Vergangenheitsfenster werden für '{}' out-of-sample validiert...\n", sym)
        );
        auto snap = client.fetch_asset_smart(sym, "6mo");
        sym = snap.symbol;
        auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, sym, horizon, "2y");
        ForecastBacktestEngine::render_backtest_report(rep);
        if (snap.current_price > 0.0) {
            auto fund = FundamentalEngine::fetch_fundamentals(client, snap);
            auto curve = client.fetch_treasury_yields();
            DataQualityDashboard::record_asset_quality(snap, &fund);
            FundamentalEngine::render_fundamental_card(fund, snap, &curve);
        }
        return;
    }

    auto data = client.fetch_asset_smart(sym, "2y");
    if (data.historical_prices.size() < 25) {
        std::cout << pal.dim_rgb << I18n::tr("Alternatif seri alınıyor...", "Fetching fallback series...", "Alternativserie wird geladen...") << pal.reset_code << "\n";
        data = client.fetch_asset_smart(data.symbol, "1y");
    }
    DataQualityDashboard::record_asset_quality(data);
    if (!data.is_live) {
        std::cout << pal.red_rgb
                  << I18n::tr("\n  ⚠ UYARI: Canlı veri alınamadı, backtest ÇEVRİMDIŞI simülasyon serisi üzerinde çalışıyor!\n",
                              "\n  ⚠ WARNING: Live data unavailable, backtest running on OFFLINE simulated series!\n",
                              "\n  ⚠ WARNUNG: Keine Live-Daten verfügbar, Backtest läuft auf OFFLINE-Simulationsdaten!\n")
                  << pal.reset_code;
    }

    StrategyType st = StrategyType::SmaCrossover;
    if (s_choice == "3") st = StrategyType::BollingerMeanReversion;
    else if (s_choice == "4") st = StrategyType::RsiMomentum;
    else if (s_choice == "5") st = StrategyType::RLDynamicPortfolio;
    else if (s_choice == "6") st = StrategyType::OrderFlowImbalance;

    auto res = BacktestEngine::run_ohlcv(data.historical_prices,
                                         data.historical_highs,
                                         data.historical_lows,
                                         data.historical_volumes,
                                         st);

    std::vector<std::string> lines = {
        std::format("{:<26}: {}{}{}", I18n::tr("Uygulanan Strateji", "Strategy Applied", "Angewandte Strategie"), pal.glow_rgb, res.strategy_name, pal.reset_code),
        std::format("{:<26}: {}{:.2f}%{}", I18n::tr("Strateji Toplam Getiri", "Strategy Total Return", "Gesamtrendite"), (res.total_return_pct >= 0 ? pal.green_rgb : pal.red_rgb), res.total_return_pct, pal.reset_code),
        std::format("{:<26}: {:+.2f}%", I18n::tr("Al-Tut (Benchmark) Getiri", "Buy & Hold (Benchmark)", "Buy & Hold (Benchmark)"), res.benchmark_return_pct),
        std::format("{:<26}: {:.2f}", I18n::tr("Yıllıklandırılmış Sharpe", "Annualized Sharpe", "Annualisierte Sharpe-Ratio"), res.annualized_sharpe),
        std::format("{:<26}: -{:.2f}%", I18n::tr("Maksimum Çekilme (Max DD)", "Maximum Drawdown (Max DD)", "Maximaler Drawdown (Max DD)"), res.max_drawdown_pct),
        std::format("{:<26}: {} {}", I18n::tr("Gerçekleşen İşlem Sayısı", "Executed Trades", "Ausgeführte Trades"), res.total_trades, I18n::tr("adet", "trades", "Trades")),
        std::format("{:<26}: {:.1f}%", I18n::tr("Kazanma Oranı (Win Rate)", "Win Rate", "Gewinnquote (Win Rate)"), res.win_rate_pct)
    };
    te.print_box(I18n::tr("BACKTEST RAPORU", "BACKTEST REPORT", "BACKTEST-BERICHT"), lines);

    if (data.current_price <= 0.0) {
        std::cout << pal.red_rgb << "\n  ❌ " << sym << " için fiyat verisi alınamadı!\n" << pal.reset_code;
    } else if (!data.historical_prices.empty()) {
        auto fund = FundamentalEngine::fetch_fundamentals(client, data);
        auto curve = client.fetch_treasury_yields();
        DataQualityDashboard::record_asset_quality(data, &fund);
        FundamentalEngine::render_fundamental_card(fund, data, &curve);
    }
}

namespace {

int64_t bar_day_key(const RealTimeAssetData& d, size_t idx) {
    if (idx < d.day_keys.size()) return d.day_keys[idx];
    if (idx < d.timestamps.size()) return d.timestamps[idx] / 86400LL;
    return static_cast<int64_t>(idx);
}

std::string format_bar_date(const RealTimeAssetData& d, size_t idx) {
    if (idx < d.timestamps.size() && d.timestamps[idx] > 0) {
        std::time_t tt = static_cast<std::time_t>(d.timestamps[idx]);
        if (std::tm* tm_p = std::gmtime(&tt)) {
            return std::format("{:04d}-{:02d}-{:02d}",
                               tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday);
        }
    }
    return std::format("Bar #{:<5}", idx + 1);
}

std::string safe_ccy_display(const RealTimeAssetData& d) {
    if (!d.currency_display.empty()) return d.currency_display;
    return LiveMarketDataClient::currency_display_for(
        LiveMarketDataClient::infer_currency_code(d.symbol), d.symbol);
}

double resolve_risk_free_rate(const RealTimeAssetData& asset,
                              const TreasuryYieldCurveData& yc,
                              std::string& out_source) {
    if (asset.currency_code == "TRY") {
        out_source = I18n::tr("TRY Politika/Tahvil Proxy (%42.5)",
                              "TRY Policy/Bond Proxy (42.5%)",
                              "TRY-Zins-Proxy (42.5%)");
        return 0.425;
    }
    if (asset.currency_code == "EUR") {
        out_source = I18n::tr("EUR Risk-Free Proxy (%3.25)",
                              "EUR Risk-Free Proxy (3.25%)",
                              "EUR-Referenzzins (3.25%)");
        return 0.0325;
    }
    out_source = yc.is_live
        ? I18n::tr("ABD 3A Hazine ^IRX [CANLI]", "US 3M T-Bill ^IRX [LIVE]", "US-3M-Treasury ^IRX [LIVE]")
        : I18n::tr("ABD 3A Varsayılan (%4.5)", "US 3M Default (4.5%)", "US-3M-Standard (4.5%)");
    return (yc.three_month_yield > 0.0) ? yc.three_month_yield : 0.045;
}

struct HistoricalFxSeries {
    double spot_fx{1.0};
    std::map<int64_t, double> daily_rates;

    double rate_on(int64_t day_key) const {
        if (daily_rates.empty()) return spot_fx;
        auto it = daily_rates.upper_bound(day_key);
        if (it == daily_rates.begin()) return daily_rates.begin()->second;
        return std::prev(it)->second;
    }
};

HistoricalFxSeries build_fx_series(LiveMarketDataClient& client,
                                   const std::string& from_ccy,
                                   const std::string& to_ccy,
                                   const std::string& range = "6mo") {
    HistoricalFxSeries fx;
    if (from_ccy.empty() || to_ccy.empty() || from_ccy == to_ccy) return fx;
    fx.spot_fx = client.get_fx_rate(from_ccy, to_ccy);
    if (fx.spot_fx <= 0.0) fx.spot_fx = 1.0;

    auto direct = client.fetch_asset(from_ccy + to_ccy + "=X", range);
    if (direct.is_live && !direct.historical_prices.empty()) {
        for (size_t i = 0; i < direct.historical_prices.size(); ++i) {
            if (direct.historical_prices[i] > 0.0) {
                fx.daily_rates[bar_day_key(direct, i)] = direct.historical_prices[i];
            }
        }
        return fx;
    }
    auto inverse = client.fetch_asset(to_ccy + from_ccy + "=X", range);
    if (inverse.is_live && !inverse.historical_prices.empty()) {
        for (size_t i = 0; i < inverse.historical_prices.size(); ++i) {
            if (inverse.historical_prices[i] > 0.0) {
                fx.daily_rates[bar_day_key(inverse, i)] = 1.0 / inverse.historical_prices[i];
            }
        }
    }
    return fx;
}

} // namespace

void run_live_signals_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [3] BOOST ACCUMULATORS GERÇEK ZAMANLI SİNYAL İZLEYİCİ ---" << pal.reset_code << "\n";
    std::cout << "Sembol girin (örn: SPY, THYAO.IS, NVDA, BTC-USD, GRAM-ALTIN) [Default: SPY]: ";
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    auto data = client.fetch_asset_smart(sym, "1mo");
    DataQualityDashboard::record_asset_quality(data);
    if (data.historical_prices.empty()) {
        std::cout << pal.red_rgb << "\n  ❌ " << data.symbol << " için tarihsel seri alınamadı!\n" << pal.reset_code;
        return;
    }

    const auto& closes = data.historical_prices;
    std::string ccy = safe_ccy_display(data);
    std::string badge = data.is_live
        ? (pal.green_rgb + "[CANLI PİYASA VERİSİ]" + pal.reset_code)
        : (pal.red_rgb + "[ÇEVRİMDIŞI SİMÜLASYON]" + pal.reset_code);
    std::cout << std::format("  Varlık: {} {} │ Toplam Bar: {}\n\n", data.symbol, badge, closes.size());

    StreamingIndicators ind(5, 1.5, 3, 5);
    size_t start = (closes.size() > 26) ? (closes.size() - 26) : 0;

    std::cout << pal.dim_rgb << "  Tarih      Fiyat       Ort.(5g)    Üst Bant    Alt Bant    Fast MA     Slow MA     Sinyal\n"
              << "  ───────────────────────────────────────────────────────────────────────────────────────────────\n" << pal.reset_code;

    for (size_t i = start; i < closes.size(); ++i) {
        auto pt = ind.update(closes[i]);
        std::string dt = format_bar_date(data, i);
        std::string sig_str = (pt.signal == 1) ? (pal.green_rgb + "[AL / GOLDEN]" + pal.reset_code) :
                              (pt.signal == -1) ? (pal.red_rgb + "[SAT / DEATH]" + pal.reset_code) : (pal.dim_rgb + "BEKLE" + pal.reset_code);
        std::cout << std::format("  {:<10} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2}  {}\n",
                                 dt, pt.price, ccy, pt.rolling_mean, ccy, pt.upper_band, ccy,
                                 pt.lower_band, ccy, pt.fast_ma, ccy, pt.slow_ma, ccy, sig_str);
    }
    if (data.current_price > 0.0 && std::abs(data.current_price - closes.back()) > 1e-4) {
        auto pt = ind.update(data.current_price);
        std::string sig_str = (pt.signal == 1) ? (pal.green_rgb + "[AL / GOLDEN]" + pal.reset_code) :
                              (pt.signal == -1) ? (pal.red_rgb + "[SAT / DEATH]" + pal.reset_code) : (pal.dim_rgb + "BEKLE" + pal.reset_code);
        std::cout << std::format("  {:<10} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2} {:>8.2f}{:<2}  {}\n",
                                 "ANLIK", pt.price, ccy, pt.rolling_mean, ccy, pt.upper_band, ccy,
                                 pt.lower_band, ccy, pt.fast_ma, ccy, pt.slow_ma, ccy, sig_str);
    }
}

void run_derivatives_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [4] QUANT TÜREV & OPSİYON MOTORU ---" << pal.reset_code << "\n";
    std::cout << "  [1] Black-Scholes Analitik Fiyatlama & Greeks Tablosu\n"
              << "  [2] Secant Yöntemi ile Zımni Oynaklık (Implied Volatility)\n"
              << "  [3] Çok Çekirdekli Monte Carlo (std::async) & Nakavt Bariyer Opsiyonları\n"
              << "  [4] Boost MultiArray Binomial Lattice (Avrupa vs Amerikan Erken Egzersiz)\n"
              << "  [5] Silahian Ch.7 Gelişmiş Modeller: Heston, Merton Jump & Dupire Local Vol MC\n"
              << "Seçiminiz [1]: ";
    std::string sub_choice;
    std::getline(std::cin, sub_choice);
    if (sub_choice.empty()) sub_choice = "1";

    std::cout << "Dayanak varlık sembolü (örn: SPY, NVDA, AAPL, THYAO.IS) [Default: SPY]: ";
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    auto live = client.fetch_asset_smart(sym, "6mo");
    auto fund = FundamentalEngine::fetch_fundamentals(client, live);
    auto yc = client.fetch_treasury_yields();
    DataQualityDashboard::record_asset_quality(live, &fund);

    if (live.current_price <= 0.0) {
        std::cout << pal.red_rgb << "\n  ❌ Dayanak varlık kotasyonu alınamadı!\n" << pal.reset_code;
        return;
    }

    double spot = live.current_price;
    double vol = (live.annualized_volatility > 0.01) ? live.annualized_volatility : 0.20;
    std::string rate_src;
    double rate = resolve_risk_free_rate(live, yc, rate_src);
    double div = (!std::isnan(fund.dividend_yield_pct) && fund.dividend_yield_pct > 0.0 && fund.dividend_yield_pct < 25.0)
        ? (fund.dividend_yield_pct / 100.0)
        : (live.symbol == "SPY" ? 0.013 : 0.0);
    std::string ccy = safe_ccy_display(live);
    std::string live_tag = live.is_live ? "[CANLI]" : "[ÇEVRİMDIŞI]";

    if (sub_choice == "1") {
        double strike = std::round(spot * 100.0) / 100.0;
        double time_to_exp = 0.5;

        BlackScholes call_bs{strike, spot, time_to_exp, PayoffType::Call, rate, div};
        BlackScholes put_bs{strike, spot, time_to_exp, PayoffType::Put, rate, div};

        double c_price = call_bs(vol);
        double p_price = put_bs(vol);
        auto c_greeks  = call_bs.risk_values(vol);
        auto p_greeks  = put_bs.risk_values(vol);

        std::vector<std::string> lines = {
            std::format("Dayanak: {} {}  │  Spot: {:.2f} {}  │  Strike (K): {:.2f} {}  │  Vade: {:.2f}y",
                        live.symbol, live_tag, spot, ccy, strike, ccy, time_to_exp),
            std::format("Gerçekleşen Vol: {:.2f}%  │  Faiz ({}): {:.2f}%  │  Temettü: {:.2f}%",
                        vol * 100.0, rate_src, rate * 100.0, div * 100.0),
            "────────────────────────────────────────────────────────────────────────────",
            std::format("CALL PRİMİ: {}{:.4f} {}{}     PUT PRİMİ: {}{:.4f} {}{}",
                        pal.glow_rgb, c_price, ccy, pal.reset_code, pal.glow_rgb, p_price, ccy, pal.reset_code),
            "────────────────────────────────────────────────────────────────────────────",
            std::format("Delta (Δ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Delta], p_greeks[RiskValues::Delta]),
            std::format("Gamma (Γ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Gamma], p_greeks[RiskValues::Gamma]),
            std::format("Vega:        Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Vega],  p_greeks[RiskValues::Vega]),
            std::format("Theta (θ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Theta], p_greeks[RiskValues::Theta]),
            std::format("Rho (ρ):     Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Rho],   p_greeks[RiskValues::Rho])
        };
        te.print_box("CANLI BLACK-SCHOLES DEĞERLEME & GREEKS", lines);
    } else if (sub_choice == "2") {
        double strike = std::round(spot * 0.95 * 100.0) / 100.0;
        double time_to_exp = 0.25;
        BlackScholes bsc{strike, spot, time_to_exp, PayoffType::Call, rate, div};
        double default_shock_vol = std::min(1.50, vol * 1.12);
        double default_mkt_price = bsc(default_shock_vol);

        std::cout << std::format("Gözlenen Call Piyasa Primi ({} K={:.2f} {}, T=0.25y) [Enter = {:.2f}]: ",
                                 live.symbol, strike, ccy, default_mkt_price);
        std::string p_str;
        std::getline(std::cin, p_str);
        double mkt_price = default_mkt_price;
        if (!p_str.empty()) {
            try {
                double parsed = std::stod(p_str);
                if (parsed > std::max(0.01, spot - strike * std::exp(-rate * time_to_exp))) mkt_price = parsed;
            } catch (...) {}
        }

        double g1 = std::max(0.02, vol * 0.70);
        double g2 = std::min(1.50, vol * 1.40);
        double iv = calculate_implied_volatility(bsc, mkt_price, g1, g2);
        std::vector<std::string> lines = {
            std::format("Dayanak: {} {}  │  Spot={:.2f} {}, Strike(K)={:.2f} {}, T=0.25y", live.symbol, live_tag, spot, ccy, strike, ccy),
            std::format("Faiz ({}): {:.2f}%  │  Tarihsel Vol (HV): {:.2f}%", rate_src, rate * 100.0, vol * 100.0),
            std::format("Piyasa Opsiyon Fiyatı:   {:.4f} {}", mkt_price, ccy),
            std::format("Çözülen Zımni Oynaklık (IV): {}{:.2f}%{} (IV - HV: {:+.2f} puan)",
                        pal.glow_rgb, iv * 100.0, pal.reset_code, (iv - vol) * 100.0),
            std::format("Model Fiyatı Geri Çatımı:    {:.4f} {}", bsc(iv), ccy)
        };
        te.print_box("SECANT ROOT-FINDING IMPLIED VOLATILITY", lines);
    } else if (sub_choice == "3") {
        double strike = std::round(spot * 0.90 * 100.0) / 100.0;
        double barrier = std::round(spot * 1.15 * 100.0) / 100.0;
        double time_to_exp = 0.5;
        OptionInfo opt_call{std::make_unique<CallPayoff>(strike), time_to_exp};
        MCOptionValuation mc{std::move(opt_call), 12, vol, rate, div};

        auto t0 = std::chrono::high_resolution_clock::now();
        double p_seq = mc.calc_price(spot, 40000, 42);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms_seq = std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto t2 = std::chrono::high_resolution_clock::now();
        double p_par = mc.calc_price_par(spot, 40000, 42);
        auto t3 = std::chrono::high_resolution_clock::now();
        double ms_par = std::chrono::duration<double, std::milli>(t3 - t2).count();

        OptionInfo opt_barr{std::make_unique<CallPayoff>(strike), time_to_exp};
        MCOptionValuation mc_barr{std::move(opt_barr), 12, vol, rate, div,
                                  BarrierType::up_and_out, barrier};
        double p_barr = mc_barr.calc_price_par(spot, 40000, 42);

        std::vector<std::string> lines = {
            std::format("Dayanak: {} {}  │  Spot: {:.2f} {}  │  K: {:.2f} {}  │  σ: {:.2f}%",
                        live.symbol, live_tag, spot, ccy, strike, ccy, vol * 100.0),
            std::format("Senaryo: 40,000 │ Adım: 12 │ Faiz ({}): {:.2f}%", rate_src, rate * 100.0),
            "────────────────────────────────────────────────────────────────────────────",
            std::format("Sıralı Fiyatlama (1 Çekirdek):  {:.2f} ms │ Prim: {:.4f} {}", ms_seq, p_seq, ccy),
            std::format("Paralel Fiyatlama (std::async):  {:.2f} ms │ Prim: {:.4f} {}", ms_par, p_par, ccy),
            std::format("Eşzamanlılık Hızlanması:        {}{:.2f}x HIZLI{}", pal.green_rgb, ms_seq / std::max(0.1, ms_par), pal.reset_code),
            "────────────────────────────────────────────────────────────────────────────",
            std::format("Up-and-Out Bariyer (Sınır {:.2f} {}): {:.4f} {} (İndirim: -{:.4f} {})",
                        barrier, ccy, p_barr, ccy, p_par - p_barr, ccy)
        };
        te.print_box("EŞZAMANLI MONTE CARLO & BARİYER DEĞERLEMESİ", lines);
    } else if (sub_choice == "5") {
        double strike = std::round(spot * 100.0) / 100.0;
        double time_to_exp = 0.5;
        BlackScholes bs_call{strike, spot, time_to_exp, PayoffType::Call, rate, div};
        BlackScholes bs_put{strike, spot, time_to_exp, PayoffType::Put, rate, div};
        double bs_c = bs_call(vol);
        double bs_p = bs_put(vol);

        OptionInfo opt_c_h{std::make_unique<CallPayoff>(strike), time_to_exp};
        OptionInfo opt_p_h{std::make_unique<PutPayoff>(strike), time_to_exp};
        MCOptionValuation mc_ch{std::move(opt_c_h), 64, vol, rate, div};
        MCOptionValuation mc_ph{std::move(opt_p_h), 64, vol, rate, div};

        MCOptionValuation::HestonParams hp;
        hp.v0 = vol * vol;
        hp.theta = vol * vol;
        hp.kappa = 2.2;
        hp.xi = std::clamp(vol * 1.4, 0.20, 0.55);
        hp.rho = -0.68; // Leverage effect / negative equity-vol correlation

        double heston_c = mc_ch.calc_heston_mc_price_par(spot, hp, 24000, 101);
        double heston_p = mc_ph.calc_heston_mc_price_par(spot, hp, 24000, 101);

        MCOptionValuation::MertonJumpParams jp;
        jp.jump_intensity = 0.85;
        jp.jump_mean = -0.05;
        jp.jump_vol = 0.14;
        double merton_c = mc_ch.calc_merton_jump_price_par(spot, jp, 24000, 202);
        double merton_p = mc_ph.calc_merton_jump_price_par(spot, jp, 24000, 202);

        MCOptionValuation::DupireSurfaceParams dp;
        dp.base_vol = vol;
        dp.skew_slope = -0.18;
        dp.smile_convexity = 0.08;
        dp.term_slope = 0.02;
        double dupire_c = mc_ch.calc_dupire_local_vol_price_par(spot, dp, 24000, 303);
        double dupire_p = mc_ph.calc_dupire_local_vol_price_par(spot, dp, 24000, 303);

        std::vector<std::string> lines = {
            std::format("Dayanak: {} {} │ Spot={:.2f} {} │ Strike K={:.2f} {} │ T=0.5y │ σ={:.2f}%",
                        live.symbol, live_tag, spot, ccy, strike, ccy, vol * 100.0),
            std::format("Heston Parametreleri: κ={:.1f}, θ=v0={:.4f}, ξ={:.2f}, ρ={:.2f} (Cholesky Euler-MC)",
                        hp.kappa, hp.theta, hp.xi, hp.rho),
            std::format("Merton Jump Param.:   λ={:.2f}/yıl, μ_J={:+.2f}, σ_J={:.2f} (Poisson-LogNormal)",
                        jp.jump_intensity, jp.jump_mean, jp.jump_vol),
            std::format("Dupire Local Vol:     σ0={:.2f}%, skew={:+.2f}, smile={:.2f}, term={:.2f}",
                        dp.base_vol * 100.0, dp.skew_slope, dp.smile_convexity, dp.term_slope),
            "────────────────────────────────────────────────────────────────────────────",
            std::format("Black-Scholes (Referans):  Call = {:.4f} {}  │  Put = {:.4f} {}", bs_c, ccy, bs_p, ccy),
            std::format("Heston Stokastik Volatilite: Call = {}{:.4f}{} ({:+.2f}) │ Put = {}{:.4f}{} ({:+.2f})",
                        pal.glow_rgb, heston_c, pal.reset_code, heston_c - bs_c,
                        pal.glow_rgb, heston_p, pal.reset_code, heston_p - bs_p),
            std::format("Merton Jump-Diffusion Model: Call = {}{:.4f}{} ({:+.2f}) │ Put = {}{:.4f}{} ({:+.2f})",
                        pal.glow_rgb, merton_c, pal.reset_code, merton_c - bs_c,
                        pal.glow_rgb, merton_p, pal.reset_code, merton_p - bs_p),
            std::format("Dupire Yerel Vol Yüzeyi:     Call = {}{:.4f}{} ({:+.2f}) │ Put = {}{:.4f}{} ({:+.2f})",
                        pal.glow_rgb, dupire_c, pal.reset_code, dupire_c - bs_c,
                        pal.glow_rgb, dupire_p, pal.reset_code, dupire_p - bs_p)
        };
        te.print_box("SİLAHİAN CH.7: HESTON, MERTON JUMP & DUPİRE LOCAL VOL", lines);
    } else {
        double strike = std::round(spot * 1.10 * 100.0) / 100.0;
        OptionInfo put_opt{std::make_unique<PutPayoff>(strike), 1.0};
        BinomialLatticePricer pricer{std::move(put_opt), vol, rate, 40};
        double a_price = pricer.calc_price(spot, OptType::American);
        double e_price = pricer.calc_price(spot, OptType::Euro);

        std::vector<std::string> lines = {
            std::format("Dayanak: {} {} │ S={:.2f} {}, K={:.2f} {}, T=1.0y, σ={:.2f}%, r={:.2f}%, N=40",
                        live.symbol, live_tag, spot, ccy, strike, ccy, vol * 100.0, rate * 100.0),
            std::format("Avrupa Tipi Put Değeri:    {:.4f} {}", e_price, ccy),
            std::format("Amerikan Tipi Put Değeri:  {:.4f} {}", a_price, ccy),
            std::format("Erken Egzersiz Primi:      {}{:.4f} {}{}", pal.glow_rgb, a_price - e_price, ccy, pal.reset_code)
        };
        te.print_box("BOOST MULTIARRAY BINOMIAL AĞAÇ DEĞERLEMESİ", lines);
    }
}

void run_portfolio_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [5] EIGEN CANLI PORTFÖY KOVARYANS & CHOLESKY RİSK ANALİZİ ---" << pal.reset_code << "\n";
    std::cout << "4 portföy sembolü girin (virgülle) [Default: SPY,QQQ,GLD,BTC-USD]: ";
    std::string raw_syms;
    std::getline(std::cin, raw_syms);

    std::vector<std::string> syms;
    if (!raw_syms.empty()) {
        std::stringstream ss(raw_syms);
        std::string item;
        while (std::getline(ss, item, ',')) {
            size_t b = item.find_first_not_of(" \t");
            if (b != std::string::npos) {
                size_t e = item.find_last_not_of(" \t");
                syms.push_back(LiveMarketDataClient::normalize_symbol(item.substr(b, e - b + 1)));
            }
        }
    }
    std::vector<std::string> defaults = {"SPY", "QQQ", "GLD", "BTC-USD"};
    for (const auto& def : defaults) {
        if (syms.size() >= 4) break;
        if (std::find(syms.begin(), syms.end(), def) == syms.end()) syms.push_back(def);
    }
    syms.resize(4);

    std::vector<RealTimeAssetData> assets;
    bool all_live = true;
    for (const auto& s : syms) {
        auto d = client.fetch_asset_smart(s, "6mo");
        DataQualityDashboard::record_asset_quality(d);
        if (!d.is_live) all_live = false;
        assets.push_back(std::move(d));
    }

    auto yc = client.fetch_treasury_yields();
    std::string anchor_ccy = assets[0].currency_code.empty() ? "USD" : assets[0].currency_code;
    std::string anchor_disp = safe_ccy_display(assets[0]);
    std::string rate_src;
    double rf_rate = resolve_risk_free_rate(assets[0], yc, rate_src);

    std::array<std::map<int64_t, double>, 4> price_maps;
    Eigen::Vector4d spots;
    for (size_t k = 0; k < 4; ++k) {
        auto fx = build_fx_series(client, assets[k].currency_code, anchor_ccy, "6mo");
        spots(static_cast<Eigen::Index>(k)) = (assets[k].current_price > 0.0)
            ? assets[k].current_price * fx.spot_fx
            : 100.0;
        const auto& closes = assets[k].historical_prices;
        for (size_t i = 0; i < closes.size(); ++i) {
            if (closes[i] > 0.0) {
                int64_t dk = bar_day_key(assets[k], i);
                price_maps[k][dk] = closes[i] * fx.rate_on(dk);
            }
        }
    }

    std::vector<int64_t> common_dates;
    for (const auto& [dk, _] : price_maps[0]) {
        if (price_maps[1].contains(dk) && price_maps[2].contains(dk) && price_maps[3].contains(dk)) {
            common_dates.push_back(dk);
        }
    }

    Eigen::MatrixXd cov_basket(4, 4);
    Eigen::MatrixXd emp_rets;
    Eigen::Matrix3d corr3 = Eigen::Matrix3d::Identity();
    Eigen::Vector3d vols3{0.20, 0.25, 0.16};
    bool used_empirical = false;

    if (common_dates.size() >= 16) {
        size_t m = common_dates.size() - 1;
        Eigen::MatrixXd rets(m, 4);
        for (size_t r = 0; r < m; ++r) {
            for (size_t k = 0; k < 4; ++k) {
                double p0 = price_maps[k][common_dates[r]];
                double p1 = price_maps[k][common_dates[r + 1]];
                rets(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(k)) = (p0 > 0.0 && p1 > 0.0) ? std::log(p1 / p0) : 0.0;
            }
        }
        Eigen::RowVector4d mean_r = rets.colwise().mean();
        Eigen::MatrixXd centered = rets.rowwise() - mean_r;
        cov_basket = (centered.transpose() * centered) / static_cast<double>(m - 1) * 252.0;
        for (int i = 0; i < 4; ++i) cov_basket(i, i) = std::max(cov_basket(i, i), 1e-6);
        emp_rets = std::move(rets);

        for (int i = 0; i < 3; ++i) vols3(i) = std::sqrt(cov_basket(i, i));
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                corr3(i, j) = cov_basket(i, j) / std::max(1e-9, vols3(i) * vols3(j));
            }
        }
        used_empirical = true;
    } else {
        cov_basket << 0.0324, 0.0280, 0.0040, 0.0150,
                      0.0280, 0.0529, 0.0050, 0.0220,
                      0.0040, 0.0050, 0.0256, 0.0060,
                      0.0150, 0.0220, 0.0060, 0.3025;
        for (int i = 0; i < 3; ++i) vols3(i) = std::sqrt(cov_basket(i, i));
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) corr3(i, j) = cov_basket(i, j) / (vols3(i) * vols3(j));
        }
        auto syn_paths = PortfolioLinearAlgebra::simulate_correlated_basket_paths(cov_basket, spots, rf_rate, 0.25, 63, 77);
        emp_rets.resize(63, 4);
        for (long r = 0; r < 63; ++r) {
            for (long k = 0; k < 4; ++k) {
                emp_rets(r, k) = (syn_paths(k, r) > 0.0) ? std::log(syn_paths(k, r + 1) / syn_paths(k, r)) : 0.0;
            }
        }
    }

    Eigen::Vector3d weights{0.40, 0.30, 0.30};
    double port_vol = PortfolioLinearAlgebra::calculate_portfolio_volatility(corr3, vols3, weights);
    auto paths = PortfolioLinearAlgebra::simulate_correlated_basket_paths(cov_basket, spots, rf_rate, 1.0, 12, 100);

    std::string mode_tag = (all_live && used_empirical) ? "[CANLI AMPİRİK KOVARYANS]" : "[KISMİ/VARSAYILAN MATRİS]";
    std::vector<std::string> lines = {
        std::format("Durum: {} │ Ortak İş Günü: {} │ Baz Kur: {} │ Faiz ({}): {:.2f}%",
                    mode_tag, common_dates.size(), anchor_ccy, rate_src, rf_rate * 100.0),
        std::format("3 Varlıklı Portföy Volatilitesi ({}/{}/{} w=[0.4,0.3,0.3]): {}{:.2f}%{}",
                    assets[0].symbol, assets[1].symbol, assets[2].symbol,
                    pal.glow_rgb, port_vol * 100.0, pal.reset_code),
        std::format("Korelasyonlar: ρ({},{})={:.2f} │ ρ({},{})={:.2f} │ ρ({},{})={:.2f}",
                    assets[0].symbol, assets[1].symbol, corr3(0, 1),
                    assets[0].symbol, assets[2].symbol, corr3(0, 2),
                    assets[1].symbol, assets[2].symbol, corr3(1, 2)),
        "────────────────────────────────────────────────────────────────────────────",
        "Cholesky Ayrışımı (Σ = L * L^T) ile 4 Varlıklı Korelasyonlu 1-Yıl Fiyat Simülasyonu:"
    };
    for (int k = 0; k < 4; ++k) {
        double y_end = paths(k, 12);
        double ret_pct = (spots(k) > 0.0) ? ((y_end - spots(k)) / spots(k) * 100.0) : 0.0;
        lines.push_back(std::format("  • {:<10} (σ={:5.1f}%): {:>8.2f}{}  ──►  1-Yıl Senaryo: {:>8.2f}{} ({:+.1f}%)",
                                    assets[k].symbol, std::sqrt(cov_basket(k, k)) * 100.0,
                                    spots(k), anchor_disp, y_end, anchor_disp, ret_pct));
    }
    if (emp_rets.rows() >= 16) {
        auto dpo = PortfolioLinearAlgebra::optimize_dynamic_portfolio(emp_rets, rf_rate);
        static const char* act_names[] = {"Equal-Weight", "Defensive-MinVar", "Momentum-Tilt", "Balanced-Core"};
        lines.push_back("────────────────────────────────────────────────────────────────────────────");
        lines.push_back(std::format("Silahian Ch.4 DPO (PSO & Nedensel RL Q-Learning Rejim: {}):",
                                    act_names[std::clamp(dpo.selected_regime_action, 0, 3)]));
        lines.push_back(std::format("  • PSO Sharpe-Max ({:.2f}):   {}={:.0f}% │ {}={:.0f}% │ {}={:.0f}% │ {}={:.0f}% (R={:+.1f}%, σ={:.1f}%)",
                                    dpo.pso_sharpe,
                                    assets[0].symbol, dpo.pso_weights(0) * 100.0,
                                    assets[1].symbol, dpo.pso_weights(1) * 100.0,
                                    assets[2].symbol, dpo.pso_weights(2) * 100.0,
                                    assets[3].symbol, dpo.pso_weights(3) * 100.0,
                                    dpo.pso_return_ann * 100.0, dpo.pso_vol_ann * 100.0));
        lines.push_back(std::format("  • RL Out-of-Sample ({:.2f}): {}={:.0f}% │ {}={:.0f}% │ {}={:.0f}% │ {}={:.0f}% (R={:+.1f}%, σ={:.1f}%)",
                                    dpo.rl_sharpe,
                                    assets[0].symbol, dpo.rl_weights(0) * 100.0,
                                    assets[1].symbol, dpo.rl_weights(1) * 100.0,
                                    assets[2].symbol, dpo.rl_weights(2) * 100.0,
                                    assets[3].symbol, dpo.rl_weights(3) * 100.0,
                                    dpo.rl_return_ann * 100.0, dpo.rl_vol_ann * 100.0));
    }
    te.print_box("EIGEN CANLI PORTFÖY & CHOLESKY ANALİZİ", lines);
}

void run_macro_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [6] CANLI SABİT GETİRİ, TAHVİL VE FAİZ EĞRİSİ PCA ---" << pal.reset_code << "\n";

    auto live_yc = client.fetch_treasury_yields();
    auto irx_hist = client.fetch_asset("^IRX", "1y");
    auto fvx_hist = client.fetch_asset("^FVX", "1y");
    auto tnx_hist = client.fetch_asset("^TNX", "1y");
    DataQualityDashboard::record_asset_quality(irx_hist);
    DataQualityDashboard::record_asset_quality(fvx_hist);
    DataQualityDashboard::record_asset_quality(tnx_hist);

    auto now_tp = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now_tp);
    std::tm now_tm = *std::localtime(&now_c);
    ChronoDate settle{now_tm.tm_year + 1900, static_cast<unsigned>(now_tm.tm_mon + 1), static_cast<unsigned>(now_tm.tm_mday)};
    settle.weekend_roll();

    // Interpolate live continuous yields across the 12-point curve (0.25Y -> 30Y)
    auto interp_live_rate = [&](double years) {
        const double t0 = 0.25, t1 = 5.0, t2 = 10.0, t3 = 30.0;
        double r0 = live_yc.three_month_yield;
        double r1 = live_yc.five_year_yield;
        double r2 = live_yc.ten_year_yield;
        double r3 = live_yc.thirty_year_yield;
        if (years <= t0) return r0;
        if (years <= t1) return r0 + (r1 - r0) * (years - t0) / (t1 - t0);
        if (years <= t2) return r1 + (r2 - r1) * (years - t1) / (t2 - t1);
        if (years <= t3) return r2 + (r3 - r2) * (years - t2) / (t3 - t2);
        return r3;
    };

    std::vector<ChronoDate> maturities{
        ChronoDate{settle}.add_days(1),
        ChronoDate{settle}.add_months(3),
        ChronoDate{settle}.add_months(6),
        ChronoDate{settle}.add_years(1),
        ChronoDate{settle}.add_years(2),
        ChronoDate{settle}.add_years(3),
        ChronoDate{settle}.add_years(5),
        ChronoDate{settle}.add_years(7),
        ChronoDate{settle}.add_years(10),
        ChronoDate{settle}.add_years(15),
        ChronoDate{settle}.add_years(20),
        ChronoDate{settle}.add_years(30)
    };
    std::vector<double> unit_prices;
    unit_prices.reserve(maturities.size());
    for (const auto& mat : maturities) {
        double yrs = std::max(1.0 / 365.0, static_cast<double>(mat - settle) / 365.0);
        unit_prices.push_back(std::exp(-interp_live_rate(yrs) * yrs));
    }

    LinearInterpYieldCurve yc{settle, maturities, unit_prices};

    ChronoDate dated = ChronoDate{settle}.add_months(-5);
    ChronoDate first_cpn = ChronoDate{dated}.add_months(6);
    ChronoDate penult_cpn = ChronoDate{first_cpn}.add_years(19);
    ChronoDate maturity = ChronoDate{penult_cpn}.add_months(6);

    Bond bond{"20Y_BENCH_CORP", dated, first_cpn, penult_cpn, maturity, 2, 0.062, 1000.0};
    double bond_val = bond.discounted_value(settle, yc);

    // Empirical Yield Curve PCA from 1Y live changes in 3M (^IRX), 5Y (^FVX), 10Y (^TNX)
    auto to_date_map = [](const RealTimeAssetData& d) {
        std::map<int64_t, double> mp;
        const auto& cl = d.historical_prices;
        for (size_t i = 0; i < cl.size(); ++i) {
            if (cl[i] > 0.0) mp[bar_day_key(d, i)] = cl[i] / 100.0;
        }
        return mp;
    };

    auto m_irx = to_date_map(irx_hist);
    auto m_fvx = to_date_map(fvx_hist);
    auto m_tnx = to_date_map(tnx_hist);
    std::vector<int64_t> yc_dates;
    for (const auto& [dk, _] : m_irx) {
        if (m_fvx.contains(dk) && m_tnx.contains(dk)) yc_dates.push_back(dk);
    }

    Eigen::MatrixXd cov_yc(3, 3);
    bool empirical_pca = false;
    if (yc_dates.size() >= 20 && irx_hist.is_live && fvx_hist.is_live && tnx_hist.is_live) {
        size_t m = yc_dates.size() - 1;
        Eigen::MatrixXd dy(m, 3);
        for (size_t r = 0; r < m; ++r) {
            dy(static_cast<Eigen::Index>(r), 0) = m_irx[yc_dates[r + 1]] - m_irx[yc_dates[r]];
            dy(static_cast<Eigen::Index>(r), 1) = m_fvx[yc_dates[r + 1]] - m_fvx[yc_dates[r]];
            dy(static_cast<Eigen::Index>(r), 2) = m_tnx[yc_dates[r + 1]] - m_tnx[yc_dates[r]];
        }
        Eigen::RowVector3d mean_dy = dy.colwise().mean();
        Eigen::MatrixXd centered = dy.rowwise() - mean_dy;
        cov_yc = (centered.transpose() * centered) / static_cast<double>(m - 1) * 252.0;
        empirical_pca = true;
    } else {
        cov_yc << 0.01892, 0.00989, 0.00582,
                  0.00989, 0.01011, 0.00612,
                  0.00582, 0.00612, 0.00566;
    }

    auto pca = PortfolioLinearAlgebra::analyze_yield_curve_pca(cov_yc);

    std::vector<std::string> lines = {
        std::format("Valüasyon Tarihi (Settle): {} │ Eğri Kaynağı: {}",
                    settle.to_string(), live_yc.is_live ? "[CANLI ABD HAZİNE]" : "[VARSAYILAN]"),
        std::format("Getiri Eğrisi: 3A={:.2f}% │ 5Y={:.2f}% │ 10Y={:.2f}% │ 30Y={:.2f}% (10Y-3M Spread: {:+.0f} bps)",
                    live_yc.three_month_yield * 100.0, live_yc.five_year_yield * 100.0,
                    live_yc.ten_year_yield * 100.0, live_yc.thirty_year_yield * 100.0,
                    (live_yc.ten_year_yield - live_yc.three_month_yield) * 10000.0),
        std::format("20 Yıllık 6.20% Kuponlu Tahvil (Nominal $1000) Bugünkü Değer: {}${:.2f}{}",
                    pal.glow_rgb, bond_val, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────────",
        std::format("Faiz Eğrisi PCA Dinamikleri ({}, {} Gün):",
                    empirical_pca ? "CANLI ^IRX/^FVX/^TNX" : "VARSAYILAN KOVARYANS", yc_dates.size()),
        std::format("  • 1. Faktör (Seviye / Paralel Kayma): {}{:.1f}%{}", pal.glow_rgb, pca.level_shift_weight * 100.0, pal.reset_code),
        std::format("  • 2. Faktör (Eğim / Tilt Değişimi):   {:.1f}%", pca.slope_tilt_weight * 100.0),
        std::format("  • 3. Faktör (Eğrilik / Curvature):    {:.1f}%", pca.curvature_weight * 100.0)
    };
    te.print_box("CANLI SABİT GETİRİLİ MENKUL KIYMET & PCA", lines);
}

void run_pairs_trading_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [7] PAIRS TRADING & İSTATİSTİKSEL ARBİTRAJ ---" << pal.reset_code << "\n";
    std::cout << "Varlık A sembolü [Default: SPY]: ";
    std::string a_sym; std::getline(std::cin, a_sym);
    if (a_sym.empty()) a_sym = "SPY";
    std::cout << "Varlık B sembolü [Default: QQQ]: ";
    std::string b_sym; std::getline(std::cin, b_sym);
    if (b_sym.empty()) b_sym = "QQQ";

    a_sym = LiveMarketDataClient::normalize_symbol(a_sym);
    b_sym = LiveMarketDataClient::normalize_symbol(b_sym);

    auto a_data = client.fetch_asset_smart(a_sym, "6mo");
    auto b_data = client.fetch_asset_smart(b_sym, "6mo");
    auto a_fund = FundamentalEngine::fetch_fundamentals(client, a_data);
    auto b_fund = FundamentalEngine::fetch_fundamentals(client, b_data);
    DataQualityDashboard::record_asset_quality(a_data, &a_fund);
    DataQualityDashboard::record_asset_quality(b_data, &b_fund);

    RealTimeAssetData b_norm = b_data;
    std::string fx_note = "Aynı Para Birimi (" + (a_data.currency_code.empty() ? std::string("USD") : a_data.currency_code) + ")";
    if (!a_data.currency_code.empty() && !b_data.currency_code.empty() && a_data.currency_code != b_data.currency_code) {
        auto fx = build_fx_series(client, b_data.currency_code, a_data.currency_code, "6mo");
        b_norm.current_price *= fx.spot_fx;
        b_norm.previous_close *= fx.spot_fx;
        for (size_t i = 0; i < b_norm.historical_prices.size(); ++i) {
            int64_t dk = bar_day_key(b_norm, i);
            double r = fx.rate_on(dk);
            b_norm.historical_prices[i] *= r;
        }
        b_norm.currency_code = a_data.currency_code;
        b_norm.currency_display = safe_ccy_display(a_data);
        fx_note = std::format("{} -> {} Tarihsel Kur Düzeltmeli (Anlık Kur: {:.4f})",
                              b_data.currency_code, a_data.currency_code, fx.spot_fx);
    }

    auto res = PairsTradingEngine::analyze_aligned(a_data, b_norm);
    std::string ccy = safe_ccy_display(a_data);
    std::string status_tag = (a_data.is_live && b_data.is_live) ? "[CANLI]" : "[ÇEVRİMDIŞI/KISMİ]";

    std::vector<std::string> lines = {
        std::format("Parite:                  {}{}{} {}", pal.glow_rgb, res.pair_name, pal.reset_code, status_tag),
        std::format("Kur Normalizasyonu:      {}", fx_note),
        std::format("Eşleşen Ortak İş Günü:   {} gün  │  Korelasyon (ρ): {:.3f}", res.aligned_days, res.correlation),
        std::format("Hedge Oranı (Beta):      {:.4f}", res.hedge_ratio),
        std::format("Mevcut Spread Değeri:    {:.2f} {}", res.current_spread, ccy),
        std::format("Spread Ortalaması (μ):   {:.2f} {} (σ = {:.2f} {})", res.spread_mean, ccy, res.spread_std, ccy),
        std::format("Güncel Spread Z-Score:   {}{:+.2f}{}", pal.glow_rgb, res.current_zscore, pal.reset_code),
        std::format("OU Ortalamaya Dönüş Yarılanma Ömrü (Half-Life): {:.1f} gün", res.half_life_days),
        std::format("IOR Net Edge (20bps Maliyet Düşülmüş Z): {:+.2f}  │  {}", res.net_edge_zscore, res.execution_urgency),
        std::format("Algoritmik Sinyal:       {}{}{}", pal.glow_rgb, res.signal, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────",
        std::format("{} Temel/FAVÖK: {}", a_data.symbol, a_fund.summary_line),
        std::format("{} Temel/FAVÖK: {}", b_data.symbol, b_fund.summary_line)
    };
    te.print_box("STAT ARB PAIRS TRADING & FAVÖK KARŞILAŞTIRMASI", lines);
}

void run_drawdown_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [8] MONTE CARLO DRAWDOWN & VAR RİSK MOTORU ---" << pal.reset_code << "\n";
    std::cout << "Sembol girin (örn: SPY, NVDA, THYAO.IS, BTC-USD) [Default: SPY]: ";
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    std::cout << "Sermaye / Portföy Büyüklüğü [Default: 100000]: ";
    std::string cap_str;
    std::getline(std::cin, cap_str);
    double notional = 100000.0;
    if (!cap_str.empty()) {
        try {
            double v = std::stod(cap_str);
            if (v > 0.0) notional = v;
        } catch (...) {}
    }

    auto data = client.fetch_asset_smart(sym, "1y");
    DataQualityDashboard::record_asset_quality(data);
    const auto& closes = data.historical_prices;
    if (closes.size() < 10) {
        std::cout << pal.red_rgb << "\n  ❌ Drawdown risk analizi için yeterli fiyat barı alınamadı!\n" << pal.reset_code;
        return;
    }

    std::vector<double> pnl;
    std::vector<double> rets;
    pnl.reserve(closes.size() - 1);
    rets.reserve(closes.size() - 1);
    for (size_t i = 1; i < closes.size(); ++i) {
        if (closes[i - 1] > 0.0 && closes[i] > 0.0) {
            double r = (closes[i] - closes[i - 1]) / closes[i - 1];
            rets.push_back(r);
            pnl.push_back(notional * r);
        }
    }

    auto risk = DrawdownRiskEngine::analyze(pnl, 1000, 42);
    auto stress = DrawdownRiskEngine::run_stress_scenarios(rets, notional, 12.0);
    std::string ccy = safe_ccy_display(data);
    std::string tag = data.is_live ? "[CANLI 1-YILLIK SERİ]" : "[ÇEVRİMDIŞI SİMÜLASYON]";

    std::vector<std::string> lines = {
        std::format("Varlık: {} {} │ Sermaye: {:.0f} {} │ Gözlem: {} iş günü",
                    data.symbol, tag, notional, ccy, pnl.size()),
        std::format("Geçmiş Net Kâr/Zarar:       {:.2f} {}", risk.net_profit, ccy),
        std::format("Gerçekleşen Max Drawdown:   {:.2f} {} ({:.2f}%)",
                    risk.original_max_drawdown, ccy, (risk.original_max_drawdown / notional) * 100.0),
        std::format("Net Kâr / Max DD Oranı:     {:.2f}", risk.profit_to_max_dd_ratio),
        std::format("1000 Simülasyon Ortalama DD: {:.2f} {} (σ = {:.2f} {})",
                    risk.mean_simulated_dd, ccy, risk.sd_simulated_dd, ccy),
        "────────────────────────────────────────────────────────────────────────",
        std::format("Parametrik %95 Max DD (VaR):     {}{:.2f} {}{}", pal.glow_rgb, risk.parametric_95_dd, ccy, pal.reset_code),
        std::format("Nonparametrik %95 Max DD (VaR):  {}{:.2f} {}{}", pal.glow_rgb, risk.nonparametric_95_dd, ccy, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────",
        std::format("Silahian Ch.4 Tarihsel %95/%99 VaR (1G): {:.2f} / {:.2f} {} (CVaR95: {:.2f})",
                    stress.hist_var_95, stress.hist_var_99, ccy, stress.cvar_95),
        std::format("Stres Testi (-%15 Çöküş): -{:.2f} {} │ Likidite Dondurma (2x Vol+Slippage): -{:.2f} {}",
                    stress.market_crash_15pct_loss, ccy, stress.vol_2x_liquidity_freeze_loss, ccy)
    };
    te.print_box("CANLI MONTE CARLO DRAWDOWN & SİLAHİAN STRES RİSK ANALİZİ", lines);
}

void run_hft_lowlatency_module(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [9] HFT EMİR DEFTERİ, IOR ROUTER & DÜŞÜK GECİKME MOTORU (SİLAHİAN 2024) ---",
                          "--- [9] HFT ORDER BOOK, IOR ROUTER & LOW-LATENCY ENGINE (SILAHIAN 2024) ---",
                          "--- [9] HFT-ORDERBUCH, IOR ROUTER & LOW-LATENCY-ENGINE (SILAHIAN 2024) ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Dayanak varlık sembolü girin (örn: SPY, AAPL, THYAO.IS, BTC-USD) [Default: SPY]: ",
                          "Enter ticker symbol (e.g. SPY, AAPL, THYAO.IS, BTC-USD) [Default: SPY]: ",
                          "Tickersymbol eingeben (z.B. SPY, AAPL, THYAO.IS, BTC-USD) [Default: SPY]: ");
    std::string sym;
    std::getline(std::cin, sym);
    if (sym.empty()) sym = "SPY";
    sym = LiveMarketDataClient::normalize_symbol(sym);

    auto live = client.fetch_asset_smart(sym, "6mo");
    DataQualityDashboard::record_asset_quality(live);
    const double spot = (live.current_price > 0.0) ? live.current_price : 100.0;
    const double vol = (live.annualized_volatility > 0.01) ? live.annualized_volatility : 0.22;
    const std::string ccy = safe_ccy_display(live);
    const std::string live_tag = live.is_live ? "[CANLI]" : "[SİMÜLASYON]";

    // 1. Lock-Free SPSC Ring Buffer -> Pre-Allocated O(1) Circular-Array LOB -> Game-Theoretic MM & Pre-Trade RMS
    const double tick = std::max(0.01, std::round((spot * 0.0005) * 100.0) / 100.0);
    CircularArrayLOB lob(std::max(0.01, spot * 0.70), spot * 1.30, tick, 4096);
    SpscRingBuffer<MarketTickMessage, 256> feed_hub;

    std::mt19937_64 rng(2024ULL);
    std::uniform_int_distribution<int64_t> qty_dist(150, 950);
    uint64_t oid = 1;
    for (int lvl = 1; lvl <= 6; ++lvl) {
        for (int rep = 0; rep < 3; ++rep) {
            feed_hub.try_push(MarketTickMessage{oid, oid, qty_dist(rng), spot - lvl * tick, OrderSide::Buy, 0, lvl});
            ++oid;
            feed_hub.try_push(MarketTickMessage{oid, oid, qty_dist(rng), spot + lvl * tick, OrderSide::Sell, 0, lvl});
            ++oid;
        }
    }
    MarketTickMessage msg;
    while (feed_hub.try_pop(msg)) {
        lob.add_order(msg.order_id, msg.side, msg.price, msg.qty);
    }

    const auto lob_m = lob.analytics();
    const auto mm_quote = GameTheoreticQuoter::run_market_making_session(lob_m.mid_price, vol, 200, 101);
    const auto nash_now = GameTheoreticQuoter::compute_nash_quotes(lob_m.mid_price, lob_m.micro_price, lob_m.top5_imbalance, 3, vol);

    PreTradeRiskConfig rms_cfg;
    rms_cfg.max_order_qty = 10000;
    rms_cfg.max_order_notional = std::max(250000.0, spot * 5000.0);
    rms_cfg.max_price_collar_pct = 5.0;
    PreTradeRiskEngine rms(rms_cfg);
    auto chk_valid = rms.validate_order(OrderSide::Buy, lob_m.best_bid, 500, lob_m.mid_price);
    auto chk_fat   = rms.validate_order(OrderSide::Buy, spot * 1.18, 500, lob_m.mid_price);
    rms.set_pnl(-30000.0);
    auto chk_kill  = rms.validate_order(OrderSide::Sell, lob_m.best_ask, 100, lob_m.mid_price);

    std::vector<std::string> lob_lines = {
        std::format("Varlık: {} {} │ Spot={:.2f} {} │ Tick={:.2f} │ L1/SPSC Lock-Free Hub + O(1) Circular LOB",
                    live.symbol, live_tag, spot, ccy, tick),
        std::format("Best Bid/Offer:  {:.2f}{} (x{})  │  {:.2f}{} (x{})  │  Spread={:.1f} bps",
                    lob_m.best_bid, ccy, lob_m.best_bid_qty, lob_m.best_ask, ccy, lob_m.best_ask_qty, lob_m.spread_bps),
        std::format("Microprice:      {:.4f}{} (Mid: {:.4f}) │ L1 OBI: {:+.3f} │ Top-5 OBI: {:+.3f}",
                    lob_m.micro_price, ccy, lob_m.mid_price, lob_m.l1_imbalance, lob_m.top5_imbalance),
        "────────────────────────────────────────────────────────────────────────────",
        std::format("Ch.7 Nash Market-Making (Inv=+3): Rez={:.4f}{} │ Bid={:.4f} │ Ask={:.4f} (±{:.1f} bps)",
                    nash_now.reservation_price, ccy, nash_now.optimal_bid, nash_now.optimal_ask, nash_now.optimal_half_spread_bps),
        std::format("200-Adım MM Simülasyonu: Fills={} │ Final Inv={} │ Net PnL={}{:+.2f} {}{}",
                    mm_quote.simulated_fills, mm_quote.final_inventory, pal.glow_rgb, mm_quote.simulated_mm_pnl, ccy, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────────",
        std::format("Ch.3 Pre-Trade RMS: [Normal 500sh: {}] │ [+18% Fat-Finger: {}] │ [-30k DD: {}]",
                    chk_valid.approved ? "APPROVED" : "REJECTED",
                    chk_fat.message,
                    chk_kill.approved ? "APPROVED" : "KILL-SWITCH HALTED")
    };
    te.print_box("SİLAHİAN CH.2/3/7: LOCK-FREE RING BUFFER, O(1) LOB, RMS & NASH MM", lob_lines);

    // 2. Intelligent Order Router (IOR - Chapter 4 Q-Learning Execution Optimization)
    IntelligentOrderRouter ior;
    const double spread_in = std::max(4.0, lob_m.spread_bps);
    const auto ior_sum = ior.simulate_parent_order(spot, vol, spread_in, 10000, 260, 42);
    std::vector<std::string> ior_lines = {
        std::format("Ana Emir: 10,000 Adet {} │ Arrival Fiyatı: {:.4f} {} │ Spread={:.1f} bps, σ={:.1f}%",
                    live.symbol, ior_sum.arrival_price, ccy, spread_in, vol * 100.0),
        std::format("Naive LIT Sweep:     VWAP={:.4f}{} │ Dolum={} sh │ Shortfall={:.2f} bps",
                    ior_sum.naive_vwap, ccy, ior_sum.naive_filled_shares, ior_sum.naive_shortfall_bps),
        std::format("RL IOR Smart Router: VWAP={:.4f}{} │ Dolum={} sh ({:.1f}%) │ Shortfall={}{:.2f} bps{}",
                    ior_sum.ior_vwap, ccy, ior_sum.ior_filled_shares, ior_sum.ior_fill_rate_pct,
                    pal.green_rgb, ior_sum.ior_shortfall_bps, pal.reset_code),
        std::format("Execution Tasarrufu: {}{:+.2f} bps{} │ Ort. Gecikme: {:.1f} µs │ Baskın Venue: {}",
                    pal.glow_rgb, ior_sum.shortfall_savings_bps, pal.reset_code,
                    ior_sum.avg_latency_us, ior_sum.dominant_venue),
        std::format("Venue Dağılımı: LIT_PRIMARY={} slice │ MIDPOINT_DARK={} slice │ ALT_ECN_FAST={} slice",
                    ior_sum.venue_allocations[0], ior_sum.venue_allocations[1], ior_sum.venue_allocations[2])
    };
    te.print_box("SİLAHİAN CH.4: INTELLIGENT ORDER ROUTER (DRL EXECUTION OPTIMIZER)", ior_lines);

    // 3. Chapter 6 + Chapter 3 Empirical On-Machine Low-Latency Benchmark
    const auto bench = LowLatencyBenchmarkSuite::run_on_machine(25000, 224);
    std::vector<std::string> perf_lines = {
        I18n::tr("Gerçek Makine Ölçümleri (std::chrono::steady_clock, bu CPU üzerinde canlı test):",
                 "Actual On-Machine Measurements (std::chrono::steady_clock on this CPU):",
                 "Echte CPU-Messungen (std::chrono::steady_clock auf dieser Hardware):"),
        std::format("  1. Ch.3 Emir Defteri (25k Ops):   std::map={:6.2f} ms  │ O(1) Circular={:6.2f} ms  ──► {}{:.2f}x HIZLI{}",
                    bench.lob_map_ms, bench.lob_circular_ms, pal.glow_rgb, bench.lob_speedup, pal.reset_code),
        std::format("  2. Ch.6 False-Sharing (2x1.2M):   Unpadded={:6.2f} ms  │ alignas(64)=  {:6.2f} ms  ──► {}{:.2f}x HIZLI{}",
                    bench.false_shared_ms, bench.cache_padded_ms, pal.glow_rgb, bench.false_sharing_speedup, pal.reset_code),
        std::format("  3. Ch.6 Data Locality & Unroll:   Col-Maj= {:6.2f} ms  │ Row-Tiled 4x= {:6.2f} ms  ──► {}{:.2f}x HIZLI{}",
                    bench.col_major_ms, bench.row_major_tiled_ms, pal.glow_rgb, bench.locality_speedup, pal.reset_code),
        std::format("  4. Ch.6 System Warmup (Hotpath):  Cold=    {:6.4f} ms  │ Primed=       {:6.4f} ms",
                    bench.cold_pass_ms, bench.warmed_pass_ms),
        std::format("  5. Ch.6 Branch Elimination (2.4M):Branchy= {:6.2f} ms  │ Branchless=   {:6.2f} ms  ──► {:.2f}x",
                    bench.branchy_unsorted_ms, bench.branchless_ms, bench.branchless_speedup)
    };
    te.print_box("SİLAHİAN CH.6: CPU CACHE LINE, FALSE SHARING & LOKALİTE BENCHMARK", perf_lines);
}

void run_data_quality_dashboard(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();

    // Trigger a live fetch to populate fresh metrics if needed
    std::cout << "\n" << pal.dim_rgb
              << I18n::tr("Canlı veri kaynakları taranıyor ve metrikler toplanıyor...",
                          "Scanning live data sources and collecting metrics...",
                          "Live-Datenquellen werden gescannt und Metriken gesammelt...")
              << pal.reset_code << "\n";

    // Fetch a few representative assets to populate quality metrics
    std::vector<std::string> sample_symbols = {"SPY", "AAPL", "THYAO.IS", "GRAM-ALTIN", "BTC-USD"};
    for (const auto& sym : sample_symbols) {
        auto data = client.fetch_asset(sym, "1mo");
        auto fund = FundamentalEngine::fetch_fundamentals(client, data);
        DataQualityDashboard::record_asset_quality(data, &fund);
        NewsSentimentEngine::get_asset_sentiment(data.symbol, data.long_name, client);
    }

    // Fetch Treasury Yields and all monitored FX pairs to populate endpoint & FX telemetry
    client.fetch_treasury_yields();
    client.get_fx_rate("USD", "TRY");
    client.get_fx_rate("EUR", "TRY");
    client.get_fx_rate("EUR", "USD");
    client.get_fx_rate("TRY", "USD");
    client.get_fx_rate("TRY", "EUR");

    // Render the full dashboard
    DataQualityDashboard::render_dashboard();

    // Show inline quick-reference for recent symbols
    std::cout << "\n" << pal.glow_rgb << "╟─── " << I18n::tr("HIZLI DURUM (SON SORGULANANLAR)", "QUICK STATUS (RECENTLY QUERIED)", "SCHNELLSTATUS (ZULETZT ANGEFRAGTE)") << " ───╢\n" << pal.reset_code;
    for (const auto& sym : sample_symbols) {
        std::string status = DataQualityDashboard::get_inline_status(sym);
        std::cout << "  " << std::format("{:<14} : {}\n", sym, status);
    }
}

void run_theme_selector() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [T] 80'LER UNIX CRT & RENK TEMASI SEÇİCİ ---",
                          "--- [T] 80s UNIX CRT & COLOR THEME SELECTOR ---",
                          "--- [T] 80er UNIX CRT- & FARBDESIGN-AUSWAHL ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr("Mevcut Tema: ", "Current Theme: ", "Aktuelles Design: ") << pal.icon << " " << pal.name << "\n\n"
              << I18n::tr("  [1] 🟨 80s Unix Amber CRT (VT220 / IBM 3101 - Kehribar Fosfor Tüp)\n",
                          "  [1] 🟨 80s Unix Amber CRT (VT220 / IBM 3101 - Amber Phosphor Tube)\n",
                          "  [1] 🟨 80s Unix Bernstein CRT (VT220 / IBM 3101 - Bernstein-Phosphor)\n")
              << I18n::tr("  [2] 🟩 80s Green Phosphor (VT100 / Matrix CRT - Yeşil Fosfor Tüp)\n",
                          "  [2] 🟩 80s Green Phosphor (VT100 / Matrix CRT - Green Phosphor Tube)\n",
                          "  [2] 🟩 80s Grün Phosphor (VT100 / Matrix CRT - Grün-Phosphor-Röhre)\n")
              << I18n::tr("  [3] 🟦 Unix VAX Deep Blue & Cyan (Lacivert / Siyan CRT)\n",
                          "  [3] 🟦 Unix VAX Deep Blue & Cyan (Deep Blue / Cyan CRT)\n",
                          "  [3] 🟦 Unix VAX Tiefblau & Cyan (Dunkelblau / Cyan CRT)\n")
              << I18n::tr("  [4] 🟪 Cyberpunk 80s Neon Synthwave (Pembe / Mor CRT)\n",
                          "  [4] 🟪 Cyberpunk 80s Neon Synthwave (Pink / Purple CRT)\n",
                          "  [4] 🟪 Cyberpunk 80er Neon Synthwave (Pink / Lila CRT)\n")
              << std::format("  [5] {} {}\n",
                             I18n::tr("CRT Isınma Titreşimi (Warmup Flicker):", "CRT Warmup Flicker:", "CRT-Aufwärmflackern:"),
                             te.crt_flicker_enabled() ? I18n::tr("AÇIK", "ON", "AN") : I18n::tr("KAPALI", "OFF", "AUS"))
              << I18n::select_prompt();
    std::string s;
    std::getline(std::cin, s);
    if (s == "1") te.set_theme(CrtThemeId::Amber);
    else if (s == "2") te.set_theme(CrtThemeId::Green);
    else if (s == "3") te.set_theme(CrtThemeId::Cyan);
    else if (s == "4") te.set_theme(CrtThemeId::Neon);
    else if (s == "5") te.toggle_crt_flicker();

    std::cout << "\n" << I18n::tr("Tema güncellendi!\n", "Theme updated!\n", "Design aktualisiert!\n");
}

void run_copilot_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [C] AI QUANT TRADING COPILOT ---",
                          "--- [C] AI QUANT TRADING COPILOT ---",
                          "--- [C] KI-QUANT-TRADING-COPILOT ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr(
        "Sorunuzu veya komutunuzu yazın (örn: 'THYAO favök ve hedef fiyat', 'GRAM-ALTIN analiz', 'SPY backtest', 'hazine faizleri')\n"
        "[Ana menüye dönmek için boş bırakıp Enter'a basın]:\n",

        "Type your question or prompt (e.g. 'THYAO ebitda and forecast', 'analyze GC=F', 'SPY backtest', 'treasury yields')\n"
        "[Press Enter with an empty line to return to main menu]:\n",

        "Geben Sie Ihre Frage oder Eingabe ein (z.B. 'SAP.DE EBITDA und Ziel', 'GC=F Analyse', 'Zinsen')\n"
        "[Leerzeile eingeben und Enter drücken für Hauptmenü]:\n"
    );
    std::cout << pal.glow_rgb << "quant@unix:~$ " << pal.reset_code;
    std::string q;
    std::getline(std::cin, q);
    if (!q.empty()) {
        QuantCopilot::handle_query(q, client);
    }
}

void run_alpha_screener_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [S] ASİMETRİK ALFA & RADAR-ALTI FIRSAT TARAYICISI ---",
                          "--- [S] ASYMMETRIC ALPHA & OFF-RADAR OPPORTUNITY SCREENER ---",
                          "--- [S] ASYMMETRISCHER ALPHA & UNTER-DEM-RADAR SCREENER ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr(
        "Tarama Yapılacak Piyasayı Seçin:\n"
        "  [1] 🇹🇷 BIST 100 & Sanayi / Büyüme Mid-Caps (Borsa İstanbul - TL)\n"
        "  [2] 🇩🇪 DAX / XETRA & Alman Teknoloji/Savunma Mid-Caps (Frankfurt - EUR)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500, Yüksek Büyüme & Dip Döngüleri - USD)\n"
        "Seçiminiz [1]: ",

        "Select Market Universe to Scan:\n"
        "  [1] 🇹🇷 BIST 100 & Industrial / Growth Mid-Caps (Borsa Istanbul - TRY)\n"
        "  [2] 🇩🇪 DAX / XETRA & German Tech/Defense Mid-Caps (Frankfurt - EUR)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500, High Growth & Turnaround Cycles - USD)\n"
        "Your Choice [1]: ",

        "Zu scannendes Marktuniversum wählen:\n"
        "  [1] 🇹🇷 BIST 100 & Industrie / Mid-Caps (Borsa Istanbul - TRY)\n"
        "  [2] 🇩🇪 DAX / XETRA & Deutsche Tech/Rüstungs-Mid-Caps (Frankfurt - EUR)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500, Wachstum & Zykliker - USD)\n"
        "Ihre Auswahl [1]: "
    );
    std::string m_str;
    std::getline(std::cin, m_str);
    std::string reg = "BIST";
    std::string reg_name = "🇹🇷 BIST 100 & Mid-Caps";
    if (m_str == "2") { reg = "XETRA"; reg_name = "🇩🇪 Frankfurt XETRA"; }
    else if (m_str == "3") { reg = "US"; reg_name = "🇺🇸 Wall Street"; }

    std::cout << I18n::tr(
        "\nFiltre Tercihi:\n"
        "  [1] Tüm Evreni Tara ve Sırala (Sürü Hissesi / Geç Giriş Uyarıları Dahil)\n"
        "  [2] Yalnızca Üstün Asimetrik Fırsatları Göster (R:R >= 2.5, Sürü Hisselerini Gizle)\n"
        "Seçiminiz [1]: ",

        "\nFilter Preference:\n"
        "  [1] Scan & Rank Full Universe (Including Crowded Herd Warnings)\n"
        "  [2] Show Only Superior Asymmetric Setups (R:R >= 2.5, Exclude Crowded)\n"
        "Your Choice [1]: ",

        "\nFiltereinstellung:\n"
        "  [1] Gesamtes Universum scannen & sortieren (inkl. Herden-Warnungen)\n"
        "  [2] Nur überlegene asymmetrische Setups anzeigen (R:R >= 2.5, Herden verbergen)\n"
        "Ihre Auswahl [1]: "
    );
    std::string f_str;
    std::getline(std::cin, f_str);
    ScreeningFilterOptions opts;
    if (f_str == "2") {
        opts.exclude_crowded = true;
        opts.min_asymmetry_ratio = 2.5;
        opts.max_crowdedness_pct = 65.0;
    }

    std::cout << "\n" << pal.dim_rgb
              << I18n::tr("Canlı borsa kotasyonları taranıyor, hacim akümülasyonu ve volatilite sıkışması hesaplanıyor...",
                          "Scanning live quotes, calculating volume accumulation & volatility squeeze...",
                          "Live-Kurse werden gescannt, Volumenakkumulation & Volatilitätskompression berechnet...")
              << pal.reset_code << "\n";
    auto candidates = QuantOpportunityScanner::scan_market(reg, client, opts);
    QuantOpportunityScanner::render_screener_report(candidates, reg_name);
}

void run_investment_advisor_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [A] ALGORİTMİK YATIRIM & SERMAYE DAĞITIM MOTORU ---",
                          "--- [A] ALGORITHMIC INVESTMENT & CAPITAL ALLOCATION ENGINE ---",
                          "--- [A] ALGORITHMISCHE ANLAGE- & KAPITALALLOKATIONS-ENGINE ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr(
        "Yatırım Yaklaşımı ve Mod Tercihi:\n"
        "  [1] 📊 Klasik Portföy Dağılımı & Varlık Tahsisi (Sermaye Koruma & Lokomotif Büyüme)\n"
        "  [2] 🎯 Asimetrik Alfa & Radar-Altı Fırsat Taraması (Başkalarının Göremediği Erken Sinyaller)\n"
        "Seçiminiz [1]: ",

        "Investment Approach & Mode:\n"
        "  [1] 📊 Classic Portfolio Allocation & Asset Sizing (Capital Preservation & Core Growth)\n"
        "  [2] 🎯 Asymmetric Alpha & Off-Radar Opportunity Screener (Unseen Early Signals)\n"
        "Your Choice [1]: ",

        "Anlageansatz & Modus:\n"
        "  [1] 📊 Klassische Portfolio-Allokation (Kapitalerhalt & Kern-Wachstum)\n"
        "  [2] 🎯 Asymmetrischer Alpha & Unter-dem-Radar Screener (Frühe Signale)\n"
        "Ihre Auswahl [1]: "
    );
    std::string mode_str;
    std::getline(std::cin, mode_str);
    if (mode_str == "2") {
        run_alpha_screener_interactive(client);
        return;
    }

    std::cout << I18n::tr(
        "\nPiyasa ve Borsa Tercihini Seçin:\n"
        "  [1] 🇹🇷 BIST 100 (Borsa İstanbul, Türkiye - TL / ₺)\n"
        "  [2] 🇩🇪 XETRA / DAX (Frankfurt, Almanya - EUR / €)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500 / NASDAQ - USD / $)\n"
        "Seçiminiz [1]: ",

        "\nSelect Target Market & Exchange:\n"
        "  [1] 🇹🇷 BIST 100 (Borsa Istanbul, Turkey - TRY / ₺)\n"
        "  [2] 🇩🇪 XETRA / DAX (Frankfurt, Germany - EUR / €)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500 / NASDAQ - USD / $)\n"
        "Your Choice [1]: ",

        "\nZielmarkt & Börse wählen:\n"
        "  [1] 🇹🇷 BIST 100 (Borsa Istanbul, Türkei - TRY / ₺)\n"
        "  [2] 🇩🇪 XETRA / DAX (Frankfurt, Deutschland - EUR / €)\n"
        "  [3] 🇺🇸 Wall Street (S&P 500 / NASDAQ - USD / $)\n"
        "Ihre Auswahl [1]: "
    );
    std::string m_str;
    std::getline(std::cin, m_str);
    MarketRegion region = MarketRegion::BIST;
    if (m_str == "2") region = MarketRegion::XETRA;
    else if (m_str == "3") region = MarketRegion::US;

    std::string curr_name = (region == MarketRegion::BIST ? "TL" : (region == MarketRegion::XETRA ? "EUR" : "USD"));
    std::string def_cap = (region == MarketRegion::BIST ? "100000" : "10000");
    std::cout << I18n::tr(
        std::format("Yatırım Yapılacak Toplam Sermaye ({}) [Default: {}]: ", curr_name, def_cap),
        std::format("Total Capital to Allocate ({}) [Default: {}]: ", curr_name, def_cap),
        std::format("Gesamtes Anlagekapital ({}) [Default: {}]: ", curr_name, def_cap)
    );
    std::string cap_str;
    std::getline(std::cin, cap_str);
    double capital = (region == MarketRegion::BIST ? 100000.0 : 10000.0);
    if (!cap_str.empty()) {
        try { capital = std::stod(cap_str); } catch (...) {}
    }

    std::cout << I18n::tr(
        "\nRisk ve Yatırım Stratejisi:\n"
        "  [1] Muhafazakar / Sermaye Koruma (Garanti Faiz + Defansif Temettü)\n"
        "  [2] Dengeli Büyüme (Hisse + Faiz Tamponu + İhracatçılar)\n"
        "  [3] Yüksek Getiri / Agresif (Momentum & Büyüme Lokomotifleri)\n"
        "Strateji Seçiminiz [2]: ",

        "\nRisk Profile & Strategy:\n"
        "  [1] Conservative / Capital Preservation (Guaranteed Fixed Income + Defensive Yield)\n"
        "  [2] Balanced Growth (Equities + Cash Buffer + Exporters)\n"
        "  [3] High Yield / Aggressive (Momentum & Growth Champions)\n"
        "Your Choice [2]: ",

        "\nRisikoprofil & Strategie:\n"
        "  [1] Konservativ / Kapitalerhalt (Garantierte Zinsen + Defensiv)\n"
        "  [2] Ausgewogenes Wachstum (Aktien + Liquiditätspuffer + Exporteure)\n"
        "  [3] Hohes Wachstum / Aggressiv (Momentum & Wachstumsführer)\n"
        "Ihre Auswahl [2]: "
    );
    std::string p_str;
    std::getline(std::cin, p_str);
    InvestorRisk risk = InvestorRisk::Dengeli_Büyüme;
    if (p_str == "1") risk = InvestorRisk::DüşükRisk_Koruma;
    else if (p_str == "3") risk = InvestorRisk::YüksekGetiri_Agresif;

    std::cout << "\n" << pal.dim_rgb
              << I18n::tr("Canlı borsa kotasyonları taranıyor ve optimize sermaye tahsis kartları hesaplanıyor...",
                          "Scanning live quotes and generating optimized capital allocation cards...",
                          "Live-Kurse werden abgefragt und optimierte Kapitalallokationskarten berechnet...")
              << pal.reset_code << "\n";
    auto plan = QuantInvestmentAdvisor::generate_regional_plan(capital, region, risk, client);

    QuantInvestmentAdvisor::render_plan_cards(plan);
}

static std::string make_default_pdf_report_path_() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &tt);
#else
    localtime_r(&tt, &tm_now);
#endif
    return std::format("reports/ModernQuant_Global_1M_Report_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.pdf",
                       tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                       tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
}

void run_global_pdf_report_interactive(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [R] KÜRESEL BORSALAR & DÜNYA EKONOMİSİ 1 AYLIK PDF RAPOR MOTORU ---",
                          "--- [R] GLOBAL MARKETS & WORLD MACRO 1-MONTH PDF REPORT ENGINE ---",
                          "--- [R] GLOBALE MÄRKTE & WELTWIRTSCHAFT 1M-PDF-BERICHT ---")
              << pal.reset_code << "\n";
    std::cout << I18n::tr(
        "Kapsam: ABD (SPY/QQQ/IWM + Hisse Temeli), Almanya (XETRA/DAX 40), Türkiye (BIST 100)\n"
        "        + Dünya Ekonomisi Değişkenleri (ABD Faizleri, VIX, DXY, Brent, Altın, Bakır, USD/TRY)\n"
        "        + 6-Aylık Makro Korelasyonlar, Vektör Tahmin Konileri ve Önümüzdeki 1 Ay Öngörüleri\n",
        "Scope : US (Wall Street), Germany (XETRA DAX 40), Turkey (BIST 100)\n"
        "        + Global Drivers (US Yields, VIX, DXY, Brent, Gold, Copper, USD/TRY)\n"
        "        + 6M Date-Aligned Correlations, Vector Forecast Cones & 1-Month Projections\n",
        "Umfang: USA, Deutschland (DAX 40), Türkei (BIST 100) + Makrovariablen + 1M-Prognosen (PDF)\n"
    );
    std::cout << I18n::tr(
        "\nÇıktı PDF dosya yolu [Enter = otomatik oluştur]: ",
        "\nOutput PDF path [Enter = auto timestamped file]: ",
        "\nAusgabe-PDF-Pfad [Enter = automatisch]: "
    );
    std::string out_path;
    std::getline(std::cin, out_path);
    while (!out_path.empty() && (out_path.front() == ' ' || out_path.front() == '"')) out_path.erase(out_path.begin());
    while (!out_path.empty() && (out_path.back() == ' ' || out_path.back() == '"' || out_path.back() == '\r')) out_path.pop_back();
    if (out_path.empty()) {
        out_path = make_default_pdf_report_path_();
    } else if (!out_path.ends_with(".pdf") && !out_path.ends_with(".PDF")) {
        out_path += ".pdf";
    }

    std::cout << "\n" << pal.dim_rgb
              << I18n::tr("Canlı 3 bölge endeksleri, şirket bilançoları (FAVÖK/F/K), dünya makro verileri ve haber akışları toplanıyor...",
                          "Fetching live 3-region indices, corporate fundamentals, global macro drivers & sticky news...",
                          "Lade Live-Indizes, Unternehmensbilanzen, globale Makrovariablen & News...")
              << pal.reset_code << "\n";

    auto rep = GlobalMacroReportEngine::collect_report_data(client);
    bool ok = GlobalMacroReportEngine::generate_pdf_report(rep, out_path);
    if (!ok) {
        std::cout << pal.red_rgb
                  << I18n::tr("[HATA] PDF dosyası yazılamadı: ", "[ERROR] Failed to write PDF: ", "[FEHLER] PDF konnte nicht geschrieben werden: ")
                  << out_path << pal.reset_code << "\n";
        return;
    }

    std::string abs_path = std::filesystem::absolute(out_path).string();
    GlobalMacroReportEngine::render_terminal_summary(rep, abs_path);

    std::cout << I18n::tr(
        "Oluşturulan PDF raporu varsayılan görüntüleyicide açılsın mı? [e/H]: ",
        "Open the generated PDF in your default viewer? [y/N]: ",
        "Erstelltes PDF im Standard-Viewer öffnen? [j/N]: "
    );
    std::string ans;
    std::getline(std::cin, ans);
    if (!ans.empty() && (ans[0] == 'e' || ans[0] == 'E' || ans[0] == 'y' || ans[0] == 'Y' || ans[0] == 'j' || ans[0] == 'J')) {
#ifdef _WIN32
        std::string cmd = std::format("start \"\" \"{}\"", abs_path);
        std::system(cmd.c_str());
#elif __APPLE__
        std::string cmd = std::format("open \"{}\"", abs_path);
        std::system(cmd.c_str());
#else
        std::string cmd = std::format("xdg-open \"{}\" >/dev/null 2>&1 &", abs_path);
        std::system(cmd.c_str());
#endif
    }
}

void run_web_dashboard_launcher(LiveMarketDataClient& client) {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    namespace fs = std::filesystem;

    std::cout << "\n" << pal.glow_rgb
              << I18n::tr("--- [W] MODERNQUANT GÖRSEL ARAŞTIRMA PANOSU (REACT + FASTAPI) ---",
                          "--- [W] MODERNQUANT VISUAL RESEARCH DASHBOARD (REACT + FASTAPI) ---",
                          "--- [W] MODERNQUANT VISUELLES FORSCHUNGS-DASHBOARD (REACT + FASTAPI) ---")
              << pal.reset_code << "\n";

    bool has_reports = false;
    if (fs::exists("reports")) {
        for (const auto& entry : fs::directory_iterator("reports")) {
            if (entry.is_directory() && fs::exists(entry.path() / "summary.csv")) {
                has_reports = true;
                break;
            }
        }
    }

    if (!has_reports) {
        std::cout << pal.dim_rgb << I18n::tr(
            "reports/ klasöründe henüz dışa aktarılmış deney yok. Dashboard için hızlı bir US Yatay Kesit raporu üretilsin mi? [E/h]: ",
            "No exported experiments in reports/ yet. Generate a live US cross-sectional report for the dashboard? [Y/n]: ",
            "Noch keine Berichte in reports/. Jetzt einen US-Querschnittsbericht generieren? [J/n]: ")
                  << pal.reset_code;
        std::string ans;
        std::getline(std::cin, ans);
        if (ans.empty() || ans[0] == 'e' || ans[0] == 'E' || ans[0] == 'y' || ans[0] == 'Y' || ans[0] == 'j' || ans[0] == 'J') {
            auto rep = CrossSectionalBacktestEngine::run_regional_study(client, "US", 5, 4, "2y", true);
            CrossSectionalBacktestEngine::render_report(rep);
        }
    }

    if (!fs::exists("dashboard/start.py")) {
        std::cout << pal.red_rgb << "\n  ❌ dashboard/start.py bulunamadı!\n" << pal.reset_code;
        return;
    }

    std::cout << pal.green_rgb << "\n  🚀 FastAPI (http://localhost:8000) & React Vite UI (http://localhost:5173) başlatılıyor...\n"
              << "     Çıkmak için yeni açılan pencerede Ctrl+C kullanabilirsiniz.\n" << pal.reset_code;
#ifdef _WIN32
    int rc = std::system("start \"ModernQuant Research Dashboard\" cmd /k \"python dashboard\\start.py\"");
#else
    int rc = std::system("python3 dashboard/start.py &");
#endif
    (void)rc;
}

void print_about_info() {
    auto& te = ThemeEngine::instance();
    std::vector<std::string> lines = {
        I18n::tr("Uygulama: Modern C++ Algorithmic & Quantitative Trading Terminal v2.0",
                 "Application: Modern C++ Algorithmic & Quantitative Trading Terminal v2.0",
                 "Anwendung: Modern C++ Algorithmisches & Quant-Trading Terminal v2.0"),
        I18n::tr("Referans Eserler:", "Foundational References:", "Referenzwerke:"),
        "  1. Learning Modern C++ for Finance (Daniel Hanson, O'Reilly 2024)",
        "     • C++20 ChronoDate, Ranges, Task Concurrency (std::async)",
        "     • Eigen Matrix Linear Algebra, Cholesky, Householder QR, PCA",
        "     • Boost MultiArray Binomial Trees & Boost Accumulators",
        "  2. C++ High Performance for Financial Systems (Ariel Silahian, Packt 2024)",
        "     • O(1) Circular-Array LOB, Lock-Free SPSC Hub, Pre-Trade RMS & Nash MM",
        "     • Dynamic Portfolio Opt (PSO / RL), IOR Router, Stress Test & Net Pairs",
        "     • Cache Alignment (64B), Circuit Breaker / P95 SLO, Heston/Merton/Dupire",
        "  3. Python for Algorithmic Trading Cookbook (Jason Strimpel, 2024)",
        "  4. Python for Algorithmic Trading (Yves Hilpisch, O'Reilly 2021)",
        "────────────────────────────────────────────────────────────────────────",
        I18n::tr("Mimari: 100% ISO C++20, RAII, Smart Pointers, Concept Safety, 0 Memory Leak",
                 "Architecture: 100% ISO C++20, RAII, Smart Pointers, Concepts, Zero Leaks",
                 "Architektur: 100% ISO C++20, RAII, Smart Pointers, Concepts, 0 Memory Leaks"),
        I18n::tr("Diller: 🇹🇷 Türkçe • 🇬🇧 English • 🇩🇪 Deutsch (Dinamik / Dynamic)",
                 "Languages: 🇹🇷 Türkçe • 🇬🇧 English • 🇩🇪 Deutsch (Dynamic i18n)",
                 "Sprachen: 🇹🇷 Türkçe • 🇬🇧 English • 🇩🇪 Deutsch (Dynamische i18n)"),
        I18n::tr("Temalar: 80'ler VT220 Amber, VT100 Green, VAX Cyan, Synthwave Neon",
                 "Themes: 80s VT220 Amber, VT100 Green, VAX Cyan, Synthwave Neon",
                 "Designs: 80er VT220 Bernstein, VT100 Grün, VAX Cyan, Synthwave Neon")
    };
    te.print_box(I18n::tr("UYGULAMA HAKKINDA", "ABOUT THIS TERMINAL", "ÜBER DIESES TERMINAL"), lines);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Windows Terminal & CMD: Enable UTF-8 code page and ANSI VT100 colors
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    // Parse CLI arguments: --test, --lang en|de|tr, --language en|de|tr
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            bool pass = AutomatedTestSuite::run_all_tests();
            return pass ? 0 : 1;
        } else if (arg == "--pdf-report") {
            std::string out_path;
            if (i + 1 < argc) {
                std::string nxt = argv[i + 1];
                if (!nxt.starts_with("--")) {
                    out_path = argv[++i];
                }
            }
            if (out_path.empty()) out_path = make_default_pdf_report_path_();
            LiveMarketDataClient cli;
            auto rep = GlobalMacroReportEngine::collect_report_data(cli);
            bool ok = GlobalMacroReportEngine::generate_pdf_report(rep, out_path);
            if (ok) {
                GlobalMacroReportEngine::render_terminal_summary(rep, std::filesystem::absolute(out_path).string());
            } else {
                std::cerr << "[ERROR] Failed to write PDF: " << out_path << "\n";
            }
            return ok ? 0 : 1;
        } else if (arg == "--export-report" && i + 1 < argc) {
            std::string reg = argv[++i];
            std::transform(reg.begin(), reg.end(), reg.begin(), ::toupper);
            LiveMarketDataClient cli;
            if (reg == "SYN") {
                std::vector<RealTimeAssetData> syn_univ;
                for (int k = 0; k < 8; ++k) {
                    syn_univ.push_back(CrossSectionalBacktestEngine::make_synthetic_asset_(
                        std::format("SYN{}.MQ", k + 1), 160, 4200ULL + static_cast<uint64_t>(k) * 97ULL,
                        0.0004, 0.014 + 0.002 * k));
                }
                CrossSectionalConfig cfg;
                cfg.region = "SYN";
                cfg.experiment_name = "bt_cpp_SYN_h5_demo";
                cfg.horizon_days = 5;
                cfg.top_n = 3;
                auto rep = CrossSectionalBacktestEngine::run_backtest(syn_univ, cfg);
                CrossSectionalBacktestEngine::export_report(rep, &syn_univ);
                CrossSectionalBacktestEngine::render_report(rep);
                return rep.valid ? 0 : 1;
            }
            auto rep = CrossSectionalBacktestEngine::run_regional_study(cli, reg, 5, 4, "2y", true);
            CrossSectionalBacktestEngine::render_report(rep);
            return rep.valid ? 0 : 1;
        } else if ((arg == "--lang" || arg == "--language") && i + 1 < argc) {
            std::string l = argv[++i];
            std::transform(l.begin(), l.end(), l.begin(), ::tolower);
            if (l == "en" || l == "english") I18n::set_lang(Language::English);
            else if (l == "de" || l == "german" || l == "deutsch") I18n::set_lang(Language::German);
            else if (l == "tr" || l == "turkish" || l == "turkce") I18n::set_lang(Language::Turkish);
        } else if (arg == "--lang=en" || arg == "--language=en") {
            I18n::set_lang(Language::English);
        } else if (arg == "--lang=de" || arg == "--language=de") {
            I18n::set_lang(Language::German);
        } else if (arg == "--lang=tr" || arg == "--language=tr") {
            I18n::set_lang(Language::Turkish);
        }
    }

    LiveMarketDataClient market_client;
    print_banner(true); // CRT warm-up flicker on startup
    print_status_bar();

    while (true) {
        print_menu();
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) line.pop_back();
        if (line.empty()) continue;

        std::string choice = line;
        std::transform(choice.begin(), choice.end(), choice.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (choice == "0") {
            std::cout << "\n\033[1;32m" << I18n::exit_message() << "\033[0m\n\n";
            break;
        } else if (choice == "L") {
            run_language_selector();
        } else if (choice == "A") {
            run_investment_advisor_interactive(market_client);
        } else if (choice == "S") {
            run_alpha_screener_interactive(market_client);
        } else if (choice == "F") {
            run_forecast_interactive(market_client);
        } else if (choice == "C") {
            run_copilot_interactive(market_client);
        } else if (choice == "G") {
            run_chart_interactive(market_client);
        } else if (choice == "1") {
            run_live_data_interactive(market_client);
        } else if (choice == "2") {
            run_backtest_interactive(market_client);
        } else if (choice == "3") {
            run_live_signals_interactive(market_client);
        } else if (choice == "4") {
            run_derivatives_module(market_client);
        } else if (choice == "5") {
            run_portfolio_module(market_client);
        } else if (choice == "6") {
            run_macro_module(market_client);
        } else if (choice == "7") {
            run_pairs_trading_module(market_client);
        } else if (choice == "8") {
            run_drawdown_module(market_client);
        } else if (choice == "9" || choice == "H") {
            run_hft_lowlatency_module(market_client);
        } else if (choice == "D") {
            run_data_quality_dashboard(market_client);
        } else if (choice == "T") {
            run_theme_selector();
        } else if (choice == "V") {
            AutomatedTestSuite::run_all_tests();
        } else if (choice == "W") {
            run_web_dashboard_launcher(market_client);
        } else if (choice == "R") {
            run_global_pdf_report_interactive(market_client);
        } else if (choice == "11") {
            print_about_info();
        } else {
            std::cout << "\n\033[31m" << I18n::invalid_option() << "\033[0m\n";
        }

        std::cout << "\n" << ThemeEngine::instance().palette().dim_rgb
                  << I18n::return_prompt()
                  << ThemeEngine::instance().palette().reset_code;
        std::getline(std::cin, line);
        print_banner(false);
        print_status_bar();
    }
    return 0;
}
