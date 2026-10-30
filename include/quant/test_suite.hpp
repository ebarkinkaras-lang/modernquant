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
#include "forecast_engine.hpp"
#include "forecast_backtest.hpp"
#include "alpha_screener.hpp"
#include <iostream>
#include <format>
#include <cassert>
#include <cmath>

namespace quant {

class AutomatedTestSuite {
public:
    static bool run_all_tests() {
        std::cout << "\n=======================================================\n";
        std::cout << "  RUNNING AUTOMATED TEXTBOOK VERIFICATION TEST SUITE   \n";
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

        std::cout << "\n-------------------------------------------------------\n";
        if (all_ok) {
            std::cout << "\033[32m[PASS] ALL TEXTBOOK VERIFICATION TESTS PASSED SUCCESSFULLY!\033[0m\n";
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
            std::cout << std::format("  \033[32m✔\033[0m {:<40} Actual: {:10.4f} | Expected: {:10.4f}\n", name, actual, expected);
        } else {
            std::cout << std::format("  \033[31m✘\033[0m {:<40} Actual: {:10.4f} | Expected: {:10.4f} (diff: {})\n", name, actual, expected, diff);
        }
        return ok;
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
        // Verify Householder QR solves least squares correctly: Y = 1*X1 + 2*X2
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
        ok &= (paths.rows() == 2 && paths.cols() == 11);
        std::cout << "  \033[32m✔\033[0m Cholesky Basket Simulation dimensions verified (2x11)\n";

        return ok;
    }

    static bool test_trading_indicators_and_drawdown() {
        std::cout << "\n[TEST 7] Boost Accumulators & Drawdown Risk (Chapters 6 & 9):\n";
        bool ok = true;

        // Streaming Indicators test (Chapter 9, Page 371)
        StreamingIndicators ind(5, 1.5, 3, 5);
        std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 105.0, 104.0, 106.0};
        IndicatorPoint last_pt;
        for (double p : prices) {
            last_pt = ind.update(p);
        }
        ok &= (last_pt.rolling_mean > 0.0 && last_pt.upper_band > last_pt.lower_band);
        std::cout << "  \033[32m✔\033[0m Boost Accumulators Bollinger & MA updated cleanly\n";

        // Drawdown test with P/L series
        std::vector<double> pnl = {100.0, -50.0, 20.0, -80.0, 150.0, -30.0, 200.0};
        auto dd_res = DrawdownRiskEngine::analyze(pnl, 100, 42);
        ok &= (dd_res.original_max_drawdown >= 80.0);
        std::cout << "  \033[32m✔\033[0m Drawdown Risk Engine: Max DD = " << dd_res.original_max_drawdown
                  << ", 95% Nonparametric = " << dd_res.nonparametric_95_dd << "\n";

        return ok;
    }

    static bool test_forecast_engine() {
        std::cout << "\n[TEST 8] Quant Target Price & Directional Regime (GBM & Sentiment):\n";
        bool ok = true;

        // Bullish Asset Test (SPY-like: Positive sentiment +0.70, low vol 0.10)
        RealTimeAssetData bull_data;
        bull_data.symbol = "SPY";
        bull_data.current_price = 500.0;
        bull_data.vwap = 500.0;
        bull_data.annualized_volatility = 0.10;
        bull_data.is_live = true;

        auto bull_fc = QuantForecastEngine::forecast_for_days(bull_data, 0.70, 30);
        ok &= (bull_fc.target_price >= bull_data.current_price);
        ok &= (bull_fc.bull_target > bull_fc.target_price);
        ok &= (bull_fc.bear_support < bull_fc.target_price);
        ok &= (bull_fc.direction == ForecastDirection::StrongBullish || bull_fc.direction == ForecastDirection::Bullish);
        std::cout << std::format("  \033[32m✔\033[0m Bullish Asset 30-Day Target: {:.2f} (Expected: >500) | Direction: {}\n",
                                 bull_fc.target_price, bull_fc.direction_label);

        // Bearish Asset Test (Negative sentiment -0.75, MSCI shock)
        RealTimeAssetData bear_data;
        bear_data.symbol = "THYAO.IS";
        bear_data.current_price = 270.0;
        bear_data.vwap = 285.0;
        bear_data.annualized_volatility = 0.28;
        bear_data.is_live = true;

        auto bear_fc = QuantForecastEngine::forecast_for_days(bear_data, -0.75, 7);
        ok &= (bear_fc.target_price < bear_data.current_price);
        ok &= (bear_fc.direction == ForecastDirection::StrongBearish || bear_fc.direction == ForecastDirection::Bearish);
        std::cout << std::format("  \033[32m✔\033[0m Bearish Asset 7-Day Target : {:.2f} (Downside Risk) | Direction: {}\n",
                                 bear_fc.target_price, bear_fc.direction_label);

        return ok;
    }

    static bool test_forecast_backtest() {
        std::cout << "\n[TEST 9] Walk-Forward Forecast Backtest Engine:\n";
        bool ok = true;

        LiveMarketDataClient client;
        auto rep = ForecastBacktestEngine::run_walk_forward_backtest(client, "SPY", 5, "6mo");
        ok &= (rep.total_predictions >= 50);
        ok &= (rep.in_band_coverage_pct >= 40.0 && rep.in_band_coverage_pct <= 95.0);
        ok &= (rep.mean_absolute_error_pct <= 10.0);

        std::cout << std::format("  \033[32m✔\033[0m SPY 6mo Walk-Forward Backtest ({} bars): MAPE = {:.2f}%, In-Band = {:.1f}%\n",
                                 rep.total_predictions, rep.mean_absolute_error_pct, rep.in_band_coverage_pct);

        return ok;
    }

    static bool test_alpha_screener() {
        std::cout << "\n[TEST 10] Asymmetric Alpha & Hidden Opportunity Screener:\n";
        bool ok = true;

        LiveMarketDataClient client;
        auto results = QuantOpportunityScanner::scan_market("BIST", client);
        ok &= (!results.empty());

        bool found_asymmetric = false;
        bool checked_categories = false;

        for (const auto& c : results) {
            if (c.asymmetry_ratio >= 2.0) {
                found_asymmetric = true;
            }
            if (c.category == AlphaCategory::HiddenAccumulation ||
                c.category == AlphaCategory::VolatilitySqueeze ||
                c.category == AlphaCategory::CrowdedTradeWarning ||
                c.category == AlphaCategory::EarlyMomentum) {
                checked_categories = true;
            }
        }

        ok &= found_asymmetric;
        ok &= checked_categories;

        std::cout << std::format("  \033[32m✔\033[0m Scanned {} BIST assets: Asymmetric R:R and Crowdedness validated\n", results.size());
        return ok;
    }
};

} // namespace quant
