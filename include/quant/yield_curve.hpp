#pragma once

#include "dates.hpp"
#include "daycount.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace quant {

// Chapter 7: YieldCurve Abstract Base Class
class YieldCurve {
public:
    virtual ~YieldCurve() = default;

    ChronoDate settle_date() const {
        return settle_;
    }

    Act365 act_365() const {
        return act_365_;
    }

    // Continuously compounded forward discount factor between two arbitrary dates
    double discount_factor(const ChronoDate& d1, const ChronoDate& d2) const {
        using std::exp;
        if (d2 < d1) {
            throw std::invalid_argument{"discount_factor: invalid inequality (d2 < d1)"};
        }
        if (d1 < settle_ || d2 < settle_) {
            throw std::invalid_argument{"discount_factor: dates must fall on or after settle date"};
        }
        if (d1 == d2) {
            return 1.0;
        }

        double t2 = act_365_.year_fraction(settle_, d2);
        double y2 = yield_curve_(t2);

        if (d1 == settle_) {
            return exp(-t2 * y2);
        }

        double t1 = act_365_.year_fraction(settle_, d1);
        double y1 = yield_curve_(t1);

        return exp(t1 * y1 - t2 * y2);
    }

protected:
    explicit YieldCurve(ChronoDate settle_date)
        : settle_{std::move(settle_date)} {}

private:
    virtual double yield_curve_(double t) const = 0;
    ChronoDate settle_;
    inline static Act365 act_365_{};
};

// Linearly Interpolated Yield Curve
class LinearInterpYieldCurve final : public YieldCurve {
public:
    LinearInterpYieldCurve(const ChronoDate& settle_date,
                           const std::vector<ChronoDate>& maturity_dates,
                           const std::vector<double>& unit_prices)
        : YieldCurve{settle_date} {
        
        if (maturity_dates.size() != unit_prices.size()) {
            throw std::invalid_argument{"Maturity dates and unit prices must have equal lengths"};
        }
        if (maturity_dates.empty()) {
            throw std::invalid_argument{"At least one maturity date is required"};
        }
        if (maturity_dates.front() < this->settle_date()) {
            throw std::invalid_argument{"First maturity date cannot be before settle date"};
        }

        maturities_.reserve(maturity_dates.size());
        yields_.reserve(maturity_dates.size());

        for (size_t i = 0; i < maturity_dates.size(); ++i) {
            double t = act_365().year_fraction(this->settle_date(), maturity_dates[i]);
            maturities_.push_back(t);
            if (t <= 1e-8) {
                yields_.push_back(0.0);
            } else {
                yields_.push_back(-std::log(unit_prices[i]) / t);
            }
        }
    }

    const std::vector<double>& maturities() const { return maturities_; }
    const std::vector<double>& yields() const { return yields_; }

private:
    double yield_curve_(double t) const override {
        if (t <= maturities_.front()) {
            return yields_.front();
        }
        if (t >= maturities_.back()) {
            return yields_.back();
        }

        size_t idx = 0;
        while (idx + 1 < maturities_.size() && maturities_[idx + 1] < t) {
            ++idx;
        }

        double dt = maturities_[idx + 1] - maturities_[idx];
        double dy = yields_[idx + 1] - yields_[idx];
        return yields_[idx] + (dy / dt) * (t - maturities_[idx]);
    }

    std::vector<double> maturities_;
    std::vector<double> yields_;
};

} // namespace quant
