#pragma once

#include "dates.hpp"

namespace quant {

// Chapter 7: Day Count Basis Interface and Concrete Classes
class DayCount {
public:
    virtual double year_fraction(const ChronoDate& date_01, const ChronoDate& date_02) const = 0;
    virtual ~DayCount() = default;
};

// Actual/365
class Act365 final : public DayCount {
public:
    double year_fraction(const ChronoDate& date_01, const ChronoDate& date_02) const override {
        return (date_02 - date_01) / 365.0;
    }
};

// Actual/360
class Act360 final : public DayCount {
public:
    double year_fraction(const ChronoDate& date_01, const ChronoDate& date_02) const override {
        return (date_02 - date_01) / 360.0;
    }
};

// 30/360 ISDA
class Thirty360 final : public DayCount {
public:
    double year_fraction(const ChronoDate& date_01, const ChronoDate& date_02) const override {
        return date_diff_(date_01, date_02) / 360.0;
    }

private:
    int date_diff_(const ChronoDate& date_01, const ChronoDate& date_02) const {
        int d1 = static_cast<int>(date_01.day());
        int d2 = static_cast<int>(date_02.day());
        if (d1 == 31) d1 = 30;
        if (d2 == 31 && d1 == 30) d2 = 30;
        return 360 * (date_02.year() - date_01.year()) +
               30 * (static_cast<int>(date_02.month()) - static_cast<int>(date_01.month())) +
               (d2 - d1);
    }
};

} // namespace quant
