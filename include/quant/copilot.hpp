#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "black_scholes.hpp"
#include "news_sentiment.hpp"
#include "fundamentals.hpp"
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
        std::string lower_input = NewsSentimentEngine::utf8_to_lower(input);

        std::cout << "\n" << pal.glow_rgb << "[COPILOT AI]: " << pal.reset_code;

        // Greeting check (only when no ticker is being queried)
        std::string found_ticker = extract_ticker_symbol_(input, lower_input);

        if (found_ticker.empty() &&
            (lower_input.find("merhaba") != std::string::npos || lower_input.find("selam") != std::string::npos ||
             lower_input == "hello" || lower_input == "hi" || lower_input.find("hallo") != std::string::npos ||
             lower_input.find("guten tag") != std::string::npos)) {
            std::cout << I18n::tr(
                "Merhaba! Ben Modern C++20 Kurumsal Quant Trading Copilot'ınızım.\n"
                "  Borsa İstanbul (BIST), Frankfurt (XETRA), Wall Street ve Emtialar (Gram/Ons Altın, Gümüş) için:\n"
                "  • Canlı fiyat, 22G VWAP, 14G OHLC ATR ve EWMA volatilite analizi\n"
                "  • Kur-uyumlu TTM FAVÖK (EBITDA), FD/FAVÖK, Net Borç/FAVÖK, F/K, PD/DD ve ROE bilanço kartı\n"
                "  • 252-iş günü kalibrasyonlu 1H / 1A / 3A hedef fiyat ve 2-yıllık Walk-Forward Backtest sunarım.\n"
                "  Örnek: 'THYAO favök ve hedef fiyat', 'ASTOR bilanço', 'GRAM-ALTIN analiz', 'BIST fırsatlar'.\n",

                "Hello! I am your Modern C++20 Institutional Quant Trading Copilot.\n"
                "  For Borsa Istanbul (BIST), Frankfurt (XETRA), Wall Street, and Precious Metals:\n"
                "  • Live spot, 22D VWAP, 14D OHLC ATR, and EWMA volatility diagnostics\n"
                "  • FX-aligned TTM EBITDA, EV/EBITDA, Net Debt/EBITDA, P/E, P/B, and ROE fundamental cards\n"
                "  • 252-trading-day calibrated 1W / 1M / 3M forecasts and 2-year Walk-Forward Backtests.\n"
                "  Examples: 'THYAO ebitda and target', 'AAPL fundamentals', 'GRAM-ALTIN forecast', 'US screener'.\n",

                "Hallo! Ich bin Ihr Modern C++20 Institutional Quant Trading Copilot.\n"
                "  Live-Kurse, 22T-VWAP, 14T-ATR, währungsbereinigtes EBITDA (EV/EBITDA, Nettoschuld/EBITDA)\n"
                "  sowie kalibrierte 1W/1M/3M-Zielkurse und 2-Jahres-Walk-Forward-Backtests.\n"
            );
            return;
        }

        // Backtest / Validation Query (works with or without explicit ticker)
        if (lower_input.find("backtest") != std::string::npos || lower_input.find("test et") != std::string::npos ||
            lower_input.find("doğru mu") != std::string::npos || lower_input.find("isabet") != std::string::npos ||
            lower_input.find("geçmiş") != std::string::npos || lower_input.find("accuracy") != std::string::npos ||
            lower_input.find("verify") != std::string::npos || lower_input.find("trefferquote") != std::string::npos) {

            std::string test_sym = found_ticker.empty() ? "SPY" : found_ticker;
            int horizon = 5;
            if (lower_input.find("1 ay") != std::string::npos || lower_input.find("21 gün") != std::string::npos ||
                lower_input.find("20 gün") != std::string::npos || lower_input.find("aylık") != std::string::npos ||
                lower_input.find("1 month") != std::string::npos || lower_input.find("1 monat") != std::string::npos) {
                horizon = 21;
            } else if (lower_input.find("10 gün") != std::string::npos || lower_input.find("2 hafta") != std::string::npos ||
                       lower_input.find("2 weeks") != std::string::npos || lower_input.find("2 wochen") != std::string::npos) {
                horizon = 10;
            }

            std::cout << I18n::tr(
                std::format("🔬 '{}' üzerinde 2-Yıllık Walk-Forward Doğrulama çalıştırılıyor (Ufuk: {} İş Günü)...\n", test_sym, horizon),
                std::format("🔬 Running 2-Year Walk-Forward Validation on '{}' (Horizon: {} Trading Days)...\n", test_sym, horizon),
                std::format("🔬 2-Jahres-Walk-Forward-Validierung auf '{}' läuft (Horizont: {} Handelstage)...\n", test_sym, horizon)
            );
            auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, test_sym, horizon, "2y");
            ForecastBacktestEngine::render_backtest_report(rep);
            return;
        }

        // Opportunity Screener Query (when no single ticker is requested)
        if (found_ticker.empty() &&
            (lower_input.find("fırsat") != std::string::npos || lower_input.find("ne al") != std::string::npos ||
             lower_input.find("öneri") != std::string::npos || lower_input.find("hangi") != std::string::npos ||
             lower_input.find("hisseler") != std::string::npos || lower_input.find("radar") != std::string::npos ||
             lower_input.find("asimetrik") != std::string::npos || lower_input.find("gizli") != std::string::npos ||
             lower_input.find("sıkışma") != std::string::npos || lower_input.find("screener") != std::string::npos ||
             lower_input.find("opportunity") != std::string::npos || lower_input.find("alpha") != std::string::npos ||
             lower_input.find("what to buy") != std::string::npos || lower_input.find("was kaufen") != std::string::npos)) {

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

            std::cout << pal.dim_rgb
                      << I18n::tr("Canlı 6-aylık OHLCV kanalları ve TTM FAVÖK çarpanları taranıyor...",
                                  "Scanning live 6-month OHLCV channels and TTM EBITDA multiples...",
                                  "Live 6-Monats-OHLCV-Kanäle und TTM-EBITDA-Multiplikatoren werden gescannt...")
                      << pal.reset_code << "\n";

            auto candidates = QuantOpportunityScanner::scan_market(scan_reg, client);
            QuantOpportunityScanner::render_screener_report(candidates, reg_title);
            return;
        }

        // Macro / Yield / Option / Regional Queries when no specific ticker was extracted
        if (found_ticker.empty()) {
            if (lower_input.find("faiz") != std::string::npos || lower_input.find("hazine") != std::string::npos ||
                lower_input.find("rate") != std::string::npos || lower_input.find("yield") != std::string::npos ||
                lower_input.find("zins") != std::string::npos || lower_input.find("anleihe") != std::string::npos) {
                auto curve = client.fetch_treasury_yields();
                std::string shape = (curve.ten_year_yield > curve.three_month_yield)
                    ? I18n::tr("normal pozitif eğimli", "normal upward-sloping", "normal positiv geneigt")
                    : I18n::tr("ters eğimli (inverted)", "inverted", "invers");
                std::string status_tag = curve.is_live ? "[CANLI]" : "[VARSAYILAN]";
                std::cout << std::format("ABD Hazine Getiri Eğrisi {}:\n"
                                         "  • 3-Ay T-Bill (^IRX) : %{:.2f}\n"
                                         "  • 5-Yıl Note  (^FVX) : %{:.2f}\n"
                                         "  • 10-Yıl Bond (^TNX) : %{:.2f}\n"
                                         "  • 30-Yıl Bond (^TYX) : %{:.2f}\n"
                                         "  • Eğri Yapısı        : {} (10Y-3M Farkı: {:+.0f} bps)\n",
                                         status_tag,
                                         curve.three_month_yield * 100.0,
                                         curve.five_year_yield * 100.0,
                                         curve.ten_year_yield * 100.0,
                                         curve.thirty_year_yield * 100.0,
                                         shape,
                                         (curve.ten_year_yield - curve.three_month_yield) * 10000.0);
                return;
            }

            if (lower_input.find("opsiyon") != std::string::npos || lower_input.find("option") != std::string::npos ||
                lower_input.find("black") != std::string::npos) {
                BlackScholes bs(100.0, 100.0, 0.5, PayoffType::Call, 0.045, 0.015);
                double call_p = bs(0.20);
                auto greeks = bs.risk_values(0.20);
                std::cout << std::format("Analitik Black-Scholes Çekirdeği (S=100, K=100, T=0.5y, σ=20%, r=4.5%):\n"
                                         "  • Call Primi: {:.4f}  │  Delta: {:.4f}  │  Gamma: {:.4f}  │  Vega: {:.4f}\n"
                                         "  👉 Canlı varlık opsiyon fiyatlaması için ana menüden [4] seçiniz.\n",
                                         call_p, greeks[RiskValues::Delta], greeks[RiskValues::Gamma], greeks[RiskValues::Vega]);
                return;
            }

            if (lower_input.find("bist") != std::string::npos || lower_input.find("istanbul") != std::string::npos ||
                lower_input.find("turkey") != std::string::npos || lower_input.find("türkei") != std::string::npos ||
                lower_input.find("msci") != std::string::npos || lower_input.find("kriz") != std::string::npos) {
                auto bist = NewsSentimentEngine::build_bist_macro_sentiment(&client);
                std::cout << std::format("🇹🇷 {} Canlı Makro Rejim Raporu:\n"
                                         "  • Sentimel & Rejim : {}  {}\n"
                                         "  • Sentez           : {}\n",
                                         bist.region_name, bist.overall_gauge_bar, bist.overall_sentiment_label, bist.macro_synthesis);
                for (const auto& d : bist.macro_developments) {
                    std::cout << std::format("  • {} ({})\n", d.title, d.source);
                }
                return;
            }

            if (lower_input.find("xetra") != std::string::npos || lower_input.find("dax") != std::string::npos ||
                lower_input.find("frankfurt") != std::string::npos || lower_input.find("almanya") != std::string::npos ||
                lower_input.find("germany") != std::string::npos || lower_input.find("deutschland") != std::string::npos) {
                auto xetra = NewsSentimentEngine::build_xetra_macro_sentiment(client);
                std::cout << std::format("🇩🇪 {} Canlı Makro Rejim Raporu:\n"
                                         "  • Sentimel & Rejim : {}  {}\n"
                                         "  • Sentez           : {}\n",
                                         xetra.region_name, xetra.overall_gauge_bar, xetra.overall_sentiment_label, xetra.macro_synthesis);
                for (const auto& d : xetra.macro_developments) {
                    std::cout << std::format("  • {} ({})\n", d.title, d.source);
                }
                return;
            }

            if (lower_input.find("haber") != std::string::npos || lower_input.find("sentiment") != std::string::npos ||
                lower_input.find("makro") != std::string::npos || lower_input.find("piyasa") != std::string::npos ||
                lower_input.find("durum") != std::string::npos || lower_input.find("özet") != std::string::npos ||
                lower_input.find("market") != std::string::npos || lower_input.find("news") != std::string::npos ||
                lower_input.find("overview") != std::string::npos) {
                std::cout << I18n::tr("🌐 Küresel Piyasa Rejimi & Canlı Endeks Özeti:\n\n",
                                      "🌐 Global Market Regime & Live Benchmark Overview:\n\n",
                                      "🌐 Globale Marktübersicht & Live-Benchmark-Status:\n\n");
                auto bist  = NewsSentimentEngine::build_bist_macro_sentiment(&client);
                auto xetra = NewsSentimentEngine::build_xetra_macro_sentiment(client);
                auto us    = NewsSentimentEngine::build_us_macro_sentiment(client);

                std::cout << std::format("  🇹🇷 BIST 100   : {}  {}\n     • {}\n\n",
                                         bist.overall_gauge_bar, bist.overall_sentiment_label, bist.macro_synthesis);
                std::cout << std::format("  🇩🇪 DAX 40     : {}  {}\n     • {}\n\n",
                                         xetra.overall_gauge_bar, xetra.overall_sentiment_label, xetra.macro_synthesis);
                std::cout << std::format("  🇺🇸 S&P 500    : {}  {}\n     • {}\n",
                                         us.overall_gauge_bar, us.overall_sentiment_label, us.macro_synthesis);
                return;
            }

            std::cout << I18n::tr(
                "Sorunuzda belirli bir hisse veya emtia sembolü tespit edilemedi.\n"
                "  Örnek komutlar:\n"
                "  • Hisse + FAVÖK   : 'THYAO favök ve hedef fiyat', 'ASTOR bilanço', 'AAPL analiz', 'RHM.DE'\n"
                "  • Emtia Analizi   : 'GRAM-ALTIN hedef fiyat', 'ONS ALTIN', 'GRAM-GUMUS'\n"
                "  • Tarama & Test   : 'BIST fırsatlar', 'THYAO backtest', 'piyasa özeti', 'faizler'\n",
                "No specific equity or commodity ticker detected in your query.\n"
                "  Examples: 'THYAO ebitda and target', 'AAPL analysis', 'GRAM-ALTIN', 'US screener', 'SPY backtest'.\n",
                "Kein Tickersymbol erkannt. Beispiele: 'SAP analyse', 'RHM.DE zielkurs', 'GRAM-ALTIN', 'DAX screener'.\n"
            );
            return;
        }

        // Single-Asset Deep Quant + FAVÖK Fundamental + Multi-Horizon Forecast Analysis
        found_ticker = LiveMarketDataClient::normalize_symbol(found_ticker);

        std::cout << I18n::tr(
            std::format("Canlı piyasa ve bilanço motoru '{}' için sorgulanıyor...\n", found_ticker),
            std::format("Querying live market & fundamental engine for '{}'...\n", found_ticker),
            std::format("Live-Markt- & Fundamental-Engine wird für '{}' abgefragt...\n", found_ticker)
        );

        auto data = client.fetch_asset(found_ticker, "6mo");

        // Smart exchange suffix fallback if bare symbol wasn't found on US exchanges
        if ((!data.is_live || data.current_price <= 0.0) &&
            found_ticker.find('.') == std::string::npos &&
            found_ticker.find('-') == std::string::npos &&
            found_ticker.find('=') == std::string::npos) {
            auto bist_try = client.fetch_asset(found_ticker + ".IS", "6mo");
            if (bist_try.is_live && bist_try.current_price > 0.0) {
                data = std::move(bist_try);
                found_ticker = data.symbol;
            } else {
                auto xetra_try = client.fetch_asset(found_ticker + ".DE", "6mo");
                if (xetra_try.is_live && xetra_try.current_price > 0.0) {
                    data = std::move(xetra_try);
                    found_ticker = data.symbol;
                }
            }
        }

        if (data.current_price <= 0.0) {
            std::cout << I18n::tr(
                std::format("\n  ❌ '{}' sembolü için geçerli bir borsa kotasyonu bulunamadı.\n"
                            "     Örnek semboller: THYAO.IS, ASELS.IS, ASTOR.IS, AAPL, NVDA, RHM.DE, GRAM-ALTIN, GC=F\n", found_ticker),
                std::format("\n  ❌ Could not find a valid market quote for '{}'.\n"
                            "     Examples: THYAO.IS, ASELS.IS, AAPL, NVDA, RHM.DE, GRAM-ALTIN, GC=F\n", found_ticker),
                std::format("\n  ❌ Kein gültiger Börsenkurs für '{}' gefunden.\n", found_ticker)
            );
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

        // Render Multi-Horizon Forecast Matrix + Integrated FAVÖK / Fundamental (or Commodity Macro) Card!
        auto multi_forecast = QuantForecastEngine::generate_multi_horizon(
            data, sent.sentiment_score, sent.sentiment_label, sent.has_live_news,
            &fund, has_yc ? &yc : nullptr
        );
        QuantForecastEngine::render_forecast_matrix(multi_forecast);

        // Optional Custom Horizon Card if requested in natural language
        int custom_days = 0;
        std::string custom_vade_label;
        if (lower_input.find("2 hafta") != std::string::npos || lower_input.find("14 gün") != std::string::npos ||
            lower_input.find("2 weeks") != std::string::npos || lower_input.find("2 wochen") != std::string::npos) {
            custom_days = 14;
            custom_vade_label = I18n::tr("2 Hafta (10 İşG)", "2 Weeks (10 TrdD)", "2 Wochen (10 HT)");
        } else if (lower_input.find("2 ay") != std::string::npos || lower_input.find("60 gün") != std::string::npos ||
                   lower_input.find("2 months") != std::string::npos || lower_input.find("2 monate") != std::string::npos) {
            custom_days = 60;
            custom_vade_label = I18n::tr("2 Ay (42 İşG)", "2 Months (42 TrdD)", "2 Monate (42 HT)");
        } else if (lower_input.find("6 ay") != std::string::npos || lower_input.find("altı ay") != std::string::npos ||
                   lower_input.find("180 gün") != std::string::npos || lower_input.find("6 months") != std::string::npos ||
                   lower_input.find("6 monate") != std::string::npos) {
            custom_days = 180;
            custom_vade_label = I18n::tr("6 Ay (126 İşG)", "6 Months (126 TrdD)", "6 Monate (126 HT)");
        } else if (lower_input.find("1 yıl") != std::string::npos || lower_input.find("bir yıl") != std::string::npos ||
                   lower_input.find("365 gün") != std::string::npos || lower_input.find("1 year") != std::string::npos ||
                   lower_input.find("1 jahr") != std::string::npos) {
            custom_days = 365;
            custom_vade_label = I18n::tr("1 Yıl (252 İşG)", "1 Year (252 TrdD)", "1 Jahr (252 HT)");
        }

        if (custom_days > 0) {
            auto custom_res = QuantForecastEngine::forecast_for_days(
                data, sent.sentiment_score, custom_days, custom_vade_label, sent.has_live_news, &fund
            );
            std::string custom_hdr = I18n::tr(
                std::format("║  🎯 TALEP ETTİĞİNİZ ÖZEL VADE TAHMİN KARTI: {}", custom_res.horizon_name),
                std::format("║  🎯 REQUESTED CUSTOM HORIZON FORECAST CARD: {}", custom_res.horizon_name),
                std::format("║  🎯 SPEZIFISCHE ZEITHORIZONT-PROGNOSE: {}", custom_res.horizon_name)
            );
            int cpad = std::max(0, 79 - ThemeEngine::utf8_visible_length(custom_hdr));
            std::cout << "\n" << pal.glow_rgb;
            std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
            std::cout << custom_hdr << std::string(static_cast<size_t>(cpad), ' ') << "║\n";
            std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
            std::cout << pal.reset_code;
            std::cout << std::format("   • {:<24}: {}{}{}\n", I18n::tr("Tahmini Yön Durumu", "Direction Outlook", "Trend-Prognose"), pal.glow_rgb, custom_res.direction_label, pal.reset_code);
            std::cout << std::format("   • {:<24}: {}{:.2f} {} ({:+.2f}%){}\n",
                                     I18n::tr("Beklenen Hedef Fiyat", "Expected Target Price", "Erwarteter Zielkurs"),
                                     pal.glow_rgb, custom_res.target_price, custom_res.currency, custom_res.expected_return_pct, pal.reset_code);
            std::cout << std::format("   • {:<24}: {:.2f} {} ({:+.2f}%)  │  -1σ: {:.2f} {} ({:+.2f}%)\n",
                                     I18n::tr("Boğa Bandı (+1σ)", "Bull Band (+1σ)", "Bull-Band (+1σ)"),
                                     custom_res.bull_target, custom_res.currency, custom_res.bull_return_pct,
                                     custom_res.bear_support, custom_res.currency, custom_res.bear_return_pct);
            std::cout << std::format("   • {:<24}: {}\n", I18n::tr("Model Gerekçesi", "Model Rationale", "Modellbegründung"), custom_res.rationale);
            std::cout << pal.glow_rgb;
            std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
        }
    }

private:
    static std::string extract_ticker_symbol_(const std::string& input, const std::string& lower_input) {
        // 1. Direct Commodity / Company Name Aliases (TR, EN, DE)
        if (lower_input.find("ons gümüş") != std::string::npos || lower_input.find("ons gumus") != std::string::npos ||
            lower_input.find("silver ounce") != std::string::npos || lower_input.find("unze silber") != std::string::npos) return "SI=F";
        if (lower_input.find("gram gümüş") != std::string::npos || lower_input.find("gram gumus") != std::string::npos ||
            lower_input.find("gram-gumus") != std::string::npos || lower_input.find("gümüş") != std::string::npos ||
            lower_input.find("gumus") != std::string::npos || lower_input.find("silver") != std::string::npos ||
            lower_input.find("silber") != std::string::npos) return "GRAM-GUMUS.TL";
        if (lower_input.find("ons altın") != std::string::npos || lower_input.find("ons altin") != std::string::npos ||
            lower_input.find("ons-altin") != std::string::npos || lower_input.find("gold ounce") != std::string::npos ||
            lower_input.find("unze gold") != std::string::npos || lower_input == "ons") return "GC=F";
        if (lower_input.find("gram altın") != std::string::npos || lower_input.find("gram altin") != std::string::npos ||
            lower_input.find("gram-altin") != std::string::npos || lower_input.find("gram-altın") != std::string::npos ||
            lower_input.find("altın") != std::string::npos || lower_input.find("altin") != std::string::npos ||
            lower_input.find("gold") != std::string::npos || lower_input == "gram") return "GRAM-ALTIN.TL";

        if (lower_input.find("john deere") != std::string::npos || lower_input.find("deere") != std::string::npos) return "DE";
        if (lower_input.find("caterpillar") != std::string::npos) return "CAT";
        if (lower_input.find("rheinmetall") != std::string::npos) return "RHM.DE";
        if (lower_input.find("aselsan") != std::string::npos) return "ASELS.IS";
        if (lower_input.find("türk hava yolları") != std::string::npos || lower_input.find("turk hava yollari") != std::string::npos ||
            lower_input == "thy" || lower_input.starts_with("thy ")) return "THYAO.IS";
        if (lower_input.find("tüpraş") != std::string::npos || lower_input.find("tupras") != std::string::npos) return "TUPRS.IS";
        if (lower_input.find("ereğli") != std::string::npos || lower_input.find("eregli") != std::string::npos) return "EREGL.IS";
        if (lower_input.find("şişecam") != std::string::npos || lower_input.find("sisecam") != std::string::npos) return "SISE.IS";
        if (lower_input.find("koç holding") != std::string::npos || lower_input.find("koc holding") != std::string::npos) return "KCHOL.IS";
        if (lower_input.find("sabancı") != std::string::npos || lower_input.find("sabanci") != std::string::npos) return "SAHOL.IS";
        if (lower_input.find("migros") != std::string::npos) return "MGROS.IS";
        if (lower_input.find("pegasus") != std::string::npos) return "PGSUS.IS";
        if (lower_input.find("astor") != std::string::npos) return "ASTOR.IS";
        if (lower_input.find("enka") != std::string::npos) return "ENKAI.IS";
        if (lower_input.find("garanti") != std::string::npos) return "GARAN.IS";
        if (lower_input.find("akbank") != std::string::npos) return "AKBNK.IS";
        if (lower_input.find("palantir") != std::string::npos) return "PLTR";
        if (lower_input.find("nvidia") != std::string::npos) return "NVDA";
        if (lower_input.find("apple") != std::string::npos) return "AAPL";
        if (lower_input.find("microsoft") != std::string::npos) return "MSFT";
        if (lower_input.find("tesla") != std::string::npos) return "TSLA";
        if (lower_input.find("amazon") != std::string::npos) return "AMZN";
        if (lower_input.find("google") != std::string::npos) return "GOOGL";
        if (lower_input.find("bitcoin") != std::string::npos) return "BTC-USD";
        if (lower_input.find("ethereum") != std::string::npos) return "ETH-USD";

        // 2. Tokenize preserving UTF-8 words so Turkish words like "favök" / "bilanço" aren't split into fragments!
        std::vector<std::string> raw_tokens;
        std::string cur;
        for (size_t i = 0; i < input.size(); ++i) {
            unsigned char ch = static_cast<unsigned char>(input[i]);
            if (std::isalnum(ch) || ch >= 0x80 || ch == '.' || ch == '-' || ch == '^' || ch == '=') {
                cur.push_back(static_cast<char>(ch));
            } else {
                if (!cur.empty()) {
                    raw_tokens.push_back(std::move(cur));
                    cur.clear();
                }
            }
        }
        if (!cur.empty()) raw_tokens.push_back(std::move(cur));

        static const std::unordered_set<std::string> stopwords = {
            // Turkish financial & fundamental stopwords (prevents "favök", "bilanço", "fk" from being parsed as tickers!)
            "analiz", "analizi", "et", "fiyat", "fiyati", "fiyatı", "hedef", "hedefi", "tahmin", "tahmini",
            "yon", "yön", "durum", "durumu", "rapor", "raporu", "nasil", "nasıl", "nedir", "kac", "kaç",
            "grafik", "grafigi", "grafiği", "al", "sat", "tut", "hakkinda", "hakkında", "yorum", "goster", "göster",
            "test", "backtest", "canli", "canlı", "piyasa", "piyasalar", "borsa", "istanbul", "bist", "xetra", "dax",
            "wall", "street", "nasdaq", "sp500", "msci", "kriz", "çöküş", "cokus", "çakılma", "cakilma",
            "hisse", "hissesi", "hisseler", "emtia", "tavsiye", "öneri", "oneri", "nerede", "para", "kazan",
            "firsat", "fırsat", "firsatlar", "fırsatlar", "hangi", "ne", "radar", "asimetrik", "gizli", "sıkışma", "sikisma",
            "favök", "favok", "ebitda", "bilanço", "bilanco", "bilançosu", "bilancosu", "temel", "çarpan", "carpan",
            "çarpanlar", "carpanlar", "fk", "pddd", "roe", "roa", "fcf", "fd", "ev", "borç", "borc", "net", "nakit",
            "marj", "marji", "marjı", "kâr", "kar", "zarar", "gelir", "ciro", "temettü", "temettu", "değerleme", "degerleme",
            "haber", "haberler", "haberleri", "sentiment", "sentimel", "makro", "duygu", "faiz", "faizler", "hazine",
            "opsiyon", "black", "scholes", "isabet", "başarı", "basari", "geçmiş", "gecmis", "doğru", "dogru",
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "12", "14", "15", "20", "21", "30", "45", "60", "63", "90", "120", "180", "252", "365",
            "ay", "aylik", "aylık", "hafta", "haftalik", "haftalık", "gun", "gün", "gunluk", "günlük",
            "sene", "yil", "yıl", "yillik", "yıllık", "vade", "vadeli", "bir", "iki", "uc", "üç", "dort", "dört",
            "bes", "beş", "altı", "alti", "on", "ve", "veya", "ile", "icin", "için", "mi", "mı", "mu", "mü",
            "lutfen", "lütfen", "copilot", "quant", "ai", "merhaba", "selam", "bilgi", "ek", "özet", "ozet", "ver", "bak", "incele",
            // English stopwords
            "hello", "hi", "hey", "analyze", "analysis", "forecast", "price", "target", "stock", "stocks",
            "market", "markets", "show", "view", "check", "get", "what", "is", "the", "of", "in", "to", "for",
            "on", "me", "tell", "predict", "direction", "trend", "invest", "investing", "investment", "portfolio",
            "advice", "advise", "recommendation", "recommend", "yield", "yields", "rate", "rates", "bond", "bonds",
            "treasury", "treasuries", "crash", "plunge", "squeeze", "crowded", "hidden", "alpha", "screener",
            "scan", "scanner", "opportunity", "opportunities", "fundamental", "fundamentals", "valuation",
            "multiple", "multiples", "balance", "sheet", "debt", "margin", "earnings", "revenue", "dividend",
            "news", "overview", "summary", "accuracy", "verify", "option", "options", "us", "usa", "turkey", "germany",
            "day", "days", "week", "weeks", "month", "months", "year", "years", "short", "mid", "long", "term",
            "buy", "sell", "holding", "give", "thanks", "thank", "and", "with",
            // German stopwords
            "aktie", "aktien", "kurs", "kurse", "ziel", "zielkurs", "prognose", "vorhersage", "analyse",
            "analysiere", "zeige", "zeigen", "bericht", "markt", "märkte", "nachricht", "nachrichten",
            "zins", "zinsen", "anleihe", "anleihen", "deutschland", "türkei", "frankfurt", "almanya", "abd",
            "geld", "anlegen", "investieren", "strategie", "empfehlung", "chance", "unter", "dem", "wie", "was",
            "wo", "wer", "warum", "bitte", "danke", "guten", "tag", "hallo", "moin", "stimmung", "übersicht",
            "trefferquote", "bewertung", "bilanz", "kennzahlen", "woche", "wochen", "monat", "monate", "jahr",
            "jahre", "tage", "kurzfristig", "mittelfristig", "langfristig", "kaufen", "verkaufen", "und", "auf", "mit", "von", "für"
        };

        for (const auto& tok : raw_tokens) {
            std::string lower_tok = NewsSentimentEngine::utf8_to_lower(tok);
            if (stopwords.contains(lower_tok)) continue;
            if (tok.length() > 14) continue;

            // Skip tokens containing non-ASCII bytes that weren't matched as known company names
            bool has_non_ascii = false;
            for (unsigned char b : tok) {
                if (b >= 0x80) { has_non_ascii = true; break; }
            }
            if (has_non_ascii) continue;

            std::string upper_tok = tok;
            std::transform(upper_tok.begin(), upper_tok.end(), upper_tok.begin(), ::toupper);

            if (upper_tok == "BTC") return "BTC-USD";
            if (upper_tok == "ETH") return "ETH-USD";
            if (upper_tok == "SOL") return "SOL-USD";

            if (upper_tok.find('.') != std::string::npos ||
                upper_tok.find('-') != std::string::npos ||
                upper_tok.find('=') != std::string::npos ||
                upper_tok.starts_with("^")) {
                return LiveMarketDataClient::normalize_symbol(upper_tok);
            }

            if (upper_tok.length() >= 1 && upper_tok.length() <= 6) {
                return LiveMarketDataClient::normalize_symbol(upper_tok);
            }
        }

        return "";
    }
};

} // namespace quant
