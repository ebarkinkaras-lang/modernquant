#pragma once

#include "option_info.hpp"
#include <boost/multi_array.hpp>
#include <cmath>
#include <algorithm>
#include <vector>

namespace quant {

// Chapter 9: Lattice Node holding underlying asset price and calculated option payoff
struct Node {
    double underlying{0.0};
    double payoff{0.0};
    bool early_exercised{false};
};

enum class OptType {
    Euro,
    American
};

// Chapter 9: Binomial Lattice Option Pricer using Boost MultiArray
class BinomialLatticePricer {
public:
    BinomialLatticePricer(OptionInfo opt,
                          double vol, double int_rate, int time_steps,
                          double div_rate = 0.0)
        : opt_{std::move(opt)}, time_points_{time_steps + 1}, div_rate_{div_rate} {
        
        double dt = opt_.time_to_expiration() / time_steps;
        u_ = std::exp(vol * std::sqrt(dt));
        d_ = 1.0 / u_;
        p_ = 0.5 * (1.0 + (int_rate - div_rate_ - 0.5 * vol * vol) * std::sqrt(dt) / vol);
        disc_fctr_ = std::exp(-int_rate * dt);

        grid_.resize(boost::extents[time_points_][time_points_]);
    }

    double calc_price(double spot, OptType opt_type) {
        project_underlying_prices_(spot);
        return calculate_node_payoffs_(opt_type);
    }

    const boost::multi_array<Node, 2>& grid() const {
        return grid_;
    }

    int time_points() const {
        return time_points_;
    }

private:
    void project_underlying_prices_(double spot) {
        grid_[0][0].underlying = spot;
        for (int j = 1; j < time_points_; ++j) {
            for (int i = 0; i <= j; ++i) {
                if (i < j) {
                    grid_[i][j].underlying = u_ * grid_[i][j - 1].underlying;
                } else {
                    grid_[i][j].underlying = d_ * grid_[i - 1][j - 1].underlying;
                }
            }
        }
    }

    double calculate_node_payoffs_(OptType opt_type) {
        // Step 1: Terminal nodes at expiration
        int last_col = time_points_ - 1;
        for (int i = 0; i <= last_col; ++i) {
            grid_[i][last_col].payoff = opt_.option_payoff(grid_[i][last_col].underlying);
            grid_[i][last_col].early_exercised = false;
        }

        // Step 2 & 3: Backward induction along lattice
        for (int j = time_points_ - 2; j >= 0; --j) {
            for (int i = 0; i <= j; ++i) {
                double disc_exp = disc_expected_val_(i, j);
                if (opt_type == OptType::American) {
                    double immediate = opt_.option_payoff(grid_[i][j].underlying);
                    if (immediate > disc_exp) {
                        grid_[i][j].payoff = immediate;
                        grid_[i][j].early_exercised = true;
                    } else {
                        grid_[i][j].payoff = disc_exp;
                        grid_[i][j].early_exercised = false;
                    }
                } else {
                    grid_[i][j].payoff = disc_exp;
                    grid_[i][j].early_exercised = false;
                }
            }
        }

        return grid_[0][0].payoff;
    }

    double disc_expected_val_(int i, int j) const {
        return disc_fctr_ * (p_ * grid_[i][j + 1].payoff +
                             (1.0 - p_) * grid_[i + 1][j + 1].payoff);
    }

    OptionInfo opt_;
    int time_points_;
    double div_rate_;
    double u_{0.0};
    double d_{0.0};
    double p_{0.0};
    double disc_fctr_{0.0};
    boost::multi_array<Node, 2> grid_;
};

} // namespace quant
