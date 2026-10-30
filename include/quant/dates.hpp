#pragma once

#include <chrono>
#include <compare>
#include <iostream>
#include <stdexcept>
#include <string>
#include <format>

namespace quant {

// Chapter 7: Wrapper class around C++20 std::chrono calendrical and chronological types
class ChronoDate {
public:
    ChronoDate(int year, unsigned month, unsigned day)
        : date_{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}} {
        validate_();
    }

    ChronoDate(std::chrono::year_month_day ymd)
        : date_{std::move(ymd)} {
        validate_();
    }

    ChronoDate() = default;

    // Accessors
    int year() const {
        return static_cast<int>(date_.year());
    }

    unsigned month() const {
        return static_cast<unsigned>(date_.month());
    }

    unsigned day() const {
        return static_cast<unsigned>(date_.day());
    }

    int serial_date() const {
        return std::chrono::sys_days(date_).time_since_epoch().count();
    }

    std::chrono::year_month_day ymd() const {
        return date_;
    }

    // Date properties
    unsigned days_in_month() const {
        std::chrono::year_month_day_last eom{date_.year() / date_.month() / std::chrono::last};
        return static_cast<unsigned>(eom.day());
    }

    bool is_end_of_month() const {
        return date_ == (date_.year() / date_.month() / std::chrono::last);
    }

    bool is_leap_year() const {
        return date_.year().is_leap();
    }

    // Operators
    int operator-(const ChronoDate& rhs) const {
        return this->serial_date() - rhs.serial_date();
    }

    bool operator==(const ChronoDate& rhs) const {
        return this->serial_date() == rhs.serial_date();
    }

    std::strong_ordering operator<=>(const ChronoDate& rhs) const {
        if (this->serial_date() < rhs.serial_date()) {
            return std::strong_ordering::less;
        }
        if (this->serial_date() == rhs.serial_date()) {
            return std::strong_ordering::equivalent;
        }
        return std::strong_ordering::greater;
    }

    // Modifying member functions
    ChronoDate& add_years(int rhs_years) {
        date_ += std::chrono::years(rhs_years);
        if (!date_.ok()) {
            // Leap year 29th February edge case in non-leap year: clamp to 28
            date_ = date_.year() / date_.month() / 28;
        }
        return *this;
    }

    ChronoDate& add_months(int rhs_months) {
        date_ += std::chrono::months(rhs_months);
        if (!date_.ok()) {
            date_ = date_.year() / date_.month() / std::chrono::day{days_in_month()};
        }
        return *this;
    }

    ChronoDate& add_days(int rhs_days) {
        auto sys = std::chrono::sys_days(date_) + std::chrono::days(rhs_days);
        date_ = sys;
        return *this;
    }

    // Modified Following Business-Day Roll Rule (Chapter 7, Page 245)
    // Rolls weekend dates (Saturday/Sunday) to Monday unless it crosses into next month,
    // in which case rolls back to Friday of the original month.
    ChronoDate& weekend_roll() {
        std::chrono::weekday wd{std::chrono::sys_days(date_)};
        std::chrono::month orig_mth{date_.month()};
        unsigned wdn = wd.iso_encoding(); // 1=Mon ... 6=Sat, 7=Sun

        if (wdn > 5) {
            date_ = std::chrono::sys_days(date_) + std::chrono::days(8 - wdn);
            if (orig_mth != date_.month()) {
                date_ = std::chrono::sys_days(date_) - std::chrono::days(3);
            }
        }
        return *this;
    }

    std::string to_string() const {
        return std::format("{:04d}-{:02d}-{:02d}", year(), month(), day());
    }

    friend std::ostream& operator<<(std::ostream& os, const ChronoDate& cd) {
        os << cd.to_string();
        return os;
    }

private:
    std::chrono::year_month_day date_{
        std::chrono::year{1970}, std::chrono::month{1}, std::chrono::day{1}
    };

    void validate_() const {
        if (!date_.ok()) {
            throw std::invalid_argument{
                std::format("ChronoDate constructor: Invalid date ({:04d}-{:02d}-{:02d})", 
                            static_cast<int>(date_.year()),
                            static_cast<unsigned>(date_.month()),
                            static_cast<unsigned>(date_.day()))
            };
        }
    }
};

} // namespace quant
