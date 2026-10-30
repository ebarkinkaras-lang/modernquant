#pragma once

#include "option_info.hpp"
#include <vector>
#include <random>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <future>
#include <chrono>

namespace quant {

// Chapter 6: Generates a single simulated equity price scenario using Geometric Brownian Motion
class EquityPriceGenerator {
public:
    EquityPriceGenerator(double spot, int num_time_steps,
                         double time_to_expiration, double volatility,
                         double rf_rate, double div_rate = 0.0)
        : spot_{spot}, num_time_steps_{num_time_steps},
          time_to_expiration_{time_to_expiration},
          volatility_{volatility}, rf_rate_{rf_rate}, div_rate_{div_rate},
          dt_{time_to_expiration / num_time_steps} {}

    std::vector<double> operator()(unsigned seed) const {
        std::vector<double> v;
        v.reserve(num_time_steps_ + 1);

        std::mt19937_64 mt(seed);
        std::normal_distribution<> nd;

        auto new_price = [this](double previous_equity_price, double norm) {
            double exp_arg_01 = (rf_rate_ - div_rate_ - ((volatility_ * volatility_) / 2.0)) * dt_;
            double exp_arg_02 = volatility_ * norm * std::sqrt(dt_);
            return previous_equity_price * std::exp(exp_arg_01 + exp_arg_02);
        };

        v.push_back(spot_);
        double equity_price = spot_;

        for (int i = 1; i <= num_time_steps_; ++i) {
            equity_price = new_price(equity_price, nd(mt));
            v.push_back(equity_price);
        }
        return v;
    }

private:
    double spot_;
    int num_time_steps_;
    double time_to_expiration_;
    double volatility_;
    double rf_rate_;
    double div_rate_;
    double dt_;
};

// Chapter 6: Scoped enum for barrier condition
enum class BarrierType {
    none,
    up_and_out,
    down_and_out
};

// Chapter 6: Monte Carlo Option Valuation Engine supporting Concurrency & Path-Dependent Options
class MCOptionValuation {
public:
    MCOptionValuation(OptionInfo&& opt, unsigned time_steps, double vol,
                      double int_rate, double div_rate = 0.0,
                      BarrierType barrier_type = BarrierType::none,
                      double barrier_value = 0.0)
        : opt_{std::move(opt)}, time_steps_{time_steps},
          vol_{vol}, int_rate_{int_rate}, div_rate_{div_rate},
          barrier_type_{barrier_type}, barrier_value_{barrier_value} {}

    // Sequential Calculation
    double calc_price(double spot, unsigned num_scenarios, unsigned unif_start_seed) const {
        // Immediate check if spot already breached barrier
        bool barrier_hit_init =
            (barrier_type_ == BarrierType::up_and_out && spot >= barrier_value_) ||
            (barrier_type_ == BarrierType::down_and_out && spot <= barrier_value_);
        if (barrier_hit_init) return 0.0;

        if (opt_.time_to_expiration() <= 0.0) {
            return opt_.option_payoff(spot);
        }

        std::mt19937_64 mt_unif{unif_start_seed};
        std::uniform_int_distribution<unsigned> unif_int_dist{};
        const double disc_factor = std::exp(-int_rate_ * opt_.time_to_expiration());

        std::vector<double> discounted_payoffs;
        discounted_payoffs.reserve(num_scenarios);

        EquityPriceGenerator epg{spot, static_cast<int>(time_steps_),
                                opt_.time_to_expiration(), vol_, int_rate_, div_rate_};

        for (unsigned i = 0; i < num_scenarios; ++i) {
            auto scenario = epg(unif_int_dist(mt_unif));
            bool barrier_hit = false;

            switch (barrier_type_) {
                case BarrierType::none:
                    break;
                case BarrierType::up_and_out: {
                    auto it = std::ranges::find_if(scenario, [this](double s) {
                        return s >= barrier_value_;
                    });
                    if (it != scenario.cend()) barrier_hit = true;
                    break;
                }
                case BarrierType::down_and_out: {
                    auto it = std::ranges::find_if(scenario, [this](double s) {
                        return s <= barrier_value_;
                    });
                    if (it != scenario.cend()) barrier_hit = true;
                    break;
                }
            }

            if (barrier_hit) {
                discounted_payoffs.push_back(0.0);
            } else {
                discounted_payoffs.push_back(disc_factor * opt_.option_payoff(scenario.back()));
            }
        }

        return (1.0 / num_scenarios) *
               std::accumulate(discounted_payoffs.cbegin(), discounted_payoffs.cend(), 0.0);
    }

    // Task-based Concurrency Calculation using std::async & std::future (Chapter 6, Page 217-219)
    double calc_price_par(double spot, unsigned num_scenarios, unsigned unif_start_seed) const {
        bool barrier_hit_init =
            (barrier_type_ == BarrierType::up_and_out && spot >= barrier_value_) ||
            (barrier_type_ == BarrierType::down_and_out && spot <= barrier_value_);
        if (barrier_hit_init) return 0.0;

        if (opt_.time_to_expiration() <= 0.0) {
            return opt_.option_payoff(spot);
        }

        std::mt19937_64 mt_unif{unif_start_seed};
        std::uniform_int_distribution<unsigned> unif_int_dist{};
        const double disc_factor = std::exp(-int_rate_ * opt_.time_to_expiration());

        // Spawn parallel tasks in batches to balance thread pool throughput
        const unsigned hardware_threads = std::max(1u, std::thread::hardware_concurrency());
        const unsigned scenarios_per_thread = num_scenarios / hardware_threads;

        struct BatchResult {
            double sum_payoff{0.0};
        };

        std::vector<std::future<BatchResult>> futures;
        futures.reserve(hardware_threads);

        for (unsigned t = 0; t < hardware_threads; ++t) {
            unsigned count = (t == hardware_threads - 1)
                                 ? (num_scenarios - t * scenarios_per_thread)
                                 : scenarios_per_thread;
            unsigned thread_seed = unif_int_dist(mt_unif);

            futures.push_back(std::async(std::launch::async, [this, spot, count, thread_seed, disc_factor]() {
                std::mt19937_64 local_mt{thread_seed};
                std::uniform_int_distribution<unsigned> local_dist{};
                EquityPriceGenerator local_epg{spot, static_cast<int>(time_steps_),
                                              opt_.time_to_expiration(), vol_, int_rate_, div_rate_};

                double local_sum = 0.0;
                for (unsigned i = 0; i < count; ++i) {
                    auto scenario = local_epg(local_dist(local_mt));
                    bool barrier_hit = false;

                    switch (barrier_type_) {
                        case BarrierType::none:
                            break;
                        case BarrierType::up_and_out: {
                            auto it = std::ranges::find_if(scenario, [this](double s) {
                                return s >= barrier_value_;
                            });
                            if (it != scenario.cend()) barrier_hit = true;
                            break;
                        }
                        case BarrierType::down_and_out: {
                            auto it = std::ranges::find_if(scenario, [this](double s) {
                                return s <= barrier_value_;
                            });
                            if (it != scenario.cend()) barrier_hit = true;
                            break;
                        }
                    }

                    if (!barrier_hit) {
                        local_sum += disc_factor * opt_.option_payoff(scenario.back());
                    }
                }
                return BatchResult{local_sum};
            }));
        }

        double total_sum = 0.0;
        for (auto& f : futures) {
            total_sum += f.get().sum_payoff;
        }

        return total_sum / num_scenarios;
    }

    // Estimate Delta via numerical bumping (Page 221)
    double calc_numerical_delta(double spot, unsigned num_scenarios, unsigned seed, double bump = 0.005) const {
        double dS = spot * bump;
        double price_up = calc_price_par(spot + dS, num_scenarios, seed);
        double price_dn = calc_price_par(spot - dS, num_scenarios, seed);
        return (price_up - price_dn) / (2.0 * dS);
    }

private:
    OptionInfo opt_;
    unsigned time_steps_;
    double vol_;
    double int_rate_;
    double div_rate_;
    BarrierType barrier_type_{BarrierType::none};
    double barrier_value_{0.0};
};

} // namespace quant
