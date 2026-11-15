#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "black_scholes.hpp"
#include "news_sentiment.hpp"
#include "fundamentals.hpp"
#include "forecast_engine.hpp"
#include "forecast_backtest.hpp"
#include "alpha_screener.hpp"
#include "data_quality.hpp"
#include "hft_lob_engine.hpp"
#include "monte_carlo.hpp"
#include "trading_signals.hpp"
#include "i18n.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <cmath>
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
                "  • Canlı fiyat, 22G VWAP, 14G OHLC ATR, EWMA/Asimetrik Heston kuyruk riski (%95 VaR/CVaR)\n"
                "  • Kur-uyumlu TTM FAVÖK (EBITDA), FD/FAVÖK, Net Borç/FAVÖK, F/K ve ROE bilanço kartı\n"
                "  • Silahian (2024) Ch.2-7: SPSC HFT Emir Defteri (LOB), IOR Smart Router, Heston/Merton/Dupire & Stres Testi.\n"
                "  Örnek: 'THYAO favök ve hedef', 'SPY heston', 'THYAO hft lob', 'NVDA stres testi', 'BIST fırsatlar'.\n",

                "Hello! I am your Modern C++20 Institutional Quant Trading Copilot.\n"
                "  For Borsa Istanbul (BIST), Frankfurt (XETRA), Wall Street, and Precious Metals:\n"
                "  • Live spot, 22D VWAP, 14D ATR, asymmetric Heston tail bands & 95% VaR/CVaR\n"
                "  • FX-aligned TTM EBITDA, EV/EBITDA, Net Debt/EBITDA, P/E, P/B, and ROE fundamental cards\n"
                "  • Silahian (2024) Ch.2-7: SPSC HFT Order Book (LOB), IOR Smart Router, Heston/Merton/Dupire & Stress Tests.\n"
                "  Examples: 'THYAO ebitda and target', 'SPY heston', 'AAPL hft ior', 'NVDA stress test', 'US screener'.\n",

                "Hallo! Ich bin Ihr Modern C++20 Institutional Quant Trading Copilot.\n"
                "  Live-Kurse, 22T-VWAP, 14T-ATR, asymmetrisches Tail-Risiko, EBITDA-Kennzahlen sowie\n"
                "  Silahian (2024) HFT-Orderbuch (LOB), IOR Router, Heston/Merton/Dupire & Stresstests.\n"
            );
            return;
        }

        // Silahian Ch.2/3/6/7: HFT Order Book (LOB), SPSC Ring Buffer, Pre-Trade RMS & IOR Execution Intent
        if (lower_input.find("hft") != std::string::npos || lower_input.find("lob") != std::string::npos ||
            lower_input.find("emir defteri") != std::string::npos || lower_input.find("order book") != std::string::npos ||
            lower_input.find("orderbook") != std::string::npos || lower_input.find("ior") != std::string::npos ||
            lower_input.find("smart router") != std::string::npos || lower_input.find("ring buffer") != std::string::npos ||
            lower_input.find("rms") != std::string::npos || lower_input.find("nash") != std::string::npos) {

            std::string sym = found_ticker.empty() ? "SPY" : LiveMarketDataClient::normalize_symbol(found_ticker);
            auto live = client.fetch_asset_smart(sym, "6mo");
            DataQualityDashboard::record_asset_quality(live);
            const double spot = (live.current_price > 0.0) ? live.current_price : 100.0;
            const double vol = (live.annualized_volatility > 0.01) ? live.annualized_volatility : 0.22;

            const double tick = std::max(0.01, std::round((spot * 0.0005) * 100.0) / 100.0);
            CircularArrayLOB lob(std::max(0.01, spot * 0.8), spot * 1.2, tick, 256);
            const double spr = std::max(tick, spot * 0.0008);
            lob.add_order(101ULL, OrderSide::Buy,  spot - spr, 600);
            lob.add_order(102ULL, OrderSide::Sell, spot + spr, 550);
            auto m = lob.analytics();
            auto q = GameTheoreticQuoter::compute_nash_quotes(
                (m.mid_price > 0.0 ? m.mid_price : spot),
                (m.micro_price > 0.0 ? m.micro_price : spot),
                m.l1_imbalance, 0, vol);
            IntelligentOrderRouter ior;
            auto exec = ior.simulate_parent_order(spot, vol, 14.0, 2500, 120, 42);

            std::cout << std::format("{} [Silahian Ch.2/3/7 HFT LOB + Nash MM + IOR Router] ({} @ {:.2f}):\n"
                                     "  • LOB Best Bid/Ask : {:.2f} / {:.2f} (O(1) Circular Array, OBI: {:+.2f})\n"
                                     "  • Nash Market Maker: Bid {:.2f} │ Ask {:.2f} (Reserve: {:.2f})\n"
                                     "  • IOR Smart Exec   : 2500 lot VWAP={:.2f} │ Slippage={:.2f} bps (Naive: {:.2f} bps)\n",
                                     live.is_live ? "🟢 CANLI" : "🟡 SİMÜLASYON", live.symbol, spot,
                                     m.best_bid, m.best_ask, m.l1_imbalance,
                                     q.optimal_bid, q.optimal_ask, q.reservation_price,
                                     exec.ior_vwap, exec.ior_shortfall_bps, exec.naive_shortfall_bps);
            return;
        }

        // Silahian Ch.7: Heston Stochastic Vol / Merton Jump-Diffusion / Dupire Local Vol Intent
        if (lower_input.find("heston") != std::string::npos || lower_input.find("merton") != std::string::npos ||
            lower_input.find("dupire") != std::string::npos || lower_input.find("stokastik") != std::string::npos ||
            lower_input.find("stochastic") != std::string::npos || lower_input.find("local vol") != std::string::npos ||
            lower_input.find("yerel vol") != std::string::npos) {

            std::string sym = found_ticker.empty() ? "SPY" : LiveMarketDataClient::normalize_symbol(found_ticker);
            auto live = client.fetch_asset_smart(sym, "6mo");
            auto yc = client.fetch_treasury_yields();
            DataQualityDashboard::record_asset_quality(live);
            const double spot = (live.current_price > 0.0) ? live.current_price : 500.0;
            const double strike = std::round(spot);
            const double vol = (live.annualized_volatility > 0.01) ? live.annualized_volatility : 0.20;
            const double r = (yc.is_live && yc.three_month_yield > 0.0) ? yc.three_month_yield : 0.045;

            BlackScholes bs{strike, spot, 0.5, PayoffType::Call, r, 0.0};
            OptionInfo opt{std::make_unique<CallPayoff>(strike), 0.5};
            MCOptionValuation mc{std::move(opt), 48, vol, r, 0.0};
            MCOptionValuation::HestonParams hp{vol * vol, 2.2, vol * vol, std::clamp(vol * 1.4, 0.20, 0.55), -0.68};
            MCOptionValuation::MertonJumpParams jp{0.85, -0.05, 0.14};
            MCOptionValuation::DupireSurfaceParams dp{vol, -0.18, 0.08, 0.02};

            std::cout << std::format("{} [Silahian Ch.7 Türev Model Karşılaştırması] ({} ATM Call S=K={:.2f}, T=0.5y, σ={:.1f}%):\n"
                                     "  • Black-Scholes (Referans) : {:.4f}\n"
                                     "  • Heston Stokastik Vol     : {:.4f} (κ=2.2, ρ=-0.68)\n"
                                     "  • Merton Jump-Diffusion    : {:.4f} (λ=0.85, μ_J=-5%)\n"
                                     "  • Dupire Local Vol Yüzeyi  : {:.4f} (skew=-0.18, smile=0.08)\n",
                                     live.is_live ? "🟢 CANLI" : "🟡 SİMÜLASYON", live.symbol, spot, vol * 100.0,
                                     bs(vol),
                                     mc.calc_heston_mc_price_par(spot, hp, 16000, 101),
                                     mc.calc_merton_jump_price_par(spot, jp, 16000, 202),
                                     mc.calc_dupire_local_vol_price_par(spot, dp, 16000, 303));
            return;
        }

        // Silahian Ch.4: Multi-Scenario Stress Test & Tail Risk Intent
        if (lower_input.find("stress") != std::string::npos || lower_input.find("stres") != std::string::npos ||
            lower_input.find("cvar") != std::string::npos || lower_input.find("kuyruk") != std::string::npos ||
            lower_input.find("tail risk") != std::string::npos) {

            std::string sym = found_ticker.empty() ? "SPY" : LiveMarketDataClient::normalize_symbol(found_ticker);
            auto live = client.fetch_asset_smart(sym, "1y");
            DataQualityDashboard::record_asset_quality(live);
            std::vector<double> rets;
            const auto& cl = live.historical_prices;
            for (size_t i = 1; i < cl.size(); ++i) {
                if (cl[i - 1] > 0.0 && cl[i] > 0.0) rets.push_back((cl[i] - cl[i - 1]) / cl[i - 1]);
            }
            auto st = DrawdownRiskEngine::run_stress_scenarios(rets, 100000.0, 12.0);
            std::cout << std::format("{} [Silahian Ch.4 Kuyruk & Stres Analizi] ({}, Portföy=100,000 {}):\n"
                                     "  • Tarihsel %95 / %99 VaR (1G) : {:.0f} / {:.0f}\n"
                                     "  • Expected Shortfall (CVaR 95): {:.0f}\n"
                                     "  • -%15 Piyasa Şoku / 2x Likidite Donması: -{:.0f} / -{:.0f}\n",
                                     live.is_live ? "🟢 CANLI" : "🟡 SİMÜLASYON", live.symbol, live.currency_display,
                                     st.hist_var_95, st.hist_var_99, st.cvar_95,
                                     st.market_crash_15pct_loss, st.vol_2x_liquidity_freeze_loss);
            return;
        }

        // Silahian Ch.3/5: System Latency SLO, Circuit Breaker & Telemetry Intent
        if (found_ticker.empty() &&
            (lower_input.find("latency") != std::string::npos || lower_input.find("gecikme") != std::string::npos ||
             lower_input.find("circuit") != std::string::npos || lower_input.find("devre") != std::string::npos ||
             lower_input.find("telemetri") != std::string::npos || lower_input.find("telemetry") != std::string::npos)) {
            DataQualityDashboard::render_dashboard();
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
                auto spy = client.fetch_asset_smart("SPY", "6mo");
                auto yc = client.fetch_treasury_yields();
                DataQualityDashboard::record_asset_quality(spy);

                double spot = (spy.current_price > 0.0) ? spy.current_price : 500.0;
                double strike = std::round(spot);
                double r = (yc.is_live && yc.three_month_yield > 0.0) ? yc.three_month_yield : 0.045;
                double q = 0.013;
                double sigma = 0.20;

                if (spy.historical_prices.size() >= 22) {
                    size_t start_idx = spy.historical_prices.size() - 21;
                    std::vector<double> lrets;
                    for (size_t i = start_idx; i < spy.historical_prices.size(); ++i) {
                        if (spy.historical_prices[i - 1] > 0.0 && spy.historical_prices[i] > 0.0) {
                            lrets.push_back(std::log(spy.historical_prices[i] / spy.historical_prices[i - 1]));
                        }
                    }
                    if (lrets.size() >= 10) {
                        double mean = 0.0;
                        for (double x : lrets) mean += x;
                        mean /= static_cast<double>(lrets.size());
                        double var = 0.0;
                        for (double x : lrets) var += (x - mean) * (x - mean);
                        var /= static_cast<double>(lrets.size() - 1);
                        sigma = std::clamp(std::sqrt(var * 252.0), 0.05, 1.50);
                    }
                }

                BlackScholes bs(strike, spot, 0.5, PayoffType::Call, r, q);
                double call_p = bs(sigma);
                auto greeks = bs.risk_values(sigma);
                std::cout << std::format("{} Analitik Black-Scholes ({} ATM: S=${:.2f}, K=${:.0f}, T=0.5y, \u03c3={:.1f}%, r={:.2f}%):\n"
                                         "  • Call Primi: ${:.2f}  │  Delta: {:.4f}  │  Gamma: {:.4f}  │  Vega: {:.4f}\n"
                                         "  👉 Detaylı varlık seçimi, vade ve Monte Carlo/Binom opsiyon fiyatlaması için [4] seçiniz.\n",
                                         spy.is_live ? "🟢 CANLI" : "🟡 ÇEVRİMDIŞI", spy.symbol,
                                         spot, strike, sigma * 100.0, r * 100.0,
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

        auto data = client.fetch_asset_smart(found_ticker, "1y");
        found_ticker = data.symbol;

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
        DataQualityDashboard::record_asset_quality(data, &fund);
        TreasuryYieldCurveData yc;
        bool has_yc = false;
        if (fund.asset_class == AssetClassType::CommodityOrFuture || data.currency_code == "USD") {
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
                data, sent.sentiment_score, custom_days, custom_vade_label, sent.has_live_news,
                &fund, has_yc ? &yc : nullptr
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
            "opsiyon", "black", "scholes", "heston", "merton", "dupire", "stokastik", "stochastic", "local", "yerel",
            "hft", "lob", "emir", "defteri", "order", "book", "orderbook", "ior", "router", "smart", "ring", "buffer", "rms", "nash",
            "dpo", "pso", "stress", "stres", "testi", "cvar", "kuyruk", "tail", "latency", "gecikme", "circuit", "devre", "telemetri", "telemetry", "cache", "yap", "vol", "jump", "vs",
            "isabet", "başarı", "basari", "geçmiş", "gecmis", "doğru", "dogru",
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

            // Handle Turkish-orthography tokens safely without matching Turkish prose words
            bool has_non_ascii = false;
            for (unsigned char b : tok) {
                if (b >= 0x80) { has_non_ascii = true; break; }
            }
            if (has_non_ascii) {
                std::string folded = LiveMarketDataClient::normalize_symbol(tok);
                if (folded.find(".IS") != std::string::npos || folded.find(".DE") != std::string::npos) {
                    return folded;
                }
                static const std::unordered_map<std::string, std::string> kTrTickers = {
                    {"ISCTR", "ISCTR.IS"}, {"SISE", "SISE.IS"}, {"TUPRS", "TUPRS.IS"},
                    {"ARCLK", "ARCLK.IS"}, {"ULKER", "ULKER.IS"}, {"CIMSA", "CIMSA.IS"},
                    {"GUBRF", "GUBRF.IS"}, {"GOLTS", "GOLTS.IS"}, {"ISGYO", "ISGYO.IS"},
                    {"ISMEN", "ISMEN.IS"}, {"ISFIN", "ISFIN.IS"}, {"ISYAT", "ISYAT.IS"},
                    {"OZKGY", "OZKGY.IS"}, {"DOAS", "DOAS.IS"}, {"GUBRE", "GUBRF.IS"},
                    {"SISECAM", "SISE.IS"}, {"TUPRAS", "TUPRS.IS"}, {"ARCELIK", "ARCLK.IS"},
                    {"GOLTAS", "GOLTS.IS"}, {"ISBANK", "ISCTR.IS"}, {"DOGUS", "DOAS.IS"}
                };
                if (auto it = kTrTickers.find(folded); it != kTrTickers.end()) {
                    return it->second;
                }
                continue;
            }

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
