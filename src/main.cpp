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
#include "quant/i18n.hpp"

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
         I18n::tr("SMA Kesişimi, Bollinger Mean-Reversion, RSI Momentum", "SMA Crossover, Bollinger Mean-Reversion, RSI Momentum", "SMA-Kreuzung, Bollinger Mean-Reversion, RSI Momentum")},
        {"3",  "[LIVE]",
         I18n::tr("Gerçek Zamanlı İzleyici", "Real-Time Streaming Signals", "Echtzeit-Signal-Streamer"),
         I18n::tr("Boost Accumulators akışkan sinyalleri (BBands & Crossover)", "Boost Accumulators rolling signals (BBands & Crossover)", "Boost Accumulators rollierende Signale")},
        {"4",  "[QUANT]",
         I18n::tr("Türev & Opsiyon Motoru", "Derivatives & Options Engine", "Derivate- & Options-Engine"),
         I18n::tr("Black-Scholes, Greeks, Secant IV, std::async MC, Bariyerler", "Black-Scholes, Greeks, Secant IV, async MC, Barriers", "Black-Scholes, Greeks, Sekanten-IV, async MC, Barrieren")},
        {"5",  "[PORT]",
         I18n::tr("Portföy Risk & Faktör Analizi", "Portfolio Risk & Factor Model", "Portfolio-Risiko & Faktormodell"),
         I18n::tr("Eigen ω^T Σ ω, Cholesky Sepet Simülasyonu, QR Regresyon", "Eigen ω^T Σ ω, Cholesky Basket Simulation, QR Regr.", "Eigen ω^T Σ ω, Cholesky Korb-Simulation, QR-Regr.")},
        {"6",  "[MACRO]",
         I18n::tr("Sabit Getiri & Hazine Eğrisi", "Fixed Income & Yield Curve", "Festverzinsliche & Zinskurve"),
         I18n::tr("C++20 ChronoDate, Bootstrapping, Düzensiz Tahviller, PCA", "C++20 ChronoDate, Bootstrapping, Stub Bonds, PCA", "C++20 ChronoDate, Bootstrapping, Stub-Bonds, PCA")},
        {"7",  "[STAT]",
         I18n::tr("Pairs Trading & Arbitraj", "Statistical Arbitrage / Pairs", "Statistische Arbitrage / Pairs"),
         I18n::tr("Kointegrasyon, Spread Z-Score, Ornstein-Uhlenbeck Half-Life", "Cointegration, Spread Z-Score, OU Mean Reversion", "Kointegration, Spread Z-Score, OU Mean Reversion")},
        {"8",  "[RISK]",
         I18n::tr("Drawdown & VaR Analizi", "Drawdown & VaR Risk Analytics", "Drawdown- & VaR-Risikoanalyse"),
         I18n::tr("Monte Carlo Shuffle, Parametrik & Nonparametrik Max DD", "Monte Carlo Shuffle, Parametric & Historical Max DD", "Monte-Carlo-Shuffle, Parametrischer & Hist. Max DD")},
        {"L",  "[LANG]",
         I18n::tr("Dil / Language / Sprache", "Language / Dil / Sprache", "Sprache / Language / Dil"),
         I18n::tr("Türkçe 🇹🇷 / English 🇬🇧 / Deutsch 🇩🇪", "Switch interface language", "Aktive Oberflächensprache umschalten")},
        {"T",  "[TEMA]",
         I18n::tr("Renk & 80s CRT Teması", "CRT Theme & Color Palette", "CRT-Design & Farbpalette"),
         I18n::tr("Amber VT220, Yeşil VT100, VAX Cyan, Neon, CRT Titreşim", "Amber VT220, Green VT100, VAX Cyan, Neon Glow", "Bernstein VT220, Grün VT100, VAX Cyan, Neon-Glow")},
        {"V",  "[TEST]",
         I18n::tr("Otomatik Doğrulama Testi", "Automated Verification Suite", "Automatische Testsuite"),
         I18n::tr("Tüm kitap formülleri ve ground-truth numerik doğrulaması", "Full textbook numerical & invariant verification", "Vollständige numerische Lehrbuch-Verifikation")},
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

    std::cout << I18n::tr(std::format("Canlı 6-aylık OHLCV, haber akışı ve TTM FAVÖK/bilanço verileri çekiliyor ({})... ", sym),
                          std::format("Fetching live 6-month OHLCV, news & TTM EBITDA/fundamentals ({})... ", sym),
                          std::format("Live 6-Monats-OHLCV, News & TTM-EBITDA werden geladen ({})... ", sym))
              << std::flush;

    auto data = client.fetch_asset(sym, "6mo");
    if ((!data.is_live || data.current_price <= 0.0) &&
        sym.find('.') == std::string::npos && sym.find('-') == std::string::npos && sym.find('=') == std::string::npos) {
        auto bist_try = client.fetch_asset(sym + ".IS", "6mo");
        if (bist_try.is_live && bist_try.current_price > 0.0) {
            data = std::move(bist_try);
            sym = data.symbol;
        } else {
            auto xetra_try = client.fetch_asset(sym + ".DE", "6mo");
            if (xetra_try.is_live && xetra_try.current_price > 0.0) {
                data = std::move(xetra_try);
                sym = data.symbol;
            }
        }
    }

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
    TreasuryYieldCurveData yc;
    bool has_yc = false;
    if (fund.asset_class == AssetClassType::CommodityOrFuture) {
        yc = client.fetch_treasury_yields();
        has_yc = true;
    }

    std::cout << pal.green_rgb
              << I18n::tr("TAMAMLANDI!\n", "COMPLETED!\n", "ABGESCHLOSSEN!\n")
              << pal.reset_code;

    auto multi = QuantForecastEngine::generate_multi_horizon(
        data, sent.sentiment_score, sent.sentiment_label, sent.has_live_news,
        &fund, has_yc ? &yc : nullptr
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
                    data, sent.sentiment_score, d, "", sent.has_live_news, &fund
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
    auto data = client.fetch_asset(sym, "6mo");
    if (data.current_price <= 0.0 || data.historical_prices.empty()) {
        std::cout << pal.red_rgb << "\n  ❌ Geçerli fiyat serisi bulunamadı!\n" << pal.reset_code;
        return;
    }
    auto fund = FundamentalEngine::fetch_fundamentals(client, data);
    auto curve = client.fetch_treasury_yields();
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
    auto data = client.fetch_asset(sym, "6mo");
    if (data.current_price <= 0.0) {
        std::cout << pal.red_rgb << "\n  ❌ Sembol bulunamadı!\n" << pal.reset_code;
        return;
    }
    auto fund = FundamentalEngine::fetch_fundamentals(client, data);
    auto curve = client.fetch_treasury_yields();
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
        "Seçiminiz [1]: ",

        "\nSelect Backtest Model / Strategy Type:\n"
        "  [1] 🎯 Quant Forecast Engine Walk-Forward Validation (2-Year Out-of-Sample Calibration & Net PnL)\n"
        "  [2] Dual SMA Crossover (5 / 20 Days)\n"
        "  [3] Bollinger Bands Mean Reversion (20 Days, 2.0 Std)\n"
        "  [4] RSI Momentum (14 Days, 30/70 Bounds)\n"
        "Your Choice [1]: ",

        "\nBacktest-Modell / Strategietyp wählen:\n"
        "  [1] 🎯 Quant Forecast Engine Walk-Forward-Validierung (2-Jahres Out-of-Sample & Netto-PnL)\n"
        "  [2] Dual-SMA-Kreuzung (5 / 20 Tage)\n"
        "  [3] Bollinger-Bänder Mean-Reversion (20 Tage, 2.0 Std)\n"
        "  [4] RSI Momentum (14 Tage, 30/70 Schwellen)\n"
        "Ihre Auswahl [1]: "
    );
    std::string s_choice;
    std::getline(std::cin, s_choice);

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
        auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, sym, horizon, "2y");
        ForecastBacktestEngine::render_backtest_report(rep);
        auto snap = client.fetch_asset(sym, "6mo");
        if (snap.current_price > 0.0) {
            auto fund = FundamentalEngine::fetch_fundamentals(client, snap);
            auto curve = client.fetch_treasury_yields();
            FundamentalEngine::render_fundamental_card(fund, snap, &curve);
        }
        return;
    }

    auto data = client.fetch_asset(sym, "2y");
    if (data.historical_prices.size() < 25) {
        std::cout << pal.dim_rgb << I18n::tr("Alternatif seri alınıyor...", "Fetching fallback series...", "Alternativserie wird geladen...") << pal.reset_code << "\n";
        data = client.fetch_asset(sym, "1y");
    }

    StrategyType st = StrategyType::SmaCrossover;
    if (s_choice == "3") st = StrategyType::BollingerMeanReversion;
    else if (s_choice == "4") st = StrategyType::RsiMomentum;

    auto res = BacktestEngine::run(data.historical_prices, st);

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

    if (data.current_price > 0.0) {
        auto fund = FundamentalEngine::fetch_fundamentals(client, data);
        auto curve = client.fetch_treasury_yields();
        FundamentalEngine::render_fundamental_card(fund, data, &curve);
    }
}

void run_live_signals_interactive() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [3] BOOST ACCUMULATORS GERÇEK ZAMANLI İZLEYİCİ ---" << pal.reset_code << "\n";
    std::cout << "Boost Accumulators ile streaming veri akışı simüle ediliyor...\n";

    StreamingIndicators ind(5, 1.5, 3, 5);
    std::vector<double> stream_prices = {
        100.00, 103.49, 102.82, 106.86, 104.91, 107.38, 107.46, 111.01,
        112.01, 114.11, 116.91, 121.74, 120.04, 120.24, 120.12, 120.61,
        121.31, 120.47, 121.25, 119.25, 118.11, 120.36, 117.36, 119.12
    };

    std::cout << pal.dim_rgb << "  Tik Fiyat  Ortalama   Üst Bant   Alt Bant   Fast MA  Slow MA   Sinyal\n"
              << "  ─────────────────────────────────────────────────────────────────────────\n" << pal.reset_code;

    for (double p : stream_prices) {
        auto pt = ind.update(p);
        std::string sig_str = (pt.signal == 1) ? (pal.green_rgb + "[AL / GOLDEN]" + pal.reset_code) :
                              (pt.signal == -1) ? (pal.red_rgb + "[SAT / DEATH]" + pal.reset_code) : (pal.dim_rgb + "BEKLE" + pal.reset_code);
        std::cout << std::format("  ${:6.2f}    ${:6.2f}    ${:6.2f}    ${:6.2f}    ${:6.2f}  ${:6.2f}   {}\n",
                                 pt.price, pt.rolling_mean, pt.upper_band, pt.lower_band,
                                 pt.fast_ma, pt.slow_ma, sig_str);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
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
              << "Seçiminiz [1]: ";
    std::string sub_choice;
    std::getline(std::cin, sub_choice);
    if (sub_choice.empty()) sub_choice = "1";

    if (sub_choice == "1") {
        auto live = client.fetch_asset("SPY");
        double spot = live.current_price;
        double strike = spot;
        double time_to_exp = 0.5;
        double vol = live.annualized_volatility;
        double rate = 0.045;
        double div = 0.015;

        BlackScholes call_bs{strike, spot, time_to_exp, PayoffType::Call, rate, div};
        BlackScholes put_bs{strike, spot, time_to_exp, PayoffType::Put, rate, div};

        double c_price = call_bs(vol);
        double p_price = put_bs(vol);
        auto c_greeks  = call_bs.risk_values(vol);
        auto p_greeks  = put_bs.risk_values(vol);

        std::vector<std::string> lines = {
            std::format("Spot: ${:.2f}  │  Kullanım (K): ${:.2f}  │  Vade: {:.2f}y", spot, strike, time_to_exp),
            std::format("Volatilite: {:.2f}%  │  Risksiz Faiz: {:.2f}%  │  Temettü: {:.2f}%", vol * 100.0, rate * 100.0, div * 100.0),
            "────────────────────────────────────────────────────────────────────────",
            std::format("CALL PRİMİ: {}${:.4f}{}     PUT PRİMİ: {}${:.4f}{}", pal.glow_rgb, c_price, pal.reset_code, pal.glow_rgb, p_price, pal.reset_code),
            "────────────────────────────────────────────────────────────────────────",
            std::format("Delta (Δ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Delta], p_greeks[RiskValues::Delta]),
            std::format("Gamma (Γ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Gamma], p_greeks[RiskValues::Gamma]),
            std::format("Vega:        Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Vega],  p_greeks[RiskValues::Vega]),
            std::format("Theta (θ):   Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Theta], p_greeks[RiskValues::Theta]),
            std::format("Rho (ρ):     Call: {:<12.4f} │ Put: {:<12.4f}", c_greeks[RiskValues::Rho],   p_greeks[RiskValues::Rho])
        };
        te.print_box("BLACK-SCHOLES DEĞERLEME & GREEKS", lines);
    } else if (sub_choice == "2") {
        BlackScholes bsc{95.0, 100.0, 0.25, PayoffType::Call, 0.05, 0.07};
        double mkt_price = 6.20;
        double iv = calculate_implied_volatility(bsc, mkt_price, 0.10, 0.25);
        std::vector<std::string> lines = {
            std::format("Piyasa Opsiyon Fiyatı:   ${:.2f}", mkt_price),
            std::format("S=100, K=95, T=0.25y, r=5%, q=7%"),
            std::format("Hesaplanan Zımni Oynaklık (IV): {}{:.2f}%{}", pal.glow_rgb, iv * 100.0, pal.reset_code),
            std::format("Model Fiyatı Geri Çatımı: ${:.4f}", bsc(iv))
        };
        te.print_box("SECANT ROOT-FINDING IMPLIED VOLATILITY", lines);
    } else if (sub_choice == "3") {
        OptionInfo opt_call{std::make_unique<CallPayoff>(75.0), 0.5};
        MCOptionValuation mc{std::move(opt_call), 12, 0.25, 0.05, 0.075};

        auto t0 = std::chrono::high_resolution_clock::now();
        double p_seq = mc.calc_price(100.0, 40000, 42);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms_seq = std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto t2 = std::chrono::high_resolution_clock::now();
        double p_par = mc.calc_price_par(100.0, 40000, 42);
        auto t3 = std::chrono::high_resolution_clock::now();
        double ms_par = std::chrono::duration<double, std::milli>(t3 - t2).count();

        // Barrier option
        OptionInfo opt_barr{std::make_unique<CallPayoff>(75.0), 0.5};
        MCOptionValuation mc_barr{std::move(opt_barr), 12, 0.25, 0.05, 0.075,
                                  BarrierType::up_and_out, 110.0};
        double p_barr = mc_barr.calc_price_par(100.0, 40000, 42);

        std::vector<std::string> lines = {
            std::format("Senaryo Sayısı: 40,000 │ Zaman Adımı: 12"),
            std::format("Sıralı Fiyatlama (1 Çekirdek):  {:.2f} ms │ Fiyat: ${:.4f}", ms_seq, p_seq),
            std::format("Paralel Fiyatlama (std::async):  {:.2f} ms │ Fiyat: ${:.4f}", ms_par, p_par),
            std::format("Eşzamanlılık Hızlanması:        {}{:.2f}x HIZLI{}", pal.green_rgb, ms_seq / std::max(0.1, ms_par), pal.reset_code),
            "────────────────────────────────────────────────────────────────────────",
            std::format("Up-and-Out Bariyer (Sınır $110): ${:.4f} (İndirim: -${:.4f})", p_barr, p_par - p_barr)
        };
        te.print_box("EŞZAMANLI MONTE CARLO & BARİYER DEĞERLEMESİ", lines);
    } else {
        OptionInfo put_opt{std::make_unique<PutPayoff>(40.0), 1.0};
        BinomialLatticePricer pricer{std::move(put_opt), 0.20, 0.06, 4};
        double a_price = pricer.calc_price(36.0, OptType::American);
        double e_price = pricer.calc_price(36.0, OptType::Euro);

        std::vector<std::string> lines = {
            std::format("Parametreler: S=36, K=40, T=1.0y, r=6%, vol=20%, Adım=4"),
            std::format("Avrupa Tipi Put Değeri:    ${:.4f}", e_price),
            std::format("Amerikan Tipi Put Değeri:  ${:.4f}", a_price),
            std::format("Erken Egzersiz Primi:      {}{:.4f}{}", pal.glow_rgb, a_price - e_price, pal.reset_code)
        };
        te.print_box("BOOST MULTIARRAY BINOMIAL AĞAÇ DEĞERLEMESİ", lines);
    }
}

void run_portfolio_module() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [5] EIGEN PORTFÖY & RİSK ANALİZİ ---" << pal.reset_code << "\n";

    Eigen::Matrix3d corr;
    corr << 1.0,  0.5,  0.25,
            0.5,  1.0, -0.7,
            0.25, -0.7, 1.0;
    Eigen::Vector3d vols{0.20, 0.10, 0.40};
    Eigen::Vector3d weights{0.60, -0.30, 0.70};

    double port_vol = PortfolioLinearAlgebra::calculate_portfolio_volatility(corr, vols, weights);

    Eigen::MatrixXd cov_basket(4, 4);
    cov_basket << 0.01263,  0.00025, -0.00017,  0.00503,
                  0.00025,  0.00138,  0.00280,  0.00027,
                 -0.00017,  0.00280,  0.03775,  0.00480,
                  0.00503,  0.00027,  0.00480,  0.02900;
    Eigen::Vector4d spots{100.0, 150.0, 25.0, 50.0};
    auto paths = PortfolioLinearAlgebra::simulate_correlated_basket_paths(cov_basket, spots, 0.01, 1.0, 6, 100);

    std::vector<std::string> lines = {
        std::format("3 Varlıklı Portföy Volatilitesi: {}{:.2f}%{} (w=[0.6, -0.3, 0.7])", pal.glow_rgb, port_vol * 100.0, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────",
        "Cholesky Ayrışımı (Σ = L * L^T) ile 4 Varlıklı Korelasyonlu Fiyat Simülasyonu:",
        std::format("  • Varlık 1: ${:6.2f}  ──►  Yıl Sonu: ${:6.2f}", spots(0), paths(0, 6)),
        std::format("  • Varlık 2: ${:6.2f}  ──►  Yıl Sonu: ${:6.2f}", spots(1), paths(1, 6)),
        std::format("  • Varlık 3: ${:6.2f}  ──►  Yıl Sonu: ${:6.2f}", spots(2), paths(2, 6)),
        std::format("  • Varlık 4: ${:6.2f}  ──►  Yıl Sonu: ${:6.2f}", spots(3), paths(3, 6))
    };
    te.print_box("EIGEN PORTFOY & SEPET ANALIZI", lines);
}

void run_macro_module() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [6] SABİT GETİRİ, TAHVİL VE FAİZ EĞRİSİ PCA ---" << pal.reset_code << "\n";

    ChronoDate settle{2023, 10, 10};
    std::vector<ChronoDate> maturities{
        {2023, 10, 11}, {2024, 1, 10}, {2024, 4, 10}, {2024, 10, 10},
        {2025, 10, 10}, {2026, 10, 12}, {2028, 10, 10}, {2030, 10, 10},
        {2033, 10, 10}, {2038, 10, 11}, {2043, 10, 12}, {2053, 10, 10}
    };
    std::vector<double> unit_prices{
        0.999945, 0.994489, 0.988210, 0.973601,
        0.939372, 0.901885, 0.827719, 0.759504,
        0.670094, 0.547598, 0.448541, 0.300886
    };

    LinearInterpYieldCurve yc{settle, maturities, unit_prices};

    Bond bond{"20Y_CORP_BOND",
              ChronoDate{2023, 5, 8},
              ChronoDate{2023, 11, 7},
              ChronoDate{2042, 11, 7},
              ChronoDate{2043, 5, 7},
              2, 0.062, 1000.0};

    double bond_val = bond.discounted_value(settle, yc);

    // Yield Curve PCA Decomposition
    Eigen::MatrixXd cov_yc(3, 3);
    cov_yc << 0.01892, 0.00989, 0.00582,
              0.00989, 0.01011, 0.00612,
              0.00582, 0.00612, 0.00566;
    auto pca = PortfolioLinearAlgebra::analyze_yield_curve_pca(cov_yc);

    std::vector<std::string> lines = {
        std::format("Valüasyon Tarihi (Settle Date):   {}", settle.to_string()),
        std::format("20 Yıllık Tahvil İskontolu Fiyatı: {}${:.2f}{}", pal.glow_rgb, bond_val, pal.reset_code),
        std::format("Kupon Yapısı: 6.20% Yıllık (Yarıyıl), Düzensiz Kısa İlk Dönem"),
        "────────────────────────────────────────────────────────────────────────",
        "Faiz Eğrisi PCA Dinamikleri (Temel Bileşen Analizi):",
        std::format("  • 1. Faktör (Seviye / Paralel Kayma): {}{:.1f}%{}", pal.glow_rgb, pca.level_shift_weight * 100.0, pal.reset_code),
        std::format("  • 2. Faktör (Eğim / Tilt Değişimi):   {:.1f}%", pca.slope_tilt_weight * 100.0),
        std::format("  • 3. Faktör (Eğrilik / Curvature):    {:.1f}%", pca.curvature_weight * 100.0)
    };
    te.print_box("SABİT GETİRİLİ MENKUL KIYMET ANALİZİ", lines);
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

    auto a_data = client.fetch_asset(a_sym, "6mo");
    auto b_data = client.fetch_asset(b_sym, "6mo");
    auto a_fund = FundamentalEngine::fetch_fundamentals(client, a_data);
    auto b_fund = FundamentalEngine::fetch_fundamentals(client, b_data);

    auto res = PairsTradingEngine::analyze_aligned(a_data, b_data);
    std::string ccy = a_data.currency_display.empty()
        ? LiveMarketDataClient::currency_display_for(LiveMarketDataClient::infer_currency_code(a_sym), a_sym)
        : a_data.currency_display;

    std::vector<std::string> lines = {
        std::format("Parite:                  {}{}{}", pal.glow_rgb, res.pair_name, pal.reset_code),
        std::format("Eşleşen Ortak İş Günü:   {} gün  │  Korelasyon (ρ): {:.3f}", res.aligned_days, res.correlation),
        std::format("Hedge Oranı (Beta):      {:.4f}", res.hedge_ratio),
        std::format("Mevcut Spread Değeri:    {:.2f} {}", res.current_spread, ccy),
        std::format("Spread Ortalaması (μ):   {:.2f} {} (σ = {:.2f} {})", res.spread_mean, ccy, res.spread_std, ccy),
        std::format("Güncel Spread Z-Score:   {}{:+.2f}{}", pal.glow_rgb, res.current_zscore, pal.reset_code),
        std::format("OU Ortalamaya Dönüş Yarılanma Ömrü (Half-Life): {:.1f} gün", res.half_life_days),
        std::format("Algoritmik Sinyal:       {}{}{}", pal.glow_rgb, res.signal, pal.reset_code),
        "────────────────────────────────────────────────────────────────────────",
        std::format("{} Temel/FAVÖK: {}", a_data.symbol, a_fund.summary_line),
        std::format("{} Temel/FAVÖK: {}", b_data.symbol, b_fund.summary_line)
    };
    te.print_box("STAT ARB PAIRS TRADING & FAVÖK KARŞILAŞTIRMASI", lines);
}

void run_drawdown_module() {
    auto& te = ThemeEngine::instance();
    const auto& pal = te.palette();
    std::cout << "\n" << pal.glow_rgb << "--- [8] MONTE CARLO DRAWDOWN & VAR RİSK MOTORU ---" << pal.reset_code << "\n";

    std::vector<double> pnl = {
        -149299.30, -673165.13, 3891123.79, 1061346.21, -578464.00,
        -260855.99, 1102167.76, 509764.96, -276786.46, -11947.13,
        -277781.70, -318603.05, 57747.06, 151336.99, 267826.14,
        -198132.05, -175232.84, -5973.61, 177224.51, -711878.76,
        -276786.56, 116489.15, -185188.30, 1183810.47, 1563147.38
    };

    auto risk = DrawdownRiskEngine::analyze(pnl, 1000, 42);

    std::vector<std::string> lines = {
        std::format("Geçmiş Net Kâr/Zarar:       ${:.2f}", risk.net_profit),
        std::format("Gerçekleşen Max Drawdown:   ${:.2f}", risk.original_max_drawdown),
        std::format("Net Kâr / Max DD Oranı:     {:.2f}", risk.profit_to_max_dd_ratio),
        std::format("1000 Simülasyon Ortalama DD: ${:.2f} (σ = ${:.2f})", risk.mean_simulated_dd, risk.sd_simulated_dd),
        "────────────────────────────────────────────────────────────────────────",
        std::format("Parametrik %95 Max DD (VaR):     {}{:.2f}{}", pal.glow_rgb, risk.parametric_95_dd, pal.reset_code),
        std::format("Nonparametrik %95 Max DD (VaR):  {}{:.2f}{}", pal.glow_rgb, risk.nonparametric_95_dd, pal.reset_code)
    };
    te.print_box("MONTE CARLO DRAWDOWN RISK ANALIZI", lines);
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
        "  2. Python for Algorithmic Trading Cookbook (Jason Strimpel, 2024)",
        "  3. Python for Algorithmic Trading (Yves Hilpisch, O'Reilly 2021)",
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
            run_live_signals_interactive();
        } else if (choice == "4") {
            run_derivatives_module(market_client);
        } else if (choice == "5") {
            run_portfolio_module();
        } else if (choice == "6") {
            run_macro_module();
        } else if (choice == "7") {
            run_pairs_trading_module(market_client);
        } else if (choice == "8") {
            run_drawdown_module();
        } else if (choice == "T") {
            run_theme_selector();
        } else if (choice == "V") {
            AutomatedTestSuite::run_all_tests();
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
