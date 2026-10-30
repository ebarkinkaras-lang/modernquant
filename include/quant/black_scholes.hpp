#pragma once

#include "payoff.hpp"
#include <array>
#include <cmath>
#include <numbers>
#include <map>
#include <limits>
#include <algorithm>

namespace quant {

// Chapter 4: Risk values (Greeks) enum class
enum class RiskValues {
    Delta,
    Gamma,
    Vega,
    Rho,
    Theta
};

// Chapter 2 & Chapter 4: Black-Scholes Model Class
class BlackScholes {
public:
    BlackScholes(double strike, double spot, double time_to_exp,
                 PayoffType payoff_type, double rate, double div = 0.0)
        : strike_{strike}, spot_{spot}, time_to_exp_{time_to_exp},
          payoff_type_{payoff_type}, rate_{rate}, div_{div} {}

    // Option valuation functor (operator()) as a function of volatility (sigma)
    double operator()(double vol) const {
        using std::exp;
        const int phi = static_cast<int>(payoff_type_);

        if (time_to_exp_ > 0.0) {
            auto norm_args = compute_norm_args_(vol);
            double d1 = norm_args[0];
            double d2 = norm_args[1];

            auto norm_cdf = [](double x) {
                return (1.0 + std::erf(x / std::numbers::sqrt2)) / 2.0;
            };

            double nd_1 = norm_cdf(phi * d1);
            double nd_2 = norm_cdf(phi * d2);
            double disc_fctr = exp(-rate_ * time_to_exp_);

            return phi * (spot_ * exp(-div_ * time_to_exp_) * nd_1 -
                          disc_fctr * strike_ * nd_2);
        } else {
            return std::max(phi * (spot_ - strike_), 0.0);
        }
    }

    // Chapter 4: Calculate all 5 key option Greeks
    std::map<RiskValues, double> risk_values(double vol) const {
        using std::exp, std::sqrt;
        std::map<RiskValues, double> results;

        if (time_to_exp_ <= 0.0) {
            results[RiskValues::Delta] = 0.0;
            results[RiskValues::Gamma] = 0.0;
            results[RiskValues::Vega]  = 0.0;
            results[RiskValues::Rho]   = 0.0;
            results[RiskValues::Theta] = 0.0;
            return results;
        }

        const int phi = static_cast<int>(payoff_type_);
        auto norm_args = compute_norm_args_(vol);
        double d1 = norm_args[0];
        double d2 = norm_args[1];

        auto norm_cdf = [this](double x) { return norm_cdf_(x); };
        double nd_1 = norm_cdf(phi * d1);
        double nd_2 = norm_cdf(phi * d2);
        double disc_fctr = exp(-rate_ * time_to_exp_);

        auto norm_pdf = [](double x) {
            return (1.0 / (std::numbers::sqrt2 * std::sqrt(std::numbers::pi))) * std::exp(-0.5 * x * x);
        };

        double delta = phi * exp(-div_ * time_to_exp_) * nd_1;
        double gamma = exp(-div_ * time_to_exp_) * norm_pdf(d1) /
                       (spot_ * vol * sqrt(time_to_exp_));
        double vega  = spot_ * spot_ * gamma * vol * time_to_exp_;
        double rho   = phi * time_to_exp_ * strike_ * disc_fctr * nd_2;
        double theta = phi * div_ * spot_ * exp(-div_ * time_to_exp_) * nd_1
                       - phi * rate_ * strike_ * exp(-rate_ * time_to_exp_) * nd_2
                       - spot_ * exp(-div_ * time_to_exp_) * norm_pdf(d1) * vol /
                         (2.0 * sqrt(time_to_exp_));

        results[RiskValues::Delta] = delta;
        results[RiskValues::Gamma] = gamma;
        results[RiskValues::Vega]  = vega;
        results[RiskValues::Rho]   = rho;
        results[RiskValues::Theta] = theta;

        return results;
    }

    // Accessors
    double strike() const { return strike_; }
    double spot() const { return spot_; }
    double time_to_expiration() const { return time_to_exp_; }
    double rate() const { return rate_; }
    double dividend() const { return div_; }
    PayoffType payoff_type() const { return payoff_type_; }

private:
    std::array<double, 2> compute_norm_args_(double vol) const {
        using std::log, std::sqrt;
        double numer = log(spot_ / strike_) + (rate_ - div_ + 0.5 * vol * vol) * time_to_exp_;
        double d1 = numer / (vol * sqrt(time_to_exp_));
        double d2 = d1 - vol * sqrt(time_to_exp_);
        return std::array<double, 2>{d1, d2};
    }

    double norm_cdf_(double x) const {
        return (1.0 + std::erf(x / std::numbers::sqrt2)) / 2.0;
    }

    double strike_{0.0};
    double spot_{0.0};
    double time_to_exp_{0.0};
    PayoffType payoff_type_{PayoffType::Call};
    double rate_{0.0};
    double div_{0.0};
};

// Chapter 2: Implied Volatility calculation via Secant Root Finding Method
inline double calculate_implied_volatility(const BlackScholes& bsc,
                                          double opt_mkt_price,
                                          double x0 = 0.10,
                                          double x1 = 0.25,
                                          double tol = 1e-6,
                                          unsigned max_iter = 1000) {
    auto diff = [&bsc, opt_mkt_price](double x) {
        return bsc(x) - opt_mkt_price;
    };

    double y0 = diff(x0);
    double y1 = diff(x1);
    double impl_vol = 0.0;

    for (unsigned count_iter = 0; count_iter <= max_iter; ++count_iter) {
        if (std::abs(y1 - y0) < 1e-15) {
            break;
        }

        if (std::abs(x1 - x0) > tol) {
            impl_vol = x1 - (x1 - x0) * y1 / (y1 - y0);
            if (impl_vol <= 0.0) {
                impl_vol = 0.001; // Clamp to avoid non-positive volatility
            }
            x0 = x1;
            x1 = impl_vol;
            y0 = y1;
            y1 = diff(x1);
        } else {
            return x1; // Convergence
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace quant
