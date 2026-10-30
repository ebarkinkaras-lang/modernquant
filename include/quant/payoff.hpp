#pragma once

#include <memory>
#include <algorithm>

namespace quant {

// Chapter 2 & 3: Scoped enumeration for option payoff type
enum class PayoffType {
    Call = 1,
    Put = -1
};

// Chapter 3: Polymorphic Payoff Abstract Base Class with Virtual Clone Idiom
class Payoff {
public:
    virtual double payoff(double spot) const = 0;
    virtual std::unique_ptr<Payoff> clone() const = 0;
    virtual PayoffType type() const = 0;
    virtual double strike() const = 0;
    virtual ~Payoff() = default;
};

// Concrete Call Payoff
class CallPayoff final : public Payoff {
public:
    explicit CallPayoff(double strike) : strike_{strike} {}

    double payoff(double spot) const override {
        return std::max(spot - strike_, 0.0);
    }

    std::unique_ptr<Payoff> clone() const override {
        return std::make_unique<CallPayoff>(*this);
    }

    PayoffType type() const override { return PayoffType::Call; }
    double strike() const override { return strike_; }

private:
    double strike_;
};

// Concrete Put Payoff
class PutPayoff final : public Payoff {
public:
    explicit PutPayoff(double strike) : strike_{strike} {}

    double payoff(double spot) const override {
        return std::max(strike_ - spot, 0.0);
    }

    std::unique_ptr<Payoff> clone() const override {
        return std::make_unique<PutPayoff>(*this);
    }

    PayoffType type() const override { return PayoffType::Put; }
    double strike() const override { return strike_; }

private:
    double strike_;
};

} // namespace quant
