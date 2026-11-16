#pragma once

#include "dates.hpp"
#include "daycount.hpp"
#include "payoff.hpp"
#include "option_info.hpp"
#include "black_scholes.hpp"
#include "monte_carlo.hpp"
#include "binomial_lattice.hpp"
#include "yield_curve.hpp"
#include "bond.hpp"
#include "portfolio_linear_algebra.hpp"
#include "trading_signals.hpp"
#include "pairs_trading.hpp"
#include "fundamentals.hpp"
#include "news_sentiment.hpp"
#include "forecast_engine.hpp"
#include "forecast_backtest.hpp"
#include "alpha_screener.hpp"
#include "data_quality.hpp"
#include "hft_lob_engine.hpp"
#include "backtester.hpp"
#include "cross_sectional_lab.hpp"
#include "global_report_pdf.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>
#include <cassert>
#include <cmath>
#include <set>

namespace quant {

class AutomatedTestSuite {
public:
    static bool run_all_tests() {
        std::cout << "\n=======================================================\n";
        std::cout << "  RUNNING AUTOMATED QUANT & FUNDAMENTAL VERIFICATION   \n";
        std::cout << "=======================================================\n";
        
        bool all_ok = true;

        all_ok &= test_dates_and_daycounts();
        all_ok &= test_black_scholes_and_iv();
        all_ok &= test_monte_carlo_and_barriers();
        all_ok &= test_binomial_lattice();
        all_ok &= test_yield_curve_and_bond();
        all_ok &= test_linear_algebra_and_pca();
        all_ok &= test_trading_indicators_and_drawdown();
        all_ok &= test_forecast_engine();
        all_ok &= test_forecast_backtest();
        all_ok &= test_alpha_screener();
        all_ok &= test_json_parser_and_microstructure();
        all_ok &= test_word_boundary_nlp_sentiment();
        all_ok &= test_multicurrency_favok_fundamentals();
        all_ok &= test_data_quality_and_alerts();
        all_ok &= test_live_intraday_cache_and_yield_wiring();
        all_ok &= test_silahian_hft_lob_rms_and_nash_mm();
        all_ok &= test_silahian_ml_dpo_ior_and_heston_merton();
        all_ok &= test_silahian_full_app_resilience_dupire_stress();
        all_ok &= test_sticky_sentiment_and_event_shock();
        all_ok &= test_cross_sectional_ic_portfolio_and_mcc();
        all_ok &= test_fake_alpha_diagnostics_leakage_and_robustness();
        all_ok &= test_dashboard_report_export_integrity();
        all_ok &= test_native_unicode_pdf_report_engine();

        std::cout << "\n-------------------------------------------------------\n";
        if (all_ok) {
            std::cout << "\033[32m[PASS] ALL 23 QUANT, CROSS-SECTIONAL, FAKE-ALPHA, STICKY-NLP, HFT/LOB & PDF REPORT TESTS PASSED!\033[0m\n";
        } else {
            std::cout << "\033[31m[FAIL] SOME TESTS FAILED!\033[0m\n";
        }
        std::cout << "=======================================================\n\n";
        return all_ok;
    }

private:
    static bool check_close(double actual, double expected, double tol, const std::string& name) {
        double diff = std::abs(actual - expected);
        bool ok = diff <= tol;
        if (ok) {
            std::cout << std::format("  \033[32m✔\033[0m {:<42} Actual: {:10.4f} | Expected: {:10.4f}\n", name, actual, expected);
        } else {
            std::cout << std::format("  \033[31m✘\033[0m {:<42} Actual: {:10.4f} | Expected: {:10.4f} (diff: {})\n", name, actual, expected, diff);
        }
        return ok;
    }

    static bool check_cond(bool cond, const std::string& msg) {
        if (cond) {
            std::cout << std::format("  \033[32m✔\033[0m {}\n", msg);
        } else {
            std::cout << std::format("  \033[31m✘\033[0m FAILED: {}\n", msg);
        }
        return cond;
    }

    static bool test_dates_and_daycounts() {
        std::cout << "\n[TEST 1] ChronoDate & Day Count Conventions (Chapter 7):\n";
        bool ok = true;

        ChronoDate d1{2021, 4, 26};
        ChronoDate d2{2023, 10, 26};

        Act365 act365;
        Act360 act360;
        Thirty360 thirty360;

        // Ground truth values from Chapter 7, Page 270:
        // Act365 = 2.50137, Act360 = 2.53611, Thirty360 = 2.50000
        ok &= check_close(act365.year_fraction(d1, d2), 2.50137, 1e-4, "Act/365 Year Fraction");
        ok &= check_close(act360.year_fraction(d1, d2), 2.53611, 1e-4, "Act/360 Year Fraction");
        ok &= check_close(thirty360.year_fraction(d1, d2), 2.50000, 1e-4, "30/360 ISDA Year Fraction");

        // Weekend Roll Test (Modified Following)
        ChronoDate sun{2024, 1, 7}; // Sunday -> should roll to 2024-01-08 Monday
        sun.weekend_roll();
        ok &= (sun.day() == 8 && sun.month() == 1);
        std::cout << "  \033[32m✔\033[0m Weekend Roll (Modified Following): Sunday rolls to " << sun << "\n";

        return ok;
    }

    static bool test_black_scholes_and_iv() {
        std::cout << "\n[TEST 2] Black-Scholes, Greeks & Implied Volatility (Chapters 2 & 4):\n";
        bool ok = true;

        // Chapter 2, Page 61: ITM Call at expiration (S=100, X=75) -> 25.0
        BlackScholes bsc_itm{75.0, 100.0, 0.0, PayoffType::Call, 0.05, 0.0};
        ok &= check_close(bsc_itm(0.25), 25.0, 1e-4, "ITM Call at Expiration");

        // Chapter 2, Page 61: OTM Put with time remaining (S=100, X=75, T=0.25, r=0.05, q=0.075, vol=0.25) -> 0.04487
        BlackScholes bsp_otm{75.0, 100.0, 0.25, PayoffType::Put, 0.05, 0.075};
        ok &= check_close(bsp_otm(0.25), 0.04487, 1e-4, "OTM Put Price with Time Value");

        // Chapter 2, Page 64: Secant Method Implied Volatility
        // S=100, X=95, T=0.25, r=0.05, q=0.07, Market Price = 6.20 -> Implied Vol ~ 0.18501
        BlackScholes bsc_iv{95.0, 100.0, 0.25, PayoffType::Call, 0.05, 0.07};
        double iv = calculate_implied_volatility(bsc_iv, 6.20, 0.10, 0.15);
        ok &= check_close(iv, 0.18501, 1e-4, "Secant Implied Volatility");

        // Verify recovered price equals market price
        ok &= check_close(bsc_iv(iv), 6.20, 1e-4, "Recovered Price at Implied Vol");

        return ok;
    }

    static bool test_monte_carlo_and_barriers() {
        std::cout << "\n[TEST 3] Monte Carlo & Barrier Options (Chapter 6):\n";
        bool ok = true;

        // European Call: S=100, X=75, T=0.5, vol=0.25, r=0.05, q=0.075
        // Analytic price ~ 23.52 (Page 224)
        OptionInfo opt_call{std::make_unique<CallPayoff>(75.0), 0.5};
        MCOptionValuation mc_val{std::move(opt_call), 12, 0.25, 0.05, 0.075};

        double serial_price = mc_val.calc_price(100.0, 20000, 42);
        double par_price    = mc_val.calc_price_par(100.0, 20000, 42);

        ok &= check_close(serial_price, 23.51, 0.25, "Monte Carlo Vanilla Call (Serial)");
        ok &= check_close(par_price, 23.51, 0.25, "Monte Carlo Vanilla Call (Parallel)");

        // Barrier Option: Up-and-out at 110.0 (Page 229)
        OptionInfo opt_barr{std::make_unique<CallPayoff>(75.0), 0.5};
        MCOptionValuation mc_barr{std::move(opt_barr), 12, 0.25, 0.05, 0.075,
                                  BarrierType::up_and_out, 110.0};
        double barr_price = mc_barr.calc_price(100.0, 20000, 42);
        ok &= check_close(barr_price, 7.83, 0.40, "Up-and-Out Barrier Call");

        return ok;
    }

    static bool test_binomial_lattice() {
        std::cout << "\n[TEST 4] Binomial Lattice Option Pricing (Chapter 9):\n";
        bool ok = true;

        // Chapter 9, Pages 358-367:
        // S=36, X=36, T=1.0, r=0.06, vol=0.20, n=4
        // European Call -> 3.77
        OptionInfo call_opt{std::make_unique<CallPayoff>(36.0), 1.0};
        BinomialLatticePricer call_pricer{std::move(call_opt), 0.20, 0.06, 4};
        double euro_call = call_pricer.calc_price(36.0, OptType::Euro);
        ok &= check_close(euro_call, 3.77, 0.02, "4-Step Binomial Euro Call");

        // American Put: S=36, X=40, T=1.0, r=0.06, vol=0.20, n=4 -> 4.54
        OptionInfo put_opt{std::make_unique<PutPayoff>(40.0), 1.0};
        BinomialLatticePricer put_pricer{std::move(put_opt), 0.20, 0.06, 4};
        double amer_put = put_pricer.calc_price(36.0, OptType::American);
        ok &= check_close(amer_put, 4.54, 0.02, "4-Step Binomial American Put");

        return ok;
    }

    static bool test_yield_curve_and_bond() {
        std::cout << "\n[TEST 5] Fixed Income & Bond Valuation (Chapter 7):\n";
        bool ok = true;

        // Spot yields market data from Table 7-5, Page 296
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

        // 20-year bond with short first coupon period (Page 273 / 295)
        Bond bond{"20 yr bond",
                  ChronoDate{2023, 5, 8},
                  ChronoDate{2023, 11, 7},
                  ChronoDate{2042, 11, 7},
                  ChronoDate{2043, 5, 7},
                  2, 0.062, 1000.0};

        // Expected discounted bond value = 1320.38 (Page 275 / 297)
        double bond_val = bond.discounted_value(settle, yc);
        ok &= check_close(bond_val, 1320.38, 0.10, "20-Year Bond Discounted Value");

        return ok;
    }

    static bool test_linear_algebra_and_pca() {
        std::cout << "\n[TEST 6] Eigen Linear Algebra, QR & PCA (Chapter 8):\n";
        bool ok = true;

        // Portfolio Volatility: Page 316-317
        Eigen::Matrix3d corr;
        corr << 1.0,  0.5,  0.25,
                0.5,  1.0, -0.7,
                0.25, -0.7, 1.0;
        Eigen::Vector3d vols{0.2, 0.1, 0.4};
        Eigen::Vector3d weights{0.6, -0.3, 0.7};

        double port_vol = PortfolioLinearAlgebra::calculate_portfolio_volatility(corr, vols, weights);
        ok &= check_close(port_vol, 0.3445, 0.001, "Portfolio Volatility (34.45%)");

        // QR Factor Regression: Page 327
        Eigen::MatrixXd X(4, 2);
        X << 1.0, 2.0,
             2.0, 1.0,
             3.0, 4.0,
             4.0, 3.0;
        Eigen::VectorXd Y(4);
        Y << 5.0, 4.0, 11.0, 10.0; // Y = 1*X1 + 2*X2
        Eigen::VectorXd beta = PortfolioLinearAlgebra::solve_factor_regression_qr(X, Y);
        ok &= check_close(beta(0), 1.0, 0.01, "QR Regression Beta 1");
        ok &= check_close(beta(1), 2.0, 0.01, "QR Regression Beta 2");

        // Cholesky decomposition verification
        Eigen::Matrix2d cov;
        cov << 0.04, 0.01,
               0.01, 0.09;
        Eigen::Vector2d spots{100.0, 50.0};
        auto paths = PortfolioLinearAlgebra::simulate_correlated_basket_paths(cov, spots, 0.05, 1.0, 10, 123);
        ok &= check_cond(paths.rows() == 2 && paths.cols() == 11,
                         "Cholesky Basket Simulation dimensions verified (2x11)");

        return ok;
    }

    static bool test_trading_indicators_and_drawdown() {
        std::cout << "\n[TEST 7] Boost Accumulators & Drawdown Risk (Chapters 6 & 9):\n";
        bool ok = true;

        StreamingIndicators ind(5, 1.5, 3, 5);
        std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 105.0, 104.0, 106.0};
        IndicatorPoint last_pt;
        for (double p : prices) {
            last_pt = ind.update(p);
        }
        ok &= check_cond(last_pt.rolling_mean > 0.0 && last_pt.upper_band > last_pt.lower_band,
                         "Boost Accumulators Bollinger & MA updated cleanly");

        std::vector<double> pnl = {100.0, -50.0, 20.0, -80.0, 150.0, -30.0, 200.0};
        auto dd_res = DrawdownRiskEngine::analyze(pnl, 100, 42);
        ok &= check_cond(dd_res.original_max_drawdown >= 80.0,
                         std::format("Drawdown Risk Engine: Max DD = {:.1f}, 95% Nonparametric = {:.1f}",
                                     dd_res.original_max_drawdown, dd_res.nonparametric_95_dd));

        return ok;
    }

    static bool test_forecast_engine() {
        std::cout << "\n[TEST 8] Grinold-Kahn Multi-Factor Target Price & Horizon Scaling:\n";
        bool ok = true;

        // 1. Bullish Asset Test (strong uptrend + positive live news + strong FAVÖK score)
        RealTimeAssetData bull_data;
        bull_data.symbol = "SPY";
        bull_data.current_price = 500.0;
        bull_data.vwap = 492.0;
        bull_data.annualized_volatility = 0.14;
        bull_data.is_live = true;
        for (int i = 0; i < 65; ++i) {
            bull_data.historical_prices.push_back(460.0 + 40.0 * (static_cast<double>(i) / 64.0));
            bull_data.historical_volumes.push_back(50'000'000.0 + i * 250'000.0);
        }

        FundamentalProfile strong_fund;
        strong_fund.is_available = true;
        strong_fund.has_ebitda = true;
        strong_fund.ev_to_ebitda = 5.4;
        strong_fund.quality_grade = "A (Güçlü)";
        strong_fund.fundamental_score = 0.55;

        auto bull_fc = QuantForecastEngine::forecast_for_days(bull_data, 0.65, 30, "", true, &strong_fund);
        ok &= check_cond(bull_fc.target_price > bull_data.current_price &&
                         bull_fc.bull_target > bull_fc.target_price &&
                         bull_fc.bear_support < bull_fc.target_price,
                         std::format("Bullish 21-Trading-Day Target: {:.2f} (Band: {:.2f} .. {:.2f}) | {}",
                                     bull_fc.target_price, bull_fc.bear_support, bull_fc.bull_target, bull_fc.direction_label));
        ok &= check_cond(bull_fc.trading_days == 21, "Calendar 30d maps cleanly to 21 trading days (252-day basis)");

        // 2. Bearish Asset Test (downtrend + severe negative live shock + weak fundamentals)
        RealTimeAssetData bear_data;
        bear_data.symbol = "XYZ.US";
        bear_data.current_price = 90.0;
        bear_data.vwap = 104.0;
        bear_data.annualized_volatility = 0.32;
        bear_data.is_live = true;
        for (int i = 0; i < 65; ++i) {
            bear_data.historical_prices.push_back(120.0 - 30.0 * (static_cast<double>(i) / 64.0));
            bear_data.historical_volumes.push_back(10'000'000.0 + i * 150'000.0);
        }

        FundamentalProfile weak_fund;
        weak_fund.is_available = true;
        weak_fund.fundamental_score = -0.60;

        auto bear_fc = QuantForecastEngine::forecast_for_days(bear_data, -0.80, 7, "", true, &weak_fund);
        ok &= check_cond(bear_fc.target_price < bear_data.current_price && bear_fc.quant_score < 0.0,
                         std::format("Bearish 5-Trading-Day Target : {:.2f} (Score: {:+.2f}) | {}",
                                     bear_fc.target_price, bear_fc.quant_score, bear_fc.direction_label));

        // 3. Case-insensitive symbol & currency normalization
        std::string thyao_ccy = LiveMarketDataClient::currency_display_for(
            LiveMarketDataClient::infer_currency_code("thyao.is"), "thyao.is");
        ok &= check_cond(thyao_ccy == "TL" &&
                         LiveMarketDataClient::normalize_symbol("thyao") == "THYAO.IS" &&
                         LiveMarketDataClient::normalize_symbol("gram altın") == "GRAM-ALTIN.TL",
                         "UTF-8 & case-insensitive ticker/currency normalization ('thyao'->'THYAO.IS', 'gram altın'->'GRAM-ALTIN.TL')");

        return ok;
    }

    static bool test_forecast_backtest() {
        std::cout << "\n[TEST 9] Causal Walk-Forward Backtest & Non-Overlapping Block Validation:\n";
        bool ok = true;

        LiveMarketDataClient client;
        auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, "SPY", 5, "2y");
        ok &= check_cond(rep.total_predictions >= 200,
                         std::format("SPY 2y Causal Walk-Forward evaluated {} bars ({} independent 5d blocks)",
                                     rep.total_predictions, rep.non_overlapping_blocks));
        ok &= check_cond(rep.in_band_coverage_pct >= 50.0 && rep.in_band_coverage_pct <= 82.0,
                         std::format("1-Sigma Band Calibration Coverage: {:.1f}% (Target ~68.3%, RW MAPE={:.2f}%, Model MAPE={:.2f}%)",
                                     rep.in_band_coverage_pct, rep.random_walk_mape_pct, rep.mean_absolute_error_pct));
        ok &= check_cond(rep.mean_absolute_error_pct <= 8.0 &&
                         rep.mean_absolute_error_pct <= rep.random_walk_mape_pct + 0.15,
                         std::format("Active Conviction Accuracy: {:.1f}% ({} signals) | Block Accuracy: {:.1f}% (z={:+.2f})",
                                     rep.active_directional_accuracy_pct, rep.active_predictions,
                                     rep.non_overlapping_accuracy_pct, rep.binomial_z_stat));

        // Deterministic synthetic oscillating market test: verify causal IC adapts to mean reversion offline
        RealTimeAssetData mr_synth;
        mr_synth.symbol = "SYNTH_MR";
        mr_synth.currency_code = "USD";
        mr_synth.is_live = true;
        for (int i = 0; i < 240; ++i) {
            double p = 100.0 * std::exp(0.06 * std::sin(2.0 * 3.14159265358979 * i / 14.0));
            mr_synth.historical_prices.push_back(p);
            mr_synth.historical_highs.push_back(p * 1.008);
            mr_synth.historical_lows.push_back(p * 0.992);
            mr_synth.historical_opens.push_back(p);
            mr_synth.historical_volumes.push_back(1000000.0);
        }
        mr_synth.current_price = mr_synth.historical_prices.back();
        auto mr_rep = ForecastBacktestEngine::evaluate_on_asset_data(mr_synth, 5, "1y");
        ok &= check_cond(mr_rep.total_predictions >= 150 &&
                         mr_rep.directional_accuracy_pct >= 60.0 &&
                         mr_rep.mean_absolute_error_pct <= mr_rep.random_walk_mape_pct,
                         std::format("Causal Bayesian IC learns synthetic cycle (Hit={:.1f}%, MAPE={:.2f}% <= RW {:.2f}%)",
                                     mr_rep.directional_accuracy_pct, mr_rep.mean_absolute_error_pct, mr_rep.random_walk_mape_pct));

        // Short-IPO & empty/invalid series graceful handling without NaN or zero-division
        RealTimeAssetData ipo_short;
        ipo_short.symbol = "NEWIPO.IS";
        ipo_short.currency_code = "TRY";
        ipo_short.is_live = true;
        for (int i = 0; i < 36; ++i) ipo_short.historical_prices.push_back(50.0 + 0.4 * i);
        ipo_short.current_price = ipo_short.historical_prices.back();
        auto ipo_rep = ForecastBacktestEngine::evaluate_on_asset_data(ipo_short, 5, "2y");
        RealTimeAssetData empty_sym;
        empty_sym.symbol = "NONE";
        auto empty_rep = ForecastBacktestEngine::evaluate_on_asset_data(empty_sym, 5, "2y");
        ok &= check_cond(ipo_rep.total_predictions >= 10 &&
                         std::isfinite(ipo_rep.mean_absolute_error_pct) &&
                         empty_rep.total_predictions == 0,
                         std::format("Short IPO (36 bars -> {} preds) & empty ticker edge cases handled cleanly",
                                     ipo_rep.total_predictions));

        return ok;
    }

    static bool test_alpha_screener() {
        std::cout << "\n[TEST 10] Asymmetric Alpha Screener + Live Multi-Currency FAVÖK Integration:\n";
        bool ok = true;

        LiveMarketDataClient client;
        auto results = QuantOpportunityScanner::scan_market("BIST", client);
        ok &= check_cond(!results.empty(), std::format("Scanned {} live BIST assets", results.size()));

        std::set<int> distinct_rr_buckets;
        int live_vwap_distinct = 0;
        int favok_populated = 0;
        bool multi_currency_favok_sane = true;

        for (const auto& c : results) {
            distinct_rr_buckets.insert(static_cast<int>(std::round(c.asymmetry_ratio * 100.0)));
            if (std::abs(c.vwap_30d - c.current_price) > 1e-3) {
                ++live_vwap_distinct;
            }
            if (c.fundamentals.has_ebitda && std::isfinite(c.fundamentals.ev_to_ebitda)) {
                ++favok_populated;
                // Ensure USD/EUR-reporting BIST exporters (THYAO, ENKAI, TAVHL) NEVER suffer from 200x+ FX mismatch!
                if (c.symbol == "THYAO.IS" || c.symbol == "ENKAI.IS" || c.symbol == "TAVHL.IS") {
                    if (c.fundamentals.ev_to_ebitda <= 0.5 || c.fundamentals.ev_to_ebitda >= 30.0) {
                        multi_currency_favok_sane = false;
                    }
                }
            }
        }

        ok &= check_cond(distinct_rr_buckets.size() >= 4,
                         std::format("Asset-specific structural stops/targets verified ({} distinct R:R ratios)", distinct_rr_buckets.size()));
        ok &= check_cond(live_vwap_distinct >= static_cast<int>(results.size() / 2),
                         std::format("True 22d Typical-Price VWAP != Spot verified on {}/{} assets", live_vwap_distinct, results.size()));
        ok &= check_cond(favok_populated >= 5 && multi_currency_favok_sane,
                         std::format("Live FAVÖK (EBITDA) & FX-aligned FD/FAVÖK verified on {} BIST corporates", favok_populated));

        int ic_populated = 0;
        for (const auto& c : results) {
            if (c.predictability_samples >= 20 && std::isfinite(c.predictability_ic) &&
                c.predictability_ic >= -1.0 && c.predictability_ic <= 1.0) {
                ++ic_populated;
            }
        }
        ok &= check_cond(ic_populated >= static_cast<int>(results.size() / 2),
                         std::format("Causal Time-IC predictability computed on {}/{} live assets", ic_populated, results.size()));

        // Deterministic offline verification of Sticky Event-Shock Veto + Time-IC
        RealTimeAssetData synth;
        synth.symbol = "TEST.IS";
        synth.is_live = false;
        constexpr int N_BARS = 78; // index 77 mod 16 = 13 -> oversold trough bouncing
        synth.historical_prices.reserve(N_BARS);
        synth.historical_highs.reserve(N_BARS);
        synth.historical_lows.reserve(N_BARS);
        synth.historical_volumes.reserve(N_BARS);
        for (int i = 0; i < N_BARS; ++i) {
            double px = 100.0 + 7.5 * std::sin(static_cast<double>(i) * 2.0 * 3.14159265358979 / 16.0);
            synth.historical_prices.push_back(px);
            synth.historical_highs.push_back(px * 1.015);
            synth.historical_lows.push_back(px * 0.985);
            double prev_px = (i > 0) ? synth.historical_prices[i - 1] : px;
            synth.historical_volumes.push_back((px >= prev_px) ? 2000000.0 : 1200000.0);
        }
        synth.current_price = synth.historical_prices.back();
        synth.vwap = 100.0;
        synth.atr_14 = 2.2;
        synth.annualized_volatility = 0.28;

        FundamentalProfile empty_fund;
        AssetSentimentProfile neutral_sent;
        neutral_sent.has_live_news = true;
        neutral_sent.sentiment_score = 0.05;
        neutral_sent.sticky_sentiment_score = 0.05;
        neutral_sent.sent_strength = 0.08;
        neutral_sent.is_event_shock = false;

        AssetSentimentProfile shock_sent;
        shock_sent.has_live_news = true;
        shock_sent.sentiment_score = -0.72;
        shock_sent.sticky_sentiment_score = -0.78;
        shock_sent.sent_strength = 0.85;
        shock_sent.is_event_shock = true;

        auto cand_neutral = QuantOpportunityScanner::evaluate_candidate(
            "TEST.IS", "Test Holding A.S.", "Sanayi", "BIST", "TL",
            synth, empty_fund, -2.0, &neutral_sent);
        auto cand_shock = QuantOpportunityScanner::evaluate_candidate(
            "TEST.IS", "Test Holding A.S.", "Sanayi", "BIST", "TL",
            synth, empty_fund, -2.0, &shock_sent);

        ok &= check_cond(
            cand_neutral.predictability_samples >= 40 &&
                cand_neutral.predictability_ic > 0.25 &&
                cand_neutral.predictability_hit_rate > 0.55,
            std::format("Mean-reverting series yields positive causal Time-IC={:+.3f}, Hit={:.1f}% (n={})",
                        cand_neutral.predictability_ic,
                        cand_neutral.predictability_hit_rate * 100.0,
                        cand_neutral.predictability_samples));
        ok &= check_cond(
            cand_neutral.category != AlphaCategory::CrowdedTradeWarning &&
                cand_shock.category == AlphaCategory::CrowdedTradeWarning &&
                cand_shock.composite_alpha_score < cand_neutral.composite_alpha_score - 1.5 &&
                cand_shock.target_price < cand_neutral.target_price,
            std::format("Sticky Event-Shock Veto downgrades setup & cuts score ({:+.2f} -> {:+.2f}) and target ({:.2f} -> {:.2f})",
                        cand_neutral.composite_alpha_score, cand_shock.composite_alpha_score,
                        cand_neutral.target_price, cand_shock.target_price));

        return ok;
    }

    static bool test_json_parser_and_microstructure() {
        std::cout << "\n[TEST 11] Boost.JSON Null-Bar Alignment, Wilder ATR & Date-Aligned Pairs:\n";
        bool ok = true;

        // Synthetic Yahoo v8 response containing a trailing intraday null close & zero volume bar
        std::string sample_json = R"({
          "chart": {
            "result": [{
              "meta": {
                "currency": "TRY",
                "symbol": "TEST.IS",
                "regularMarketPrice": 112.5,
                "regularMarketDayHigh": 114.0,
                "regularMarketDayLow": 111.0,
                "regularMarketVolume": 2500000,
                "gmtoffset": 10800
              },
              "timestamp": [1704153600, 1704240000, 1704326400, 1704412800, 1704672000, 1704758400],
              "indicators": {
                "quote": [{
                  "open":   [100.0, 102.0, 105.0, 107.0, 109.0, null],
                  "high":   [103.0, 106.0, 108.0, 110.0, 113.0, null],
                  "low":    [99.0,  101.0, 104.0, 106.0, 108.0, null],
                  "close":  [102.0, 105.0, 107.0, 109.0, 112.0, null],
                  "volume": [1000000, 1200000, 1100000, 1300000, 1500000, 0]
                }],
                "adjclose": [{
                  "adjclose": [102.0, 105.0, 107.0, 109.0, 112.0, null]
                }]
              }
            }],
            "error": null
          }
        })";

        RealTimeAssetData parsed;
        bool p_ok = LiveMarketDataClient::parse_yahoo_chart_json(sample_json, parsed);
        LiveMarketDataClient::compute_analytics(parsed);

        ok &= check_cond(p_ok && parsed.historical_prices.size() == 5 &&
                         parsed.historical_volumes.size() == 5 &&
                         parsed.day_keys.size() == 5,
                         "Null intraday bar cleanly skipped; OHLCV & day_keys arrays strictly synchronized (size=5)");
        ok &= check_cond(std::abs(parsed.vwap - parsed.current_price) > 1.0 && parsed.atr_14 > 2.5,
                         std::format("VWAP ({:.2f}) != Spot ({:.2f}) and Wilder OHLC ATR-14 = {:.2f}",
                                     parsed.vwap, parsed.current_price, parsed.atr_14));

        // Date alignment test: drop holiday bar present in A (FX) but missing in B (Exchange)
        RealTimeAssetData fx_a, eq_b;
        fx_a.symbol = "TRY=X";
        fx_a.day_keys = {19724, 19725, 19726, 19727, 19728, 19729};
        fx_a.historical_prices = {30.0, 30.1, 30.2, 30.3, 30.4, 30.5};
        eq_b.symbol = "GC=F";
        eq_b.day_keys = {19724, 19726, 19727, 19728, 19729}; // 19725 holiday missing
        eq_b.historical_prices = {2000.0, 2020.0, 2030.0, 2040.0, 2050.0};

        auto aligned = LiveMarketDataClient::align_series_by_date(fx_a, eq_b, false);
        ok &= check_cond(aligned.prices_a.size() == 5 && aligned.prices_a[1] == 30.2 && aligned.prices_b[1] == 2020.0,
                         "Date-key join eliminates cross-exchange holiday drift (5 exact matched days)");

        return ok;
    }

    static bool test_word_boundary_nlp_sentiment() {
        std::cout << "\n[TEST 12] Word-Boundary NLP & Turkish UTF-8 Negation-Aware Sentiment:\n";
        bool ok = true;

        // Former substring false positives MUST score 0.0
        double s_ent  = NewsSentimentEngine::score_headline("Enterprise AI software architecture update");
        double s_ag   = NewsSentimentEngine::score_headline("Hedging against currency volatility");
        double s_exec = NewsSentimentEngine::score_headline("Company executes strategic plan");
        double s_veri = NewsSentimentEngine::score_headline("Veritabanı altyapı güncellemesi tamamlandı");

        ok &= check_close(s_ent,  0.0, 1e-6, "No false 'rise' in 'Enterprise'");
        ok &= check_close(s_ag,   0.0, 1e-6, "No false 'gain' in 'against'");
        ok &= check_close(s_exec, 0.0, 1e-6, "No false 'cut' in 'executes'");
        ok &= check_close(s_veri, 0.0, 1e-6, "No false 'eri' in Turkish 'Veritabanı'");

        // Genuine Turkish & English sentiment + negation flip
        double s_tr_pos = NewsSentimentEngine::score_headline("Şirket rekor net kâr ve güçlü büyüme açıkladı");
        double s_tr_inf = NewsSentimentEngine::score_headline("ASELSAN yükselişi rekorunu kırdı ve yeni siparişleri açıkladı");
        double s_tr_neg = NewsSentimentEngine::score_headline("Şirket zararını açıkladı ve hisseleri sert düştüğü için baskısı arttı");
        double s_neg_flip = NewsSentimentEngine::score_headline("No recession expected as economy remains stable");
        double s_contrast = NewsSentimentEngine::score_headline("Company beats estimates, but weak guidance sparks selloff");
        double s_scandal  = NewsSentimentEngine::score_headline("What the Nvidia scandal means after CEO resignation and espionage accusation");
        double s_tr_scand = NewsSentimentEngine::score_headline("CEO casusluk skandalına karıştı, suçlamalar sonrası istifa ve soruşturma başladı");
        double rel_nvda_on_aapl = NewsSentimentEngine::compute_headline_relevance_(
            "5 big analyst AI moves: Nvidia hit with downgrades after Huang's exit", "AAPL", "Apple Inc.");
        double rel_aapl_on_aapl = NewsSentimentEngine::compute_headline_relevance_(
            "Evercore explains why Apple has the right assets to win", "AAPL", "Apple Inc.");

        ok &= check_cond(s_tr_pos > 0.35,
                         std::format("Turkish UTF-8 positive headline ('rekor net kâr ve güçlü büyüme') = {:+.2f}", s_tr_pos));
        ok &= check_cond(s_tr_inf > 0.30 && s_tr_neg < -0.25,
                         std::format("Turkish inflected stem polarity (pos={:+.2f}, neg={:+.2f})", s_tr_inf, s_tr_neg));
        ok &= check_cond(s_scandal < -0.75 && s_tr_scand < -0.75,
                         std::format("Corporate scandal / espionage / CEO resignation polarity (EN={:+.2f}, TR={:+.2f})", s_scandal, s_tr_scand));
        ok &= check_cond(s_neg_flip >= 0.0 && s_contrast < -0.15,
                         std::format("Negation & contrastive pivot ('No recession'={:+.2f}, 'but selloff'={:+.2f})", s_neg_flip, s_contrast));
        ok &= check_cond(rel_nvda_on_aapl == 0.0 && rel_aapl_on_aapl == 1.0,
                         std::format("Cross-ticker headline isolation verified (Nvidia->AAPL: {:.1f}, Apple->AAPL: {:.1f})",
                                     rel_nvda_on_aapl, rel_aapl_on_aapl));

        // MSCI / Index deletion & demotion shock coverage + positive-control isolation
        double s_msci_std  = NewsSentimentEngine::score_headline("MSCI, Sabancı ve Astor'u standart endeksinden çıkardı");
        double s_msci_sml  = NewsSentimentEngine::score_headline("MSCI kararı: Sabancı ve Astor küçük ölçekli endekse indi, 18 hisse listeden çıktı");
        double s_msci_sil  = NewsSentimentEngine::score_headline("MSCI'dan Türkiye kararı: Sabancı Holding ve Astor'un endeksi değişti, 18 hisse listeden silindi");
        double s_msci_en   = NewsSentimentEngine::score_headline("Company removed from MSCI Standard index");
        double s_tp_raise  = NewsSentimentEngine::score_headline("Hedef fiyat 500 TL'ye çıkarıldı");
        double s_idx_add   = NewsSentimentEngine::score_headline("MSCI Türkiye endeksine eklendi");
        double rel_enerji  = NewsSentimentEngine::compute_headline_relevance_("Elektrik enerji tarifesi değişti", "ASTOR.IS", "Astor Enerji");

        ok &= check_cond(
            s_msci_std < -0.70 && s_msci_sml < -0.70 && s_msci_sil < -0.70 && s_msci_en < -0.70 &&
                std::abs(s_tp_raise) < 1e-6 && s_idx_add > 0.35 && rel_enerji < 0.5,
            std::format("MSCI/index exclusion detected (std={:+.2f}, small={:+.2f}, del={:+.2f}, EN={:+.2f}); neutral-raise={:.2f}, add={:+.2f}, sector-rel={:.2f}",
                        s_msci_std, s_msci_sml, s_msci_sil, s_msci_en, s_tp_raise, s_idx_add, rel_enerji));

        return ok;
    }

    static bool test_multicurrency_favok_fundamentals() {
        std::cout << "\n[TEST 13] Multi-Currency FAVÖK (EBITDA), Bank & Commodity Classification:\n";
        bool ok = true;

        // 1. USD-Reporting BIST Exporter (THYAO.IS archetype: TRY stock price, USD financial statements)
        RawFundamentalInputs thyao_raw;
        thyao_raw.symbol = "THYAO.IS";
        thyao_raw.company_name = "Türk Hava Yolları A.O.";
        thyao_raw.trading_currency = "TRY";
        thyao_raw.financial_currency = "USD";
        thyao_raw.spot_price = 292.75;
        thyao_raw.fx_fin_to_trade = 50.13; // 1 USD = 50.13 TRY
        thyao_raw.shares_outstanding = 1'380'000'000.0;
        thyao_raw.asset_class = AssetClassType::Equity;
        thyao_raw.has_ebitda = true;  thyao_raw.ebitda_ttm = 6.101e9;       // $6.10B EBITDA
        thyao_raw.has_revenue = true; thyao_raw.revenue_ttm = 24.50e9;      // $24.50B Revenue
        thyao_raw.has_net_income = true; thyao_raw.net_income_ttm = 2.56e9; // $2.56B Net Income
        thyao_raw.has_debt = true;    thyao_raw.total_debt = 15.86e9;
        thyao_raw.has_cash = true;    thyao_raw.cash_and_st_investments = 6.76e9;
        thyao_raw.has_equity = true;  thyao_raw.stockholders_equity = 23.25e9;

        auto thyao_prof = FundamentalEngine::compute_from_raw(thyao_raw);
        // Market Cap in USD = (292.75 * 1.38e9) / 50.13 = $8.059B
        // EV in USD = 8.059B + (15.86B - 6.76B) = $17.159B -> EV/EBITDA = 17.159 / 6.101 = 2.81x
        ok &= check_close(thyao_prof.ev_to_ebitda, 2.8125, 0.05, "FX-Aligned THYAO FD/FAVÖK (USD/TRY)");
        ok &= check_close(thyao_prof.pb_ratio, 0.3466, 0.02, "FX-Aligned THYAO PD/DD");
        ok &= check_close(thyao_prof.pe_ratio, 3.1480, 0.05, "FX-Aligned THYAO F/K");

        // 2. Net-Cash Industrial Equity (Net Borç / FAVÖK < 0)
        RawFundamentalInputs netcash_raw = thyao_raw;
        netcash_raw.symbol = "ASTOR.IS";
        netcash_raw.financial_currency = "TRY";
        netcash_raw.fx_fin_to_trade = 1.0;
        netcash_raw.total_debt = 2.0e9;
        netcash_raw.cash_and_st_investments = 9.5e9;
        netcash_raw.ebitda_ttm = 10.0e9;
        auto netcash_prof = FundamentalEngine::compute_from_raw(netcash_raw);
        ok &= check_cond(netcash_prof.net_debt_to_ebitda < 0.0,
                         std::format("Net-Cash balance sheet verified: Net Borç/FAVÖK = {:+.2f}x (< 0)", netcash_prof.net_debt_to_ebitda));

        // 3. Bank Classification (FAVÖK must be structurally N/A; ROE & PD/DD active)
        RawFundamentalInputs bank_raw;
        bank_raw.symbol = "GARAN.IS";
        bank_raw.sector = "Financial Services";
        bank_raw.industry = "Banks - Regional";
        bank_raw.trading_currency = "TRY";
        bank_raw.financial_currency = "TRY";
        bank_raw.spot_price = 130.0;
        bank_raw.shares_outstanding = 4'200'000'000.0;
        bank_raw.asset_class = AssetClassType::BankOrFinancial;
        bank_raw.has_net_income = true; bank_raw.net_income_ttm = 92.0e9;
        bank_raw.has_equity = true;     bank_raw.stockholders_equity = 310.0e9;
        auto bank_prof = FundamentalEngine::compute_from_raw(bank_raw);
        ok &= check_cond(!bank_prof.has_ebitda && !std::isfinite(bank_prof.ev_to_ebitda) &&
                         bank_prof.roe_pct > 25.0 && bank_prof.pb_ratio > 1.0,
                         std::format("Bank profile verified (GARAN.IS): FAVÖK=N/A, ROE={:.1f}%, PD/DD={:.2f}x",
                                     bank_prof.roe_pct, bank_prof.pb_ratio));

        // 4. Commodity Classification (GRAM-ALTIN.TL -> Non-Equity Macro Asset)
        RealTimeAssetData gold_asset;
        gold_asset.symbol = "GRAM-ALTIN.TL";
        gold_asset.current_price = 6450.0;
        auto cls = FundamentalEngine::classify_asset_type(gold_asset);
        auto gold_prof = FundamentalEngine::build_non_equity_profile(gold_asset, cls);
        ok &= check_cond(cls == AssetClassType::CommodityOrFuture && !gold_prof.is_available && !gold_prof.has_ebitda,
                         "Commodity profile verified (GRAM-ALTIN.TL): CommodityOrFuture, no corporate balance sheet");

        return ok;
    }

    static bool test_data_quality_and_alerts() {
        std::cout << "\n[TEST 14] Data Quality Telemetry, Scoring Integrity, Alerts & Normalization:\n";
        bool ok = true;

        auto has_alert = [](const HealthSummary& hs, const std::string& code) {
            for (const auto& a : hs.alerts) {
                if (a.code == code) return true;
            }
            return false;
        };

        // 1. Silent-zero guard: empty API & FX telemetry MUST cap overall score & raise Critical alerts
        DataQualityDashboard::reset();
        RealTimeAssetData spy_dummy;
        spy_dummy.symbol = "SPY";
        spy_dummy.is_live = true;
        spy_dummy.raw_bar_count = 23;
        spy_dummy.historical_prices.assign(23, 500.0);
        spy_dummy.historical_raw_closes.assign(23, 500.0);
        DataQualityDashboard::record_asset_quality(spy_dummy);

        auto h_empty = DataQualityDashboard::evaluate_health();
        ok &= check_cond(h_empty.api_score == 0.0 && h_empty.fx_score == 0.0 &&
                         h_empty.overall_score <= 0.45 && h_empty.has_critical() &&
                         has_alert(h_empty, "API_TELEMETRY_EMPTY") &&
                         has_alert(h_empty, "FX_TELEMETRY_EMPTY") &&
                         has_alert(h_empty, "PILLAR_ZERO"),
                         std::format("Zero API/FX telemetry prevented inflated score (overall={:.2f}) and raised Critical alerts",
                                     h_empty.overall_score));

        // 2. Fallback FX & offline endpoint/asset detection
        DataQualityDashboard::reset();
        for (const auto& [src, ep] : DataQualityTelemetry::expected_endpoints()) {
            bool ep_ok = (src != "Yahoo v10 QuoteSummary");
            DataQualityDashboard::record_fetch(src, ep, ep_ok, std::chrono::milliseconds(120), false,
                                               ep_ok ? "" : "HTTP 401", ep_ok ? 200 : 401);
        }
        DataQualityDashboard::record_fx_rate("USD/TRY", 38.0, false, "fallback");
        DataQualityDashboard::record_fx_rate("EUR/TRY", 41.0, true, "live");
        DataQualityDashboard::record_fx_rate("EUR/USD", 1.08, true, "live");
        DataQualityDashboard::record_fx_rate("TRY/USD", 1.0 / 38.0, false, "fallback");
        DataQualityDashboard::record_fx_rate("TRY/EUR", 1.0 / 41.0, true, "live (inv)");

        RealTimeAssetData fail_asset;
        fail_asset.symbol = "FAIL.IS";
        fail_asset.is_live = false;
        fail_asset.raw_bar_count = 10;
        fail_asset.historical_prices.assign(10, 100.0);
        DataQualityDashboard::record_asset_quality(fail_asset);

        auto h_deg = DataQualityDashboard::evaluate_health();
        ok &= check_cond(h_deg.overall_score < 0.50 && h_deg.has_critical() &&
                         has_alert(h_deg, "API_ENDPOINT_OFFLINE") &&
                         has_alert(h_deg, "FX_FALLBACK_ACTIVE") &&
                         has_alert(h_deg, "ASSET_OFFLINE"),
                         std::format("Offline endpoint, fallback FX & offline asset detected (overall={:.2f}, alerts={})",
                                     h_deg.overall_score, h_deg.alerts.size()));

        // 3. Null-bar counting & symbol normalization in get_inline_status ("GRAM-ALTIN" -> "GRAM-ALTIN.TL")
        DataQualityDashboard::reset();
        std::string null_json = R"({
          "chart": {
            "result": [{
              "meta": { "currency": "USD", "symbol": "NUL.TEST", "regularMarketPrice": 105.0, "gmtoffset": 0 },
              "timestamp": [1704153600, 1704240000, 1704326400, 1704412800, 1704672000, 1704758400],
              "indicators": {
                "quote": [{
                  "open":   [100.0, 101.0, 102.0, 103.0, 104.0, null],
                  "high":   [101.0, 102.0, 103.0, 104.0, 105.0, null],
                  "low":    [99.0,  100.0, 101.0, 102.0, 103.0, null],
                  "close":  [100.5, 101.5, 102.5, 103.5, 104.5, null],
                  "volume": [1000, 1100, 1200, 1300, 1400, 0]
                }]
              }
            }],
            "error": null
          }
        })";
        RealTimeAssetData null_parsed;
        bool np_ok = LiveMarketDataClient::parse_yahoo_chart_json(null_json, null_parsed);
        DataQualityDashboard::record_asset_quality(null_parsed);

        RealTimeAssetData gold_live;
        gold_live.symbol = "GRAM-ALTIN.TL";
        gold_live.is_live = true;
        gold_live.raw_bar_count = 22;
        gold_live.historical_prices.assign(22, 6500.0);
        gold_live.is_synthetic_metal = true;
        gold_live.underlying_usdtry = 50.0;
        DataQualityDashboard::record_asset_quality(gold_live);

        std::string gold_status = DataQualityDashboard::get_inline_status("GRAM-ALTIN");
        std::string trunc_ok = DataQualityTelemetry::utf8_truncate("query2.finance.yahoo.com/v10/finance/quoteSummary", 20);
        ok &= check_cond(np_ok && null_parsed.raw_bar_count == 6 && null_parsed.null_bars_skipped == 1 &&
                         gold_status.find("Live:Y") != std::string::npos &&
                         gold_status.find("Bars:22") != std::string::npos &&
                         trunc_ok == "query2.finance.yaho…",
                         std::format("Null-bar counter (1/6), UTF-8 truncate & GRAM-ALTIN inline lookup verified ({})", gold_status));

        // 4. End-to-end live dashboard pipeline check
        DataQualityDashboard::reset();
        LiveMarketDataClient client;
        std::vector<std::string> syms = {"SPY", "AAPL", "THYAO.IS", "GRAM-ALTIN", "BTC-USD"};
        for (const auto& s : syms) {
            auto d = client.fetch_asset(s, "1mo");
            auto f = FundamentalEngine::fetch_fundamentals(client, d);
            DataQualityDashboard::record_asset_quality(d, &f);
            NewsSentimentEngine::get_asset_sentiment(d.symbol, d.long_name, client);
        }
        client.fetch_treasury_yields();
        client.get_fx_rate("USD", "TRY");
        client.get_fx_rate("EUR", "TRY");
        client.get_fx_rate("EUR", "USD");
        client.get_fx_rate("TRY", "USD");
        client.get_fx_rate("TRY", "EUR");

        auto h_live = DataQualityDashboard::evaluate_health();
        std::string live_gold_inline = DataQualityDashboard::get_inline_status("GRAM-ALTIN");
        ok &= check_cond(h_live.api_tracked_count == 5 && h_live.fx_tracked_count == 5 &&
                         h_live.asset_tracked_count == 5 &&
                         h_live.api_score > 0.50 && h_live.fx_score > 0.50 &&
                         h_live.asset_score > 0.50 && h_live.overall_score > 0.50 &&
                         live_gold_inline.find("Live:Y") != std::string::npos,
                         std::format("Live telemetry pipeline populated (API:{:.2f}, FX:{:.2f}, Asset:{:.2f}, Overall:{:.2f})",
                                     h_live.api_score, h_live.fx_score, h_live.asset_score, h_live.overall_score));

        return ok;
    }

    static bool test_live_intraday_cache_and_yield_wiring() {
        std::cout << "\n[TEST 15] Testing Active Intraday Bar Recovery, Dynamic Cache & Live Yield Wiring...\n";
        bool ok = true;

        // 1. Live BIST intraday trailing bar recovery + chartPreviousClose isolation
        std::string thyao_json = R"({
          "chart": {
            "result": [{
              "meta": {
                "currency": "TRY",
                "symbol": "THYAO.IS",
                "regularMarketPrice": 320.25,
                "previousClose": null,
                "chartPreviousClose": 286.25,
                "regularMarketTime": 1794582590,
                "gmtoffset": 10800
              },
              "timestamp": [1794258000, 1794344400, 1794430800, 1794551400],
              "indicators": {
                "quote": [{
                  "open":   [295.0, 297.0, 299.0, 315.75],
                  "high":   [298.0, 300.0, 302.0, 323.50],
                  "low":    [294.0, 296.0, 298.0, 315.00],
                  "close":  [296.0, 298.0, 300.0, null],
                  "volume": [1000000, 1100000, 1200000, 29057838]
                }]
              }
            }],
            "error": null
          }
        })";

        RealTimeAssetData parsed;
        bool p_ok = LiveMarketDataClient::parse_yahoo_chart_json(thyao_json, parsed);
        ok &= check_cond(p_ok && parsed.null_bars_skipped == 0 &&
                         parsed.historical_prices.size() == 4 &&
                         std::abs(parsed.historical_prices.back() - 320.25) < 1e-6 &&
                         std::abs(parsed.historical_highs.back() - 323.50) < 1e-6 &&
                         std::abs(parsed.historical_lows.back() - 315.00) < 1e-6 &&
                         std::abs(parsed.previous_close - 300.0) < 1e-6,
                         std::format("THYAO trailing intraday bar recovered (bars={}, last={:.2f}, prevClose={:.2f} != chartPrev 286.25)",
                                     parsed.historical_prices.size(),
                                     parsed.historical_prices.empty() ? 0.0 : parsed.historical_prices.back(),
                                     parsed.previous_close));

        // 2. Dynamic Fundamental recomputation on spot change + cache reset
        LiveMarketDataClient client;
        client.clear_caches();
        FundamentalEngine::clear_cache();

        RealTimeAssetData aapl = client.fetch_asset_smart("AAPL", "1mo");
        double orig_spot = (aapl.current_price > 0.0) ? aapl.current_price : 200.0;
        aapl.current_price = orig_spot;
        auto f1 = FundamentalEngine::fetch_fundamentals(client, aapl);
        RealTimeAssetData aapl_bumped = aapl;
        aapl_bumped.current_price = orig_spot * 1.25;
        auto f2 = FundamentalEngine::fetch_fundamentals(client, aapl_bumped);

        if (f1.is_available && !std::isnan(f1.pe_ratio) && f1.pe_ratio > 0.0) {
            double ratio = f2.pe_ratio / f1.pe_ratio;
            ok &= check_cond(ratio > 1.23 && ratio < 1.27,
                             std::format("Fundamental cache dynamically recomputed P/E on +25% spot change ({:.2f} -> {:.2f})",
                                         f1.pe_ratio, f2.pe_ratio));
        } else {
            ok &= check_cond(f2.is_available == f1.is_available,
                             "Fundamental cache handled offline/partial state cleanly without crashing");
        }

        // 3. Yield-curve sensitivity in QuantForecastEngine
        RealTimeAssetData spy_synth;
        spy_synth.symbol = "SPY";
        spy_synth.currency_code = "USD";
        spy_synth.is_live = true;
        for (int i = 0; i < 65; ++i) {
            double p = 500.0 + 0.35 * i + ((i % 2 == 0) ? 0.6 : -0.4);
            spy_synth.historical_prices.push_back(p);
            spy_synth.historical_highs.push_back(p + 1.5);
            spy_synth.historical_lows.push_back(p - 1.5);
            spy_synth.historical_opens.push_back(p - 0.2);
            spy_synth.historical_volumes.push_back(50000000.0);
        }
        spy_synth.current_price = spy_synth.historical_prices.back();
        spy_synth.previous_close = spy_synth.historical_prices[spy_synth.historical_prices.size() - 2];

        TreasuryYieldCurveData yc_low{0.010, 0.020, 0.025, 0.030, true};
        TreasuryYieldCurveData yc_high{0.055, 0.050, 0.050, 0.052, true};

        auto fc_low = QuantForecastEngine::forecast_for_trading_days(
            spy_synth, 63, 0.0, false, nullptr, "3A", 90, &yc_low);
        auto fc_high = QuantForecastEngine::forecast_for_trading_days(
            spy_synth, 63, 0.0, false, nullptr, "3A", 90, &yc_high);

        ok &= check_cond(fc_high.expected_return_pct > fc_low.expected_return_pct &&
                         std::isfinite(fc_high.target_price),
                         std::format("USD forecast carry wired to live Treasury yields (1% -> {:+.2f}%, 5.5% -> {:+.2f}%)",
                                     fc_low.expected_return_pct, fc_high.expected_return_pct));

        return ok;
    }

    static bool test_silahian_hft_lob_rms_and_nash_mm() {
        std::cout << "\n[TEST 16] Silahian Ch.2/3/6/7: SPSC Hub, O(1) Circular LOB vs Map LOB, RMS & Nash MM:\n";
        bool ok = true;

        // 1. Lock-Free SPSC Ring Buffer FIFO & Cache Alignment
        static_assert(alignof(MarketTickMessage) == 64, "MarketTickMessage must be 64B cache-aligned");
        SpscRingBuffer<MarketTickMessage, 64> rb;
        ok &= check_cond(rb.empty() && rb.capacity() == 63, "SPSC RingBuffer starts empty with cap=63");

        bool pushed_all = true;
        for (uint64_t i = 1; i <= 63; ++i) {
            pushed_all &= rb.try_push(MarketTickMessage{i, i, static_cast<int64_t>(i * 10), 100.0 + 0.01 * i, OrderSide::Buy, 0, 1});
        }
        bool rejected_full = !rb.try_push(MarketTickMessage{64, 64, 10, 100.0, OrderSide::Buy, 0, 1});
        bool fifo_ok = true;
        for (uint64_t i = 1; i <= 63; ++i) {
            MarketTickMessage m;
            if (!rb.try_pop(m) || m.seq_id != i) fifo_ok = false;
        }
        ok &= check_cond(pushed_all && rejected_full && fifo_ok && rb.empty(),
                         "SPSC Lock-Free RingBuffer preserves strict FIFO order & handles back-pressure");

        // 2. O(1) CircularArrayLOB exact parity against std::map MapLimitOrderBook
        CircularArrayLOB circ_lob(90.0, 110.0, 0.05, 1024);
        MapLimitOrderBook map_lob(0.05);

        struct Op { uint8_t kind; uint64_t id; OrderSide side; double px; int64_t qty; };
        std::vector<Op> ops = {
            {0, 1, OrderSide::Buy,  99.90, 200},
            {0, 2, OrderSide::Buy,  99.95, 150},
            {0, 3, OrderSide::Buy,  99.95, 100},
            {0, 4, OrderSide::Sell, 100.05, 300},
            {0, 5, OrderSide::Sell, 100.10, 250},
            {1, 2, OrderSide::Buy,  0.0,   400}, // update order 2 qty 150 -> 400
            {2, 2, OrderSide::Buy,  0.0,   0},   // delete order 2 -> level 99.95 drops from 500 to 100
            {2, 3, OrderSide::Buy,  0.0,   0},   // delete order 3 -> best bid must scan down to 99.90!
            {2, 4, OrderSide::Sell, 0.0,   0},   // delete best ask 100.05 -> best ask must scan up to 100.10!
            {0, 6, OrderSide::Sell, 100.00, 180} // new tighter best ask
        };
        for (const auto& op : ops) {
            if (op.kind == 0) {
                circ_lob.add_order(op.id, op.side, op.px, op.qty);
                map_lob.add_order(op.id, op.side, op.px, op.qty);
            } else if (op.kind == 1) {
                circ_lob.update_order(op.id, op.qty);
                map_lob.update_order(op.id, op.qty);
            } else {
                circ_lob.delete_order(op.id);
                map_lob.delete_order(op.id);
            }
        }
        auto cb = circ_lob.get_best_bid(), mb = map_lob.get_best_bid();
        auto ca = circ_lob.get_best_offer(), ma = map_lob.get_best_offer();
        ok &= check_close(cb.price, mb.price, 1e-6, "CircularLOB best bid matches MapLOB after deletes");
        ok &= check_close(ca.price, ma.price, 1e-6, "CircularLOB best ask matches MapLOB after updates");
        ok &= check_cond(cb.quantity == mb.quantity && ca.quantity == ma.quantity,
                         std::format("LOB L1 quantities match exactly (Bid {} vs {}, Ask {} vs {})",
                                     cb.quantity, mb.quantity, ca.quantity, ma.quantity));

        auto m_an = circ_lob.analytics();
        ok &= check_cond(m_an.spread_bps > 0.0 && m_an.micro_price > cb.price && m_an.micro_price < ca.price &&
                         m_an.l1_imbalance >= -1.0 && m_an.l1_imbalance <= 1.0,
                         std::format("LOB microstructure metrics coherent (micro={:.4f}, spread={:.1f}bps)",
                                     m_an.micro_price, m_an.spread_bps));

        // 3. Pre-Trade Institutional Risk Management System (RMS)
        PreTradeRiskConfig rcfg;
        rcfg.max_order_qty = 1000;
        rcfg.max_order_notional = 100000.0;
        rcfg.max_price_collar_pct = 5.0;
        PreTradeRiskEngine rms(rcfg);
        auto r1 = rms.validate_order(OrderSide::Buy, 100.0, 500, 100.0);
        auto r2 = rms.validate_order(OrderSide::Buy, 100.0, 1500, 100.0);
        auto r3 = rms.validate_order(OrderSide::Buy, 112.0, 100, 100.0);
        rms.set_pnl(-30000.0);
        auto r4 = rms.validate_order(OrderSide::Sell, 100.0, 100, 100.0);
        ok &= check_cond(r1.approved && r2.reason == RiskRejectReason::MaxOrderQtyExceeded &&
                         r3.reason == RiskRejectReason::FatFingerCollarExceeded &&
                         r4.reason == RiskRejectReason::KillSwitchEngaged,
                         "Pre-Trade RMS enforces qty limit, fat-finger collar, and daily drawdown kill-switch");

        // 4. Game-Theoretic Nash Quoter (inventory skew + MM session) + Ch 6 CPU Bench
        auto q_long = GameTheoreticQuoter::compute_nash_quotes(100.0, 100.0, 0.0, +15, 0.25);
        auto q_short = GameTheoreticQuoter::compute_nash_quotes(100.0, 100.0, 0.0, -15, 0.25);
        auto mm_sess = GameTheoreticQuoter::run_market_making_session(100.0, 0.25, 150, 77);
        ok &= check_cond(q_long.reservation_price < q_short.reservation_price &&
                         q_long.optimal_bid < q_long.reservation_price && q_long.reservation_price < q_long.optimal_ask &&
                         mm_sess.simulated_fills > 0 && std::isfinite(mm_sess.simulated_mm_pnl),
                         std::format("Ch.7 Nash Quoter skews inventory cleanly (fills={}, PnL={:+.2f})",
                                     mm_sess.simulated_fills, mm_sess.simulated_mm_pnl));

        auto bench = LowLatencyBenchmarkSuite::run_on_machine(4000, 64);
        ok &= check_cond(bench.lob_circular_ms > 0.0 && bench.cache_padded_ms > 0.0 && bench.row_major_tiled_ms > 0.0,
                         std::format("Ch.6 Empirical Benchmarks measured live (LOB {:.2f}x, CachePad {:.2f}x)",
                                     bench.lob_speedup, bench.false_sharing_speedup));
        return ok;
    }

    static bool test_silahian_ml_dpo_ior_and_heston_merton() {
        std::cout << "\n[TEST 17] Silahian Ch.4/7: DPO (PSO & RL), IOR Router, RL/OFI Backtests, Heston & Merton:\n";
        bool ok = true;

        // 1. Dynamic Portfolio Optimization (PSO + Causal RL)
        Eigen::MatrixXd rets(80, 4);
        std::mt19937_64 rng(99ULL);
        std::normal_distribution<double> nd(0.0, 1.0);
        for (int r = 0; r < 80; ++r) {
            rets(r, 0) = 0.0012 + 0.008 * nd(rng); // High Sharpe asset
            rets(r, 1) = 0.0005 + 0.011 * nd(rng);
            rets(r, 2) = 0.0002 + 0.007 * nd(rng);
            rets(r, 3) = -0.0010 + 0.025 * nd(rng); // High vol negative drift
        }
        auto dpo = PortfolioLinearAlgebra::optimize_dynamic_portfolio(rets, 0.03, 0.65, 42);
        ok &= check_close(dpo.pso_weights.sum(), 1.0, 1e-6, "DPO PSO weights satisfy budget sum(w) == 1.0");
        ok &= check_close(dpo.rl_weights.sum(), 1.0, 1e-6, "DPO RL weights satisfy budget sum(w) == 1.0");
        ok &= check_cond(dpo.pso_weights.minCoeff() >= -1e-9 && dpo.rl_weights.minCoeff() >= -1e-9 &&
                         dpo.pso_weights(0) > dpo.pso_weights(3),
                         std::format("DPO PSO & RL favor quality asset over negative-drift high-vol (w0={:.2f} vs w3={:.2f}, Sharpe={:.2f})",
                                     dpo.pso_weights(0), dpo.pso_weights(3), dpo.pso_sharpe));

        // 2. Intelligent Order Router (IOR) RL Execution
        IntelligentOrderRouter ior;
        auto ior_res = ior.simulate_parent_order(100.0, 0.25, 14.0, 10000, 260, 42);
        ok &= check_cond(ior_res.ior_filled_shares > 5000 && std::isfinite(ior_res.ior_vwap) &&
                         ior_res.ior_shortfall_bps < ior_res.naive_shortfall_bps + 2.5,
                         std::format("IOR smart router executed cleanly (Shortfall {:.2f} vs Naive {:.2f} bps)",
                                     ior_res.ior_shortfall_bps, ior_res.naive_shortfall_bps));

        // 3. Causal Backtests: RLDynamicPortfolio & OrderFlowImbalance
        std::vector<double> closes, highs, lows, vols;
        double px = 100.0;
        for (int i = 0; i < 120; ++i) {
            const double cyc = std::sin(i * 0.25);
            px *= std::exp(0.0015 + 0.008 * cyc);
            closes.push_back(px);
            highs.push_back(px * (cyc >= 0.0 ? 1.003 : 1.012));
            lows.push_back(px * (cyc >= 0.0 ? 0.988 : 0.997));
            vols.push_back(1000000.0 + (i % 7) * 150000.0);
        }
        auto bt_rl = BacktestEngine::run_ohlcv(closes, highs, lows, vols, StrategyType::RLDynamicPortfolio);
        auto bt_ofi = BacktestEngine::run_ohlcv(closes, highs, lows, vols, StrategyType::OrderFlowImbalance);
        ok &= check_cond(std::isfinite(bt_rl.total_return_pct) && bt_rl.max_drawdown_pct >= 0.0 &&
                         std::isfinite(bt_ofi.total_return_pct) && bt_ofi.max_drawdown_pct >= 0.0,
                         std::format("Causal RL & OFI Backtests ran cleanly (RL {:+.1f}%, OFI {:+.1f}%)",
                                     bt_rl.total_return_pct, bt_ofi.total_return_pct));

        // 4. Heston Stochastic Volatility (xi->0 -> BS) & Merton Jump-Diffusion (lambda->0 -> BS & fat-tail check)
        const double spot = 100.0, K_atm = 100.0, T = 0.5, r = 0.05, sigma = 0.20;
        BlackScholes bs_atm{K_atm, spot, T, PayoffType::Call, r, 0.0};
        const double bs_ref = bs_atm(sigma);

        OptionInfo opt_atm{std::make_unique<CallPayoff>(K_atm), T};
        MCOptionValuation mc{std::move(opt_atm), 48, sigma, r, 0.0};

        MCOptionValuation::HestonParams hp_zero{sigma * sigma, 2.0, sigma * sigma, 0.0, 0.0};
        double h_lim = mc.calc_heston_mc_price_par(spot, hp_zero, 50000, 123);
        ok &= check_close(h_lim, bs_ref, 0.25, "Heston MC (xi=0) converges to Black-Scholes ATM Call");

        MCOptionValuation::MertonJumpParams jp_zero{0.0, 0.0, 0.0};
        double m_lim = mc.calc_merton_jump_price_par(spot, jp_zero, 50000, 456);
        ok &= check_close(m_lim, bs_ref, 0.25, "Merton Jump MC (lambda=0) converges to Black-Scholes ATM Call");

        // OTM Put crash-tail premium test under Merton Negative Jumps
        BlackScholes bs_otm_put{80.0, spot, T, PayoffType::Put, r, 0.0};
        double bs_put_ref = bs_otm_put(sigma);
        OptionInfo opt_otm_put{std::make_unique<PutPayoff>(80.0), T};
        MCOptionValuation mc_put{std::move(opt_otm_put), 48, sigma, r, 0.0};
        MCOptionValuation::MertonJumpParams jp_crash{1.5, -0.15, 0.20};
        double merton_crash_put = mc_put.calc_merton_jump_price_par(spot, jp_crash, 40000, 789);
        ok &= check_cond(merton_crash_put > bs_put_ref + 0.5,
                         std::format("Merton Jump prices crash fat-tail OTM Put higher than BS ({:.4f} vs BS {:.4f})",
                                     merton_crash_put, bs_put_ref));

        return ok;
    }

    static bool test_silahian_full_app_resilience_dupire_stress() {
        std::cout << "\n[TEST 18] Silahian Full-App Ch.3-7: Dupire Local Vol, Circuit Breaker/RateLimiter/P95, Tail Forecast, Stress & Net Pairs:\n";
        bool ok = true;

        // 1. Ch.7 Dupire Local Volatility (flat surface -> BS; negative skew raises OTM Put)
        const double spot = 100.0, K_atm = 100.0, T = 0.5, r = 0.05, sigma = 0.20;
        BlackScholes bs_atm{K_atm, spot, T, PayoffType::Call, r, 0.0};
        OptionInfo opt_c{std::make_unique<CallPayoff>(K_atm), T};
        MCOptionValuation mc_c{std::move(opt_c), 48, sigma, r, 0.0};
        MCOptionValuation::DupireSurfaceParams dp_flat{sigma, 0.0, 0.0, 0.0};
        double dup_flat = mc_c.calc_dupire_local_vol_price_par(spot, dp_flat, 50000, 321);
        ok &= check_close(dup_flat, bs_atm(sigma), 0.25, "Dupire Local Vol (flat smile/skew) converges to Black-Scholes ATM Call");

        BlackScholes bs_otm_put{85.0, spot, T, PayoffType::Put, r, 0.0};
        OptionInfo opt_p{std::make_unique<PutPayoff>(85.0), T};
        MCOptionValuation mc_p{std::move(opt_p), 48, sigma, r, 0.0};
        MCOptionValuation::DupireSurfaceParams dp_skew{sigma, -0.35, 0.25, 0.04};
        double dup_skew_put = mc_p.calc_dupire_local_vol_price_par(spot, dp_skew, 40000, 654);
        ok &= check_cond(dup_skew_put > bs_otm_put(sigma) + 0.15,
                         std::format("Dupire negative skew raises OTM Put tail premium ({:.4f} vs BS {:.4f})",
                                     dup_skew_put, bs_otm_put(sigma)));

        // 2. Ch.5 TokenBucketRateLimiter + EndpointCircuitBreaker + Latency P50/P95/P99
        TokenBucketRateLimiter limiter(50.0, 2.0);
        ok &= check_cond(limiter.try_acquire() && limiter.try_acquire() && !limiter.try_acquire(),
                         "TokenBucketRateLimiter drains burst tokens accurately and throttles excess");

        EndpointCircuitBreaker cb(3, std::chrono::seconds(1));
        ok &= check_cond(cb.state() == CircuitState::Closed && cb.allow_request(), "Circuit starts Closed");
        cb.record_failure();
        cb.record_failure();
        bool tripped = cb.record_failure();
        ok &= check_cond(tripped && cb.state() == CircuitState::Open && !cb.allow_request(),
                         "EndpointCircuitBreaker trips Open after failure threshold and fails fast");
        cb.reset();
        cb.record_success();
        ok &= check_cond(cb.state() == CircuitState::Closed && cb.consecutive_failures() == 0,
                         "EndpointCircuitBreaker recovers cleanly to Closed");

        DataQualityDashboard::reset();
        DataQualityTelemetry::record_fetch("Yahoo v8 Chart", "ep1", true, std::chrono::milliseconds(20), false, "", 200);
        DataQualityTelemetry::record_fetch("Yahoo v8 Chart", "ep1", true, std::chrono::milliseconds(40), false, "", 200);
        DataQualityTelemetry::record_fetch("Yahoo v8 Chart", "ep1", true, std::chrono::milliseconds(60), false, "", 200);
        DataQualityTelemetry::record_fetch("Yahoo v8 Chart", "ep1", true, std::chrono::milliseconds(300), false, "", 200);
        auto atom = DataQualityTelemetry::atomic_snapshot();
        DataSourceMetrics dm;
        dm.latency_samples_ms = {10, 20, 30, 40, 50, 60, 70, 80, 90, 200};
        ok &= check_cond(atom.total_requests == 4 && atom.successful_requests == 4 &&
                         dm.p50_latency_ms() == 50 && dm.p95_latency_ms() == 200,
                         std::format("Latency SLO & Atomic counters exact (P50={}ms, P95={}ms, total={})",
                                     dm.p50_latency_ms(), dm.p95_latency_ms(), atom.total_requests));
        DataQualityDashboard::reset();

        // 3. Ch.4/7 Asymmetric Tail Forecast Cone & Cornish-Fisher VaR/CVaR
        RealTimeAssetData skew_asset;
        skew_asset.symbol = "SKEW_TEST";
        skew_asset.currency_code = "USD";
        skew_asset.currency_display = "$";
        skew_asset.is_live = true;
        double px = 100.0;
        for (int i = 0; i < 90; ++i) {
            double r_day = (i % 15 == 14) ? -0.055 : 0.0045;
            px *= std::exp(r_day);
            skew_asset.historical_prices.push_back(px);
            skew_asset.historical_highs.push_back(px * 1.006);
            skew_asset.historical_lows.push_back(px * 0.992);
            skew_asset.historical_volumes.push_back(2500000.0);
        }
        skew_asset.current_price = px;
        skew_asset.annualized_volatility = 0.28;
        auto fc_skew = QuantForecastEngine::forecast_for_trading_days(skew_asset, 21);
        ok &= check_cond(fc_skew.downside_tail_mult > 1.0 &&
                         fc_skew.var_95_pct > 2.0 &&
                         fc_skew.cvar_95_pct >= fc_skew.var_95_pct &&
                         fc_skew.regime_confidence >= 0.65 && fc_skew.regime_confidence <= 1.0,
                         std::format("Asymmetric Forecast Cone widens bear tail ({:.3f}x, VaR95=-{:.1f}%, CVaR95=-{:.1f}%)",
                                     fc_skew.downside_tail_mult, fc_skew.var_95_pct, fc_skew.cvar_95_pct));

        // 4. Ch.4 Stress Scenarios (Historical VaR/CVaR, -15% Crash, 2x Liquidity Freeze) & Pairs Net Edge
        std::vector<double> sample_rets;
        for (int i = 0; i < 100; ++i) sample_rets.push_back((i % 10 == 0) ? -0.032 : 0.0025);
        auto sr = DrawdownRiskEngine::run_stress_scenarios(sample_rets, 100000.0, 15.0);
        ok &= check_cond(sr.hist_var_95 > 0.0 && sr.cvar_95 >= sr.hist_var_95 &&
                         std::abs(sr.market_crash_15pct_loss - 15000.0) < 1e-6 &&
                         sr.vol_2x_liquidity_freeze_loss > sr.cvar_95,
                         std::format("Stress scenarios valid (VaR95={:.0f}, CVaR95={:.0f}, Freeze={:.0f})",
                                     sr.hist_var_95, sr.cvar_95, sr.vol_2x_liquidity_freeze_loss));

        std::vector<double> pa, pb;
        for (int i = 0; i < 60; ++i) {
            pb.push_back(100.0 + i * 0.2);
            pa.push_back(2.0 * pb.back() + ((i == 59) ? -14.0 : std::sin(i * 0.5)));
        }
        auto pair_res = PairsTradingEngine::analyze("A_TST", "B_TST", pa, pb);
        ok &= check_cond(std::isfinite(pair_res.net_edge_zscore) &&
                         std::abs(pair_res.net_edge_zscore) <= std::abs(pair_res.current_zscore) + 1e-9 &&
                         !pair_res.execution_urgency.empty(),
                         std::format("Pairs Net-of-Cost Edge computed (Z={:+.2f}, NetZ={:+.2f}, {})",
                                     pair_res.current_zscore, pair_res.net_edge_zscore, pair_res.execution_urgency));

        return ok;
    }

    static bool test_sticky_sentiment_and_event_shock() {
        std::cout << "\n[TEST 19] Sticky 2-Stream Sentiment Memory, sent_strength & Event-Shock Damping:\n";
        bool ok = true;

        const int64_t day0 = 20500LL * 86400LL;
        std::vector<StickyHeadlineObs> scandal_plus_fluff = {
            {day0 + 3600, -0.85, 1.0},
            {day0 + 7200, -0.78, 1.0},
            {day0 + 2 * 86400 + 3600, 0.00, 1.0},
            {day0 + 2 * 86400 + 7200, 0.00, 1.0}
        };
        auto st = NewsSentimentEngine::compute_sticky_sentiment(scandal_plus_fluff, day0 + 2 * 86400 + 10000);
        ok &= check_cond(st.sticky_score <= -0.50 && st.negative_state <= -0.50 && st.is_event_shock &&
                         st.count_log > 1.5 && st.strength <= -0.80 && st.dispersion > 0.35,
                         std::format("Sticky memory preserves t-2 scandal across neutral weekend (sticky={:+.3f}, strength={:+.3f}, disp={:.3f})",
                                     st.sticky_score, st.strength, st.dispersion));

        // Verify event-shock profile meaningfully shifts forecast compared to neutral news
        auto syn = CrossSectionalBacktestEngine::make_synthetic_asset_("NVDA_TST", 120, 777, 0.001, 0.02);
        AssetSentimentProfile prof;
        prof.has_live_news = true;
        prof.sticky_sentiment_score = st.sticky_score;
        prof.sent_count_log = st.count_log;
        prof.sent_strength = st.strength;
        prof.sent_dispersion = st.dispersion;
        prof.is_event_shock = true;
        prof.sentiment_score = st.sticky_score;

        auto fc_neutral = QuantForecastEngine::forecast_for_trading_days(syn, 5, 0.0, false);
        auto fc_shock   = QuantForecastEngine::forecast_for_trading_days(syn, 5, prof.sentiment_score, true, nullptr, "", 0, nullptr, &prof);
        ok &= check_cond(fc_shock.expected_return_pct < fc_neutral.expected_return_pct - 0.35,
                         std::format("Event-shock dampens 5d target ({:+.2f}% vs neutral {:+.2f}%)",
                                     fc_shock.expected_return_pct, fc_neutral.expected_return_pct));

        return ok;
    }

    static bool test_cross_sectional_ic_portfolio_and_mcc() {
        std::cout << "\n[TEST 20] Cross-Sectional Portfolio Engine, Spearman CS-IC, Q-Spread, MCC & ETF Hygiene:\n";
        bool ok = true;

        // 1. Spearman & Classification Metrics (Accuracy, Zero-Rule Lift, MCC, Macro-F1)
        std::vector<double> rx = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<double> ry = {10.0, 20.0, 30.0, 40.0, 50.0};
        double sp = CrossSectionalBacktestEngine::spearman_rank_ic(rx, ry);
        ok &= check_close(sp, 1.0, 1e-9, "Spearman monotone Rank IC == 1.0");

        std::vector<int> yt = {1, 1, 1, 0, -1, -1};
        std::vector<int> yp = {1, 1, 0, 0, -1, -1};
        auto cm = CrossSectionalBacktestEngine::compute_classification_metrics(yt, yp);
        ok &= check_cond(std::abs(cm.accuracy - (5.0 / 6.0)) < 1e-6 &&
                         std::abs(cm.zero_rule - 0.5) < 1e-6 &&
                         std::abs(cm.lift - (5.0 / 6.0 - 0.5)) < 1e-6 &&
                         cm.mcc > 0.70 && cm.f1_macro > 0.75,
                         std::format("Multi-class metrics exact (Acc={:.3f}, Lift={:+.3f}, MCC={:.3f}, F1={:.3f})",
                                     cm.accuracy, cm.lift, cm.mcc, cm.f1_macro));

        // 2. Multi-Asset Universe with injected ETF (must be filtered by ETF hygiene!)
        std::vector<RealTimeAssetData> assets;
        RealTimeAssetData etf = CrossSectionalBacktestEngine::make_synthetic_asset_("SPY", 130, 10, 0.0004, 0.01);
        etf.instrument_type = "ETF";
        assets.push_back(etf);
        for (int i = 0; i < 8; ++i) {
            assets.push_back(CrossSectionalBacktestEngine::make_synthetic_asset_(
                std::format("EQ{}.TST", i + 1), 130, 100ULL + static_cast<uint64_t>(i) * 17ULL,
                0.0003 * (i % 3 == 0 ? 1.5 : 0.8), 0.012 + 0.002 * i));
        }
        CrossSectionalConfig cfg;
        cfg.region = "SYN";
        cfg.horizon_days = 5;
        cfg.rebalance_days = 5;
        cfg.top_n = 3;
        cfg.warmup_bars = 65;
        cfg.monte_carlo_trials = 6;

        auto rep = CrossSectionalBacktestEngine::run_backtest(assets, cfg);
        ok &= check_cond(rep.valid && rep.excluded_etf_count == 1 && rep.universe_size == 8 &&
                         rep.evaluated_days >= 30 && rep.strategies.size() == 9 &&
                         rep.per_stock.size() == 8 && rep.per_fold.size() >= 4 &&
                         !rep.daily_ic_series.empty() && !rep.topn_holdings.empty(),
                         std::format("CS-Backtest valid (N=8 eq, excl={}, days={}, strategies={}, folds={})",
                                     rep.excluded_etf_count, rep.evaluated_days, rep.strategies.size(), rep.per_fold.size()));

        return ok;
    }

    static bool test_fake_alpha_diagnostics_leakage_and_robustness() {
        std::cout << "\n[TEST 21] Fake-Alpha Diagnostics, Look-Ahead Leakage Seal, Timing & Robustness:\n";
        bool ok = true;

        auto syn = CrossSectionalBacktestEngine::make_synthetic_asset_("LEAK_AUD", 120, 505, 0.0004, 0.015);
        auto audit = CrossSectionalBacktestEngine::verify_feature_causality(syn, 5, 14);
        ok &= check_cond(audit.passed && audit.max_feature_diff < 1e-12 && audit.max_forecast_diff < 1e-12,
                         std::format("Zero Look-Ahead Leakage verified ({})", audit.detail));

        auto diag = CrossSectionalBacktestEngine::run_diagnostic_sanity_checks(&syn);
        ok &= check_cond(diag.lookahead_audit.passed && diag.synthetic_overfit_passed && diag.temporal_sentiment_var_passed &&
                         diag.signal_quality.pearson_r > 0.15 && diag.signal_quality.p_value < 0.05,
                         std::format("Pre-Deployment Sanity Suite passed (OverfitAcc={:.1f}%, SentStd={:.3f}, r={:+.3f}, p={:.4f})",
                                     diag.synthetic_overfit_dir_acc * 100.0, diag.temporal_sentiment_std,
                                     diag.signal_quality.pearson_r, diag.signal_quality.p_value));

        // Verify Vol-Bias, High-Vol Pool IC, Timing Alpha & Ex-Top3/Ex-Best5 Robustness compute cleanly
        std::vector<RealTimeAssetData> mix;
        for (int i = 0; i < 6; ++i) {
            mix.push_back(CrossSectionalBacktestEngine::make_synthetic_asset_(
                std::format("FA{}.TST", i + 1), 120, 900ULL + static_cast<uint64_t>(i) * 31ULL,
                0.0002, 0.010 + 0.003 * i));
        }
        CrossSectionalConfig cfg;
        cfg.region = "SYN_FA";
        cfg.horizon_days = 5;
        cfg.top_n = 2;
        cfg.monte_carlo_trials = 6;
        auto rep = CrossSectionalBacktestEngine::run_backtest(mix, cfg);
        ok &= check_cond(rep.valid &&
                         std::isfinite(rep.skill.excess_sharpe_vs_vol) &&
                         std::isfinite(rep.skill.excess_sharpe_vs_shuffled) &&
                         std::isfinite(rep.skill.high_vol_pool_ic) &&
                         std::isfinite(rep.skill.timing_alpha_pct) &&
                         std::isfinite(rep.skill.sharpe_ex_top3_stocks) &&
                         std::isfinite(rep.skill.sharpe_ex_best5_days) &&
                         !rep.skill.verdict.empty(),
                         std::format("Fake-Alpha & Robustness computed (ΔSharpe_Vol={:+.2f}, Timing={:+.1f}%, Verdict='{}')",
                                     rep.skill.excess_sharpe_vs_vol, rep.skill.timing_alpha_pct, rep.skill.verdict));

        return ok;
    }

    static bool test_dashboard_report_export_integrity() {
        std::cout << "\n[TEST 22] Dashboard Standard CSV/JSON + OHLCV Export Contract Verification:\n";
        bool ok = true;
        namespace fs = std::filesystem;

        std::vector<RealTimeAssetData> assets;
        for (int i = 0; i < 6; ++i) {
            assets.push_back(CrossSectionalBacktestEngine::make_synthetic_asset_(
                std::format("RPT{}.TST", i + 1), 115, 300ULL + static_cast<uint64_t>(i) * 19ULL,
                0.0005, 0.014));
        }
        CrossSectionalConfig cfg;
        cfg.region = "TEST";
        cfg.experiment_name = "_unit_test_dashboard_export";
        cfg.horizon_days = 5;
        cfg.top_n = 2;
        cfg.monte_carlo_trials = 4;
        cfg.reports_root = "reports";

        auto rep = CrossSectionalBacktestEngine::run_backtest(assets, cfg);
        std::string dir = CrossSectionalBacktestEngine::export_report(rep, &assets);
        fs::path root(dir);

        const std::vector<std::string> expected_files = {
            "config.json", "summary.csv", "per_fold_results.csv",
            "per_stock_results.csv", "cross_sectional_ic.csv",
            "backtest_results.csv", "QuantForecast Top-2_trades.csv", "QuantForecast Top-2_holdings.csv"
        };
        bool all_files = !dir.empty() && fs::exists(root);
        for (const auto& fn : expected_files) {
            fs::path p = root / fn;
            if (!fs::exists(p) || fs::file_size(p) < 20) all_files = false;
        }
        bool ohlcv_ok = fs::exists(fs::path("reports") / "ohlcv" / "RPT1.TST.json");
        ok &= check_cond(all_files && ohlcv_ok,
                         std::format("All 8 CSV/JSON report artifacts & local OHLCV JSON exported ({})", dir));

        // Clean up temporary test artifacts cleanly
        std::error_code ec;
        fs::remove_all(root, ec);
        for (int i = 0; i < 6; ++i) {
            fs::remove(fs::path("reports") / "ohlcv" / std::format("RPT{}.TST.json", i + 1), ec);
        }
        return ok;
    }

    static bool test_native_unicode_pdf_report_engine() {
        std::cout << "\n[TEST 23] Pure C++20 Unicode PDF Writer & 3-Market + World Macro 1-Month Report:\n";
        bool ok = true;
        namespace fs = std::filesystem;

        // Build clean deterministic GlobalMacroReportData spanning US, XETRA, BIST & Macro Drivers
        GlobalMacroReportData rep;
        rep.generated_at = "2026-11-16 12:30";
        rep.yield_curve.three_month_yield = 0.0412;
        rep.yield_curve.five_year_yield = 0.0465;
        rep.yield_curve.ten_year_yield = 0.0515;
        rep.yield_curve.thirty_year_yield = 0.0540;
        rep.yield_curve.is_live = true;
        rep.yield_spread_bps = 103.0;
        rep.global_macro_score = 0.14;
        rep.global_regime_title = "DENGELİ / MAKRO SEÇİCİ ROTASYON";
        rep.global_1m_outlook = "Önümüzdeki 1 ay ABD 10Y (%5.15), VIX (19.9), Brent Petrol ve USD/TRY dinamikleri altında seçici bilanço kalitesi ön planda kalacaktır.";

        auto fill_region = [](RegionDeepReport& reg, const std::string& code, const std::string& title,
                              const std::string& sym, double spot, double r20, double pe, double ev) {
            reg.region_code = code;
            reg.region_title = title;
            reg.benchmark_tech.symbol = sym;
            reg.benchmark_tech.display_name = sym;
            reg.benchmark_tech.unit = (code == "US") ? "$" : "pt";
            reg.benchmark_tech.spot = spot;
            reg.benchmark_tech.ret_20d_pct = r20;
            reg.benchmark_tech.rsi_14 = 54.0;
            reg.benchmark_tech.sma_20 = spot * 0.99;
            reg.benchmark_tech.sma_50 = spot * 0.96;
            reg.benchmark_tech.trend_regime = "Pozitif Trend";
            for (int k = 0; k < 60; ++k) {
                reg.benchmark_tech.last_60_prices.push_back(spot * (0.94 + 0.06 * (static_cast<double>(k) / 59.0)));
            }
            reg.benchmark_forecast_1m.symbol = sym;
            reg.benchmark_forecast_1m.spot_price = spot;
            reg.benchmark_forecast_1m.target_price = spot * 1.022;
            reg.benchmark_forecast_1m.bull_target = spot * 1.075;
            reg.benchmark_forecast_1m.bear_support = spot * 0.965;
            reg.benchmark_forecast_1m.expected_return_pct = 2.2;
            reg.benchmark_forecast_1m.bull_return_pct = 7.5;
            reg.benchmark_forecast_1m.bear_return_pct = -3.5;
            reg.benchmark_forecast_1m.direction = ForecastDirection::Bullish;
            reg.benchmark_forecast_1m.direction_label = "YUKARI";
            reg.benchmark_forecast_1m.regime_confidence = 0.72;
            reg.benchmark_forecast_1m.var_95_pct = -6.8;
            reg.benchmark_forecast_1m.cvar_95_pct = -8.4;
            reg.sub_indices_tech = {reg.benchmark_tech};
            reg.sub_indices_fc = {reg.benchmark_forecast_1m};
            reg.macro_sent.overall_sentiment_score = 0.18;
            reg.macro_sent.macro_synthesis = "Güçlü şirket nakit akışları ve dengeli kur görünümü destekleyici.";
            reg.fund_agg.median_pe = pe;
            reg.fund_agg.median_ev_ebitda = ev;
            reg.fund_agg.median_pb = 1.9;
            reg.fund_agg.mean_ebitda_margin_pct = 24.0;
            reg.fund_agg.mean_roe_pct = 18.5;
            reg.fund_agg.grade_a_count = 6;
            reg.fund_agg.grade_b_count = 4;
            reg.fund_agg.pct_above_vwap = 65.0;
            reg.fund_agg.pct_rsi_bull = 60.0;
            AlphaCandidate c;
            c.symbol = sym;
            c.market = code;
            c.category = AlphaCategory::VolatilitySqueeze;
            c.category_label = "VOLATİLİTE SIKIŞMASI";
            c.current_price = spot;
            c.return_30d_pct = r20;
            c.net_asymmetry_ratio = 2.85;
            c.fundamentals.is_available = true;
            c.fundamentals.has_ebitda = true;
            c.fundamentals.ev_to_ebitda = ev;
            c.fundamentals.quality_grade = "A";
            c.has_live_news = true;
            c.sticky_sentiment_score = 0.35;
            c.predictability_ic = 0.42;
            c.predictability_hit_rate = 0.62;
            reg.candidates.push_back(c);
        };

        fill_region(rep.us, "US", "ABD Borsaları (Wall Street)", "SPY", 754.0, 2.4, 24.5, 16.8);
        fill_region(rep.xetra, "XETRA", "Almanya Borsası (XETRA)", "^GDAXI", 26716.0, 1.8, 15.2, 9.6);
        fill_region(rep.bist, "BIST", "Türkiye Borsası (BIST 100)", "XU100.IS", 11909.0, 3.1, 7.8, 5.9);
        rep.bist_usd_ret20d_pct = 1.9;
        rep.bist_usd_ret60d_pct = 4.2;

        AlphaCandidate shock_c;
        shock_c.symbol = "TESTSHOCK.IS";
        shock_c.market = "BIST";
        shock_c.category = AlphaCategory::CrowdedTradeWarning;
        shock_c.category_label = "OLAY ŞOKU VETO";
        shock_c.has_live_news = true;
        shock_c.is_event_shock = true;
        shock_c.sticky_sentiment_score = -0.48;
        shock_c.sentiment_score = -0.53;
        shock_c.quant_diagnosis = "MSCI endeksinden çıkarılma şoku tespit edildi.";
        rep.bist.candidates.push_back(shock_c);

        MacroDriverEntry dr;
        dr.symbol = "BZ=F";
        dr.label = "Brent Ham Petrol";
        dr.unit = "$/bbl";
        dr.role_note = "Küresel enflasyon ve enerji maliyeti";
        dr.tech.spot = 95.30;
        dr.tech.ret_5d_pct = 1.1;
        dr.tech.ret_20d_pct = 4.2;
        dr.tech.ann_vol_pct = 31.0;
        dr.forecast_1m.target_price = 97.4;
        dr.forecast_1m.bear_support = 89.2;
        dr.forecast_1m.bull_target = 105.8;
        dr.forecast_1m.expected_return_pct = 2.2;
        dr.forecast_1m.direction = ForecastDirection::Bullish;
        rep.drivers.push_back(dr);

        rep.corr_row_labels = {"ABD SPY", "Almanya DAX", "Türkiye BIST"};
        rep.corr_col_labels = {"ABD 10Y", "VIX", "Brent"};
        rep.corr_matrix = {{-0.32, -0.78, 0.12}, {-0.24, -0.62, -0.18}, {-0.15, -0.35, -0.22}};
        rep.corr_samples = {{120, 120, 120}, {120, 120, 120}, {120, 120, 120}};
        rep.correlation_summary = "Tarih-hizalı test korelasyon özeti.";

        const std::string pdf_path = "reports/_unit_test_global_1m.pdf";
        bool saved = GlobalMacroReportEngine::generate_pdf_report(rep, pdf_path);
        std::string bytes;
        if (saved && fs::exists(pdf_path)) {
            std::ifstream ifs(pdf_path, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        }
        bool struct_ok = saved && bytes.starts_with("%PDF-1.4") &&
                         bytes.find("/Count 4") != std::string::npos &&
                         bytes.find("xref") != std::string::npos &&
                         bytes.find("%%EOF") != std::string::npos &&
                         bytes.size() > 15000;
        ok &= check_cond(struct_ok,
                         std::format("4-page Turkish Unicode + Vector Chart PDF written ({} bytes)", bytes.size()));
        std::error_code ec;
        fs::remove(pdf_path, ec);
        return ok;
    }
};

} // namespace quant
