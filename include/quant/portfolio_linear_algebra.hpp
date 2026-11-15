#pragma once

#include <Eigen/Dense>
#include <vector>
#include <random>
#include <cmath>
#include <numeric>
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

    // =========================================================================
    // Ariel Silahian (Packt, 2024) Ch 4 - Dynamic Portfolio Optimization (DPO)
    //   1. Particle Swarm Optimization (PSO) on long-only constrained simplex
    //   2. Causal RL Q-Learning Rebalancer with Experience Replay
    // =========================================================================
    struct DPOPortfolioResult {
        Eigen::VectorXd pso_weights;
        double pso_return_ann{0.0};
        double pso_vol_ann{0.0};
        double pso_sharpe{0.0};

        Eigen::VectorXd rl_weights;
        double rl_return_ann{0.0};
        double rl_vol_ann{0.0};
        double rl_sharpe{0.0};
        int selected_regime_action{0};
    };

    static DPOPortfolioResult optimize_dynamic_portfolio(const Eigen::MatrixXd& daily_returns,
                                                         double rf_rate = 0.04,
                                                         double max_single_weight = 0.65,
                                                         unsigned seed = 42) {
        DPOPortfolioResult out;
        const long n_obs = daily_returns.rows();
        const long n_assets = daily_returns.cols();
        if (n_assets <= 0) return out;

        out.pso_weights = Eigen::VectorXd::Constant(n_assets, 1.0 / static_cast<double>(n_assets));
        out.rl_weights = out.pso_weights;
        if (n_obs < 15) return out;

        // 1. Estimate empirical annualized drift and covariance
        Eigen::RowVectorXd mean_daily = daily_returns.colwise().mean();
        Eigen::MatrixXd centered = daily_returns.rowwise() - mean_daily;
        Eigen::MatrixXd cov_ann = (centered.transpose() * centered) / static_cast<double>(std::max<long>(1, n_obs - 1)) * 252.0;
        for (long i = 0; i < n_assets; ++i) cov_ann(i, i) = std::max(cov_ann(i, i), 1e-6);
        Eigen::VectorXd mu_ann = mean_daily.transpose() * 252.0;

        auto project_simplex_box = [&](Eigen::VectorXd& w) {
            const double cap = std::clamp(max_single_weight, 1.0 / static_cast<double>(n_assets), 1.0);
            for (long i = 0; i < n_assets; ++i) w(i) = std::max(0.0, w(i));
            double s = w.sum();
            if (s <= 1e-12) {
                w.setConstant(1.0 / static_cast<double>(n_assets));
                return;
            }
            w /= s;
            for (int iter = 0; iter < 6; ++iter) {
                double excess = 0.0;
                double free_sum = 0.0;
                for (long i = 0; i < n_assets; ++i) {
                    if (w(i) > cap) {
                        excess += (w(i) - cap);
                        w(i) = cap;
                    } else if (w(i) < cap - 1e-12) {
                        free_sum += w(i);
                    }
                }
                if (excess <= 1e-12) break;
                if (free_sum > 1e-12) {
                    for (long i = 0; i < n_assets; ++i) {
                        if (w(i) < cap - 1e-12) w(i) += excess * (w(i) / free_sum);
                    }
                } else {
                    break;
                }
            }
            double final_sum = w.sum();
            if (final_sum > 1e-12) w /= final_sum;
        };

        auto eval_sharpe = [&](const Eigen::VectorXd& w) -> double {
            double r = w.dot(mu_ann);
            double var = (w.transpose() * cov_ann * w).value();
            double vol = std::sqrt(std::max(1e-8, var));
            return (r - rf_rate) / vol;
        };

        // 2. Particle Swarm Optimization (PSO) over Sharpe Objective
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        constexpr int kSwarm = 32;
        constexpr int kIters = 55;
        std::vector<Eigen::VectorXd> pos(kSwarm, Eigen::VectorXd(n_assets));
        std::vector<Eigen::VectorXd> vel(kSwarm, Eigen::VectorXd::Zero(n_assets));
        std::vector<Eigen::VectorXd> pbest(kSwarm, Eigen::VectorXd(n_assets));
        std::vector<double> pbest_score(kSwarm, -1e9);

        Eigen::VectorXd gbest = out.pso_weights;
        double gbest_score = eval_sharpe(gbest);

        for (int p = 0; p < kSwarm; ++p) {
            for (long i = 0; i < n_assets; ++i) pos[p](i) = (p == 0) ? 1.0 : u01(rng);
            project_simplex_box(pos[p]);
            pbest[p] = pos[p];
            pbest_score[p] = eval_sharpe(pos[p]);
            if (pbest_score[p] > gbest_score) {
                gbest_score = pbest_score[p];
                gbest = pos[p];
            }
        }

        for (int it = 0; it < kIters; ++it) {
            const double w_in = 0.72 - 0.32 * (static_cast<double>(it) / static_cast<double>(kIters));
            for (int p = 0; p < kSwarm; ++p) {
                for (long i = 0; i < n_assets; ++i) {
                    const double r1 = u01(rng), r2 = u01(rng);
                    vel[p](i) = w_in * vel[p](i)
                              + 1.45 * r1 * (pbest[p](i) - pos[p](i))
                              + 1.65 * r2 * (gbest(i) - pos[p](i));
                    pos[p](i) += vel[p](i);
                }
                project_simplex_box(pos[p]);
                double sc = eval_sharpe(pos[p]);
                if (sc > pbest_score[p]) {
                    pbest_score[p] = sc;
                    pbest[p] = pos[p];
                    if (sc > gbest_score) {
                        gbest_score = sc;
                        gbest = pos[p];
                    }
                }
            }
        }

        out.pso_weights = gbest;
        out.pso_return_ann = gbest.dot(mu_ann);
        out.pso_vol_ann = std::sqrt(std::max(1e-8, (gbest.transpose() * cov_ann * gbest).value()));
        out.pso_sharpe = (out.pso_return_ann - rf_rate) / out.pso_vol_ann;

        // 3. Causal RL Q-Learning Dynamic Allocator with Experience Replay (Ch 4)
        // Actions: 0 = Equal-Weight, 1 = Min-Variance / Defensive, 2 = Momentum Tilt, 3 = PSO Core Mix
        constexpr int kFeat = 4; // [bias, mean_20d_ret_z, avg_20d_vol_z, dispersion_z]
        constexpr int kActs = 4;
        struct ReplayExp {
            Eigen::Vector4d state;
            int action = 0;
            double reward = 0.0;
            Eigen::Vector4d next_state;
        };

        Eigen::Matrix<double, kActs, kFeat> q_w = Eigen::Matrix<double, kActs, kFeat>::Zero();
        std::vector<ReplayExp> replay;
        replay.reserve(256);

        auto build_action_weights = [&](long t, int action) -> Eigen::VectorXd {
            Eigen::VectorXd w = Eigen::VectorXd::Zero(n_assets);
            const long win = std::min<long>(20, t);
            Eigen::VectorXd mom = daily_returns.block(t - win, 0, win, n_assets).colwise().mean().transpose();
            Eigen::VectorXd vols(n_assets);
            for (long i = 0; i < n_assets; ++i) {
                double m = mom(i);
                double sq = 0.0;
                for (long k = t - win; k < t; ++k) {
                    double d = daily_returns(k, i) - m;
                    sq += d * d;
                }
                vols(i) = std::sqrt(std::max(1e-8, sq / static_cast<double>(std::max<long>(1, win - 1))));
            }
            if (action == 0) {
                w.setConstant(1.0 / static_cast<double>(n_assets));
            } else if (action == 1) {
                // Inverse-volatility defensive weighting
                for (long i = 0; i < n_assets; ++i) w(i) = 1.0 / vols(i);
            } else if (action == 2) {
                // Risk-adjusted momentum ranking softmax
                double max_score = -1e9;
                Eigen::VectorXd sc(n_assets);
                for (long i = 0; i < n_assets; ++i) {
                    sc(i) = std::clamp(mom(i) / vols(i) * 4.0, -3.0, 3.0);
                    max_score = std::max(max_score, sc(i));
                }
                for (long i = 0; i < n_assets; ++i) w(i) = std::exp(sc(i) - max_score);
            } else {
                // 50% Equal Weight + 50% Inverse-Vol Core
                Eigen::VectorXd iv(n_assets);
                for (long i = 0; i < n_assets; ++i) iv(i) = 1.0 / vols(i);
                project_simplex_box(iv);
                w = 0.5 * iv + 0.5 * Eigen::VectorXd::Constant(n_assets, 1.0 / static_cast<double>(n_assets));
            }
            project_simplex_box(w);
            return w;
        };

        auto make_state = [&](long t) -> Eigen::Vector4d {
            const long win = std::min<long>(15, t);
            Eigen::MatrixXd blk = daily_returns.block(t - win, 0, win, n_assets);
            Eigen::RowVectorXd col_m = blk.colwise().mean();
            double avg_m = col_m.mean() * 252.0;
            double avg_v = 0.0;
            for (long i = 0; i < n_assets; ++i) {
                double sq = 0.0;
                for (long r = 0; r < win; ++r) {
                    double d = blk(r, i) - col_m(i);
                    sq += d * d;
                }
                avg_v += std::sqrt(std::max(1e-8, sq / static_cast<double>(std::max<long>(1, win - 1)))) * std::sqrt(252.0);
            }
            avg_v /= static_cast<double>(n_assets);
            double disp = std::sqrt((col_m.array() - col_m.mean()).square().mean()) * std::sqrt(252.0);
            return Eigen::Vector4d{
                1.0,
                std::clamp(avg_m * 2.0, -1.5, 1.5),
                std::clamp((avg_v - 0.18) * 3.0, -1.5, 1.5),
                std::clamp(disp * 4.0, 0.0, 2.0)
            };
        };

        double eps = 0.35;
        const double lr = 0.03;
        const double disc = 0.85;
        Eigen::VectorXd prev_w = Eigen::VectorXd::Constant(n_assets, 1.0 / static_cast<double>(n_assets));
        std::vector<double> rl_daily_rets;
        rl_daily_rets.reserve(static_cast<std::size_t>(n_obs));

        for (long t = 15; t < n_obs; ++t) {
            const Eigen::Vector4d st = make_state(t);
            Eigen::Matrix<double, kActs, 1> qvals = q_w * st;
            int act = 0;
            if (u01(rng) < eps) {
                act = static_cast<int>(rng() % kActs);
            } else {
                qvals.maxCoeff(&act);
            }
            Eigen::VectorXd w_t = build_action_weights(t, act); // strictly causal (<= t-1)
            const double turnover = (w_t - prev_w).cwiseAbs().sum();
            const double gross_r = daily_returns.row(t).dot(w_t);
            const double net_r = gross_r - turnover * 0.0005; // 5 bps one-way turnover cost
            rl_daily_rets.push_back(net_r);
            prev_w = w_t;

            // Risk-adjusted daily reward (return minus quadratic variance penalty)
            const double rew = net_r * 100.0 - 0.25 * (net_r * 100.0) * (net_r * 100.0);
            if (t + 1 < n_obs) {
                replay.push_back(ReplayExp{st, act, rew, make_state(t + 1)});
                if (replay.size() > 200) replay.erase(replay.begin());

                // Experience Replay mini-batch Q-learning step (Ch 4)
                const std::size_t mb = std::min<std::size_t>(12, replay.size());
                for (std::size_t k = 0; k < mb; ++k) {
                    const auto& ex = replay[static_cast<std::size_t>(rng() % replay.size())];
                    const double q_pred = q_w.row(ex.action).dot(ex.state);
                    const double q_next = (q_w * ex.next_state).maxCoeff();
                    const double err = std::clamp((ex.reward + disc * q_next) - q_pred, -5.0, 5.0);
                    q_w.row(ex.action) += lr * err * ex.state.transpose();
                }
            }
            eps = std::max(0.05, eps * 0.975);
        }

        // Evaluate final greedy RL policy for next rebalancing step
        const Eigen::Vector4d cur_state = make_state(n_obs);
        Eigen::Matrix<double, kActs, 1> cur_q = q_w * cur_state;
        int best_act = 0;
        cur_q.maxCoeff(&best_act);
        out.selected_regime_action = best_act;
        out.rl_weights = build_action_weights(n_obs, best_act);

        if (rl_daily_rets.size() >= 2) {
            double m = std::accumulate(rl_daily_rets.begin(), rl_daily_rets.end(), 0.0)
                     / static_cast<double>(rl_daily_rets.size());
            double sq = 0.0;
            for (double r : rl_daily_rets) sq += (r - m) * (r - m);
            double sd = std::sqrt(std::max(1e-10, sq / static_cast<double>(rl_daily_rets.size() - 1)));
            out.rl_return_ann = m * 252.0;
            out.rl_vol_ann = sd * std::sqrt(252.0);
            out.rl_sharpe = (out.rl_vol_ann > 1e-6) ? ((out.rl_return_ann - rf_rate) / out.rl_vol_ann) : 0.0;
        }
        return out;
    }
};

} // namespace quant
