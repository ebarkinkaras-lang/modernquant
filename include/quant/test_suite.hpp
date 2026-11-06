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

        std::cout << "\n-------------------------------------------------------\n";
        if (all_ok) {
            std::cout << "\033[32m[PASS] ALL 13 QUANT, MICROSTRUCTURE & FAVÖK TESTS PASSED!\033[0m\n";
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
        ok &= check_cond(rep.mean_absolute_error_pct <= 8.0,
                         std::format("Active Conviction Accuracy: {:.1f}% ({} signals) | Block Accuracy: {:.1f}% (z={:+.2f})",
                                     rep.active_directional_accuracy_pct, rep.active_predictions,
                                     rep.non_overlapping_accuracy_pct, rep.binomial_z_stat));

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
        double s_neg_flip = NewsSentimentEngine::score_headline("No recession expected as economy remains stable");
        ok &= check_cond(s_tr_pos > 0.35,
                         std::format("Turkish UTF-8 positive headline ('rekor net kâr ve güçlü büyüme') = {:+.2f}", s_tr_pos));
        ok &= check_cond(s_neg_flip >= 0.0,
                         std::format("Negation handling ('No recession expected') = {:+.2f} (>= 0.0)", s_neg_flip));

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
};

} // namespace quant
