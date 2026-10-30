#pragma once

#include "payoff.hpp"
#include <utility>
#include <memory>

namespace quant {

// Chapter 3: OptionInfo Class
// Demonstrates modern RAII, resource ownership with std::unique_ptr,
// copy-and-swap idiom, and the Rule of Five.
class OptionInfo {
public:
    OptionInfo(std::unique_ptr<Payoff> payoff, double time_to_exp)
        : payoff_ptr_{std::move(payoff)}, time_to_exp_{time_to_exp} {}

    double option_payoff(double spot) const {
        return payoff_ptr_ ? payoff_ptr_->payoff(spot) : 0.0;
    }

    double time_to_expiration() const {
        return time_to_exp_;
    }

    PayoffType payoff_type() const {
        return payoff_ptr_ ? payoff_ptr_->type() : PayoffType::Call;
    }

    double strike() const {
        return payoff_ptr_ ? payoff_ptr_->strike() : 0.0;
    }

    void swap(OptionInfo& rhs) noexcept {
        using std::swap;
        swap(payoff_ptr_, rhs.payoff_ptr_);
        swap(time_to_exp_, rhs.time_to_exp_);
    }

    // Rule of Five:
    // 1. User-defined copy constructor via clone()
    OptionInfo(const OptionInfo& rhs)
        : payoff_ptr_{rhs.payoff_ptr_ ? rhs.payoff_ptr_->clone() : nullptr},
          time_to_exp_{rhs.time_to_exp_} {}

    // 2. User-defined copy assignment using copy-and-swap (provides strong exception guarantee)
    OptionInfo& operator=(const OptionInfo& rhs) {
        OptionInfo{rhs}.swap(*this);
        return *this;
    }

    // 3. Move constructor defaulted
    OptionInfo(OptionInfo&& rhs) noexcept = default;

    // 4. Move assignment defaulted
    OptionInfo& operator=(OptionInfo&& rhs) noexcept = default;

    // 5. Destructor defaulted (unique_ptr automatically cleans up heap memory)
    ~OptionInfo() = default;

private:
    std::unique_ptr<Payoff> payoff_ptr_;
    double time_to_exp_{0.0};
};

} // namespace quant
