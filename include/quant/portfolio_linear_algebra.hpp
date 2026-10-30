#pragma once

#include <Eigen/Dense>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace quant {

// Chapter 8: Linear Algebra with Eigen for Financial Portfolio & Risk Modeling
struct PCAResult {
    Eigen::VectorXd sorted_eigenvalues;
    Eigen::VectorXd explained_variance_ratio;
    double level_shift_weight{0.0};   // 1st component
    double slope_tilt_weight{0.0};    // 2nd component
    double curvature_weight{0.0};     // 3rd component
};

class PortfolioLinearAlgebra {
public:
    // Portfolio Volatility: sigma_p = sqrt(w^T * Sigma * w)
    // where Sigma = D(vol) * Corr * D(vol) (Page 294-295)
    static double calculate_portfolio_volatility(const Eigen::MatrixXd& corr_mtx,
                                                 const Eigen::VectorXd& vols,
                                                 const Eigen::VectorXd& weights) {
        Eigen::MatrixXd cov_mtx = vols.asDiagonal() * corr_mtx * vols.asDiagonal();
        double variance = (weights.transpose() * cov_mtx * weights).value();
        return std::sqrt(variance);
    }

    // Fund Tracking / Sector Attribution via Multiple Linear Regression (Page 303-305)
    // Solves Y = X * beta using Compact Householder QR Decomposition
    static Eigen::VectorXd solve_factor_regression_qr(const Eigen::MatrixXd& X,
                                                      const Eigen::VectorXd& Y) {
        return X.householderQr().solve(Y);
    }

    // Correlated Random Asset Paths via Cholesky Decomposition for Basket Options (Page 306-310)
    // Sigma = L * L^T,  W = L * Z
    static Eigen::MatrixXd simulate_correlated_basket_paths(
        const Eigen::MatrixXd& cov_basket,
        const Eigen::VectorXd& spot_prices,
        double rf_rate,
        double time_to_maturity,
        unsigned num_time_steps,
        unsigned seed) {
        
        long num_assets = cov_basket.rows();
        double dt = time_to_maturity / num_time_steps;

        // Cholesky decomposition to get lower triangular matrix L
        Eigen::LLT<Eigen::MatrixXd> chol{cov_basket};
        Eigen::MatrixXd chol_mtx = chol.matrixL();

        // Matrix to hold paths: rows = assets, columns = time steps (0 to num_time_steps)
        Eigen::MatrixXd price_paths(num_assets, num_time_steps + 1);
        price_paths.col(0) = spot_prices;

        std::mt19937_64 mt{seed};
        std::normal_distribution<> nd;

        for (unsigned step = 1; step <= num_time_steps; ++step) {
            // Draw uncorrelated standard normals
            Eigen::VectorXd Z(num_assets);
            for (long i = 0; i < num_assets; ++i) {
                Z(i) = nd(mt);
            }

            // Transform into correlated normals W = L * Z
            Eigen::VectorXd W = chol_mtx * Z;

            // Update each asset price
            for (long i = 0; i < num_assets; ++i) {
                double vol = std::sqrt(cov_basket(i, i));
                double prev_price = price_paths(i, step - 1);
                double exp_arg1 = (rf_rate - 0.5 * vol * vol) * dt;
                double exp_arg2 = W(i) * std::sqrt(dt);
                price_paths(i, step) = prev_price * std::exp(exp_arg1 + exp_arg2);
            }
        }

        return price_paths;
    }

    // Yield Curve Dynamics and Principal Component Analysis (PCA) (Page 310-313)
    // Analyzes the sources and magnitudes of yield curve shifts
    static PCAResult analyze_yield_curve_pca(const Eigen::MatrixXd& cov_yields) {
        // Compute eigenvalues of real symmetric covariance matrix
        Eigen::VectorXd eigenvalues = cov_yields.selfadjointView<Eigen::Upper>().eigenvalues().real();

        // Sort eigenvalues descending
        std::vector<double> ev_vec(eigenvalues.data(), eigenvalues.data() + eigenvalues.size());
        std::ranges::sort(ev_vec, std::greater<double>{});

        Eigen::VectorXd sorted_ev(ev_vec.size());
        for (size_t i = 0; i < ev_vec.size(); ++i) {
            sorted_ev(i) = ev_vec[i];
        }

        double total_variance = sorted_ev.sum();
        Eigen::VectorXd explained_ratio = sorted_ev / (total_variance > 1e-12 ? total_variance : 1.0);

        PCAResult result;
        result.sorted_eigenvalues = sorted_ev;
        result.explained_variance_ratio = explained_ratio;

        if (explained_ratio.size() >= 3) {
            result.level_shift_weight = explained_ratio(0);
            result.slope_tilt_weight  = explained_ratio(1);
            result.curvature_weight   = explained_ratio(2);
        }

        return result;
    }
};

} // namespace quant
