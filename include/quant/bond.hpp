#pragma once

#include "dates.hpp"
#include "yield_curve.hpp"
#include <string>
#include <vector>

namespace quant {

// Chapter 7: Bond Class supporting regular/irregular coupons, weekend roll, and discounted valuation
class Bond {
public:
    Bond(const std::string& bond_id, const ChronoDate& dated_date,
         const ChronoDate& first_coupon_date,
         const ChronoDate& penultimate_coupon_date,
         const ChronoDate& maturity_date,
         int coupon_frequency, double coupon_rate, double face_value)
        : bond_id_{bond_id} {
        
        const int months_in_regular_period = 12 / coupon_frequency;
        const double regular_coupon_payment = coupon_rate * face_value / coupon_frequency;

        calculate_pmt_schedule_(first_coupon_date, penultimate_coupon_date,
                                months_in_regular_period, regular_coupon_payment);

        amend_initial_irregular_dates_and_pmts_(dated_date, first_coupon_date,
                                                months_in_regular_period, regular_coupon_payment);

        amend_final_irregular_dates_and_pmts_(penultimate_coupon_date, maturity_date,
                                              months_in_regular_period, regular_coupon_payment, face_value);

        due_dates_.push_back(maturity_date);
        ChronoDate mat_payment{maturity_date};
        payment_dates_.push_back(mat_payment.weekend_roll());
    }

    double discounted_value(const ChronoDate& bond_settle_date,
                            const YieldCurve& yield_curve) const {
        double pv = 0.0;
        for (size_t i = 0; i < due_dates_.size(); ++i) {
            if (bond_settle_date < due_dates_[i]) {
                pv += yield_curve.discount_factor(bond_settle_date, payment_dates_[i]) * payment_amounts_[i];
            }
        }
        return yield_curve.discount_factor(yield_curve.settle_date(), bond_settle_date) * pv;
    }

    std::string bond_id() const { return bond_id_; }
    const std::vector<ChronoDate>& due_dates() const { return due_dates_; }
    const std::vector<ChronoDate>& payment_dates() const { return payment_dates_; }
    const std::vector<double>& payment_amounts() const { return payment_amounts_; }

private:
    void calculate_pmt_schedule_(const ChronoDate& first_coupon_date,
                                 const ChronoDate& penultimate_coupon_date,
                                 int months_in_regular_period, double regular_coupon_payment) {
        for (ChronoDate reg_due{first_coupon_date};
             reg_due <= penultimate_coupon_date;
             reg_due.add_months(months_in_regular_period)) {
            
            due_dates_.push_back(reg_due);
            ChronoDate pmt_date{reg_due};
            payment_dates_.push_back(pmt_date.weekend_roll());
            payment_amounts_.push_back(regular_coupon_payment);
        }
    }

    void amend_initial_irregular_dates_and_pmts_(const ChronoDate& dated_date,
                                                const ChronoDate& first_coupon_date,
                                                int months_in_regular_period,
                                                double regular_coupon_payment) {
        ChronoDate first_prior{first_coupon_date};
        first_prior.add_months(-months_in_regular_period);

        if (first_prior != dated_date) {
            if (first_prior < dated_date) {
                // Short first coupon
                double fraction = static_cast<double>(first_coupon_date - dated_date) /
                                  static_cast<double>(first_coupon_date - first_prior);
                payment_amounts_[0] *= fraction;
            } else {
                // Long first coupon
                ChronoDate second_prior{first_prior};
                second_prior.add_months(-months_in_regular_period);
                double fraction = static_cast<double>(first_prior - dated_date) /
                                  static_cast<double>(first_prior - second_prior);
                payment_amounts_[0] += fraction * regular_coupon_payment;
            }
        }
    }

    void amend_final_irregular_dates_and_pmts_(const ChronoDate& penultimate_coupon_date,
                                              const ChronoDate& maturity_date,
                                              int months_in_regular_period,
                                              double regular_coupon_payment,
                                              double face_value) {
        ChronoDate mat_regular{penultimate_coupon_date};
        mat_regular.add_months(months_in_regular_period);
        double final_coupon = regular_coupon_payment;

        if (mat_regular != maturity_date) {
            if (maturity_date < mat_regular) {
                // Short final coupon
                double fraction = static_cast<double>(maturity_date - penultimate_coupon_date) /
                                  static_cast<double>(mat_regular - penultimate_coupon_date);
                final_coupon *= fraction;
            } else {
                // Long final coupon
                ChronoDate next_regular{mat_regular};
                next_regular.add_months(months_in_regular_period);
                double extra_fraction = static_cast<double>(maturity_date - mat_regular) /
                                        static_cast<double>(next_regular - mat_regular);
                final_coupon += extra_fraction * regular_coupon_payment;
            }
        }

        payment_amounts_.push_back(face_value + final_coupon);
    }

    std::string bond_id_;
    std::vector<ChronoDate> due_dates_;
    std::vector<ChronoDate> payment_dates_;
    std::vector<double> payment_amounts_;
};

} // namespace quant
