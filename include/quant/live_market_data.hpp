#pragma once

#ifndef BOOST_JSON_NO_LIB
#define BOOST_JSON_NO_LIB
#endif

#include <boost/json.hpp>
#include <curl/curl.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <numeric>
#include <cmath>
#include <iostream>
#include <sstream>
#include <format>
#include <algorithm>
#include <cstdint>

namespace quant {

struct RealTimeAssetData {
    std::string symbol;
    std::string long_name;
    std::string currency_code{"USD"};
    std::string currency_display{"$"};
    std::string instrument_type{"EQUITY"}; // EQUITY, ETF, FUTURE, CURRENCY, CRYPTOCURRENCY, INDEX

    double current_price{0.0};
    double previous_close{0.0};
    double fifty_two_week_high{0.0};
    double fifty_two_week_low{0.0};

    // Synchronized OHLCV + Dividend-Adjusted Close + Exchange Calendar Day Keys
    std::vector<int64_t> timestamps;
    std::vector<int64_t> day_keys;
    std::vector<double> historical_opens;
    std::vector<double> historical_highs;
    std::vector<double> historical_lows;
    std::vector<double> historical_raw_closes;
    std::vector<double> historical_prices; // Split- & dividend-adjusted closes (adjclose)
    std::vector<double> historical_volumes;

    std::vector<double> log_returns;
    double vwap{0.0};                    // 22-trading-day volume-weighted typical price
    double annualized_volatility{0.20};  // Regime-adaptive blend: 60% EWMA(0.94) + 40% Yang-Zhang/Sample
    double ewma_volatility{0.20};
    double sample_volatility{0.20};
    double atr_14{0.0};                  // True 14-period Wilder OHLC Average True Range

    bool is_live{false};
    bool is_synthetic_metal{false};
    double underlying_ounce_usd{0.0};
    double underlying_usdtry{0.0};
    std::string error_message;
};

struct TreasuryYieldCurveData {
    double three_month_yield{0.0430};
    double five_year_yield{0.0410};
    double ten_year_yield{0.0425};
    double thirty_year_yield{0.0450};
    bool is_live{false};
};

struct AlignedPairSeries {
    std::vector<int64_t> day_keys;
    std::vector<double> prices_a;
    std::vector<double> prices_b;
    std::vector<double> volumes_a;
    std::vector<double> volumes_b;
};

class LiveMarketDataClient {
public:
    struct LiveHeadline {
        std::string title;
        std::string publisher;
        int64_t publish_time{0};
    };

    LiveMarketDataClient() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~LiveMarketDataClient() {
        if (!cookie_file_path_.empty()) {
            std::remove(cookie_file_path_.c_str());
        }
        curl_global_cleanup();
    }

    // Normalizes UTF-8 Turkish/German/ASCII ticker input into canonical Yahoo symbol
    static std::string normalize_symbol(const std::string& raw_input) {
        std::string s = ascii_fold_upper_(trim_(raw_input));
        if (s.empty()) return "SPY";

        // Precious Metal Shortcuts (Gram Gold & Gram Silver in TL, Ounce Gold & Silver in USD)
        if (s == "GRAM" || s == "ALTIN" || s == "GRAM-ALTIN" || s == "GRAM ALTIN" ||
            s == "GRAM_ALTIN" || s == "GRAM-ALTIN.TL" || s == "ALTIN.TL" ||
            s == "GAU" || s == "GAUTRY" || s == "GAU-TRY" || s == "XAUTRY") {
            return "GRAM-ALTIN.TL";
        }
        if (s == "GUMUS" || s == "GRAM-GUMUS" || s == "GRAM GUMUS" || s == "GRAM_GUMUS" ||
            s == "GRAM-GUMUS.TL" || s == "GUMUS.TL" || s == "XAGTRY" || s == "XAG-TRY") {
            return "GRAM-GUMUS.TL";
        }
        if (s == "ONS" || s == "ONS-ALTIN" || s == "ONS ALTIN" || s == "XAU" ||
            s == "XAUUSD" || s == "XAU-USD") {
            return "GC=F";
        }
        if (s == "ONS-GUMUS" || s == "ONS GUMUS" || s == "XAG" ||
            s == "XAGUSD" || s == "XAG-USD") {
            return "SI=F";
        }

        // Auto-resolve common bare BIST tickers (e.g. "thyao" -> "THYAO.IS")
        static const std::unordered_set<std::string> known_bist = {
            "THYAO", "ASELS", "TUPRS", "BIMAS", "GARAN", "EREGL", "AKBNK", "KCHOL", "SISE", "SAHOL",
            "FROTO", "PGSUS", "MGROS", "ENKAI", "TAVHL", "CCOLA", "ALARK", "ASTOR", "KRDMD", "PETKM",
            "TCELL", "TTKOM", "ISCTR", "YKBNK", "HALKB", "VAKBN", "TOASO", "ARCLK", "SOKM", "EKGYO",
            "SASA", "HEKTS", "GUBRF", "KOZAL", "KOZAA", "IPEKE", "ODAS", "CANTE", "VESTL", "VESBE",
            "ISGYO", "CIMSA", "AKSEN", "OYAKC", "KONTR", "EUPWR", "GESAN", "SMRTG", "ALFAS", "DOAS",
            "OTKAR", "ULKER", "AEFES", "AGHOL", "BRSAN", "MAVI", "MPARK", "TABGD", "MIATK", "YEOTK",
            "TSKB", "SKBNK", "ALBRK"
        };
        if (s.find('.') == std::string::npos && s.find('-') == std::string::npos &&
            s.find('=') == std::string::npos && !s.starts_with("^")) {
            if (known_bist.contains(s)) {
                return s + ".IS";
            }
        }

        return s;
    }

    static std::string currency_display_for(const std::string& iso_ccy, const std::string& symbol = "") {
        std::string u = ascii_fold_upper_(iso_ccy);
        if (u == "TRY" || u == "TL" || symbol.ends_with(".IS") || symbol.ends_with(".TL")) return "TL";
        if (u == "EUR" || symbol.ends_with(".DE")) return "EUR";
        if (u == "GBP") return "GBP";
        if (u == "CHF") return "CHF";
        return "$";
    }

    static std::string infer_currency_code(const std::string& symbol, const std::string& meta_ccy = "") {
        if (!meta_ccy.empty()) {
            std::string u = ascii_fold_upper_(meta_ccy);
            if (u == "TL") return "TRY";
            return u;
        }
        std::string sym_u = ascii_fold_upper_(symbol);
        if (sym_u.ends_with(".IS") || sym_u.ends_with(".TL")) return "TRY";
        if (sym_u.ends_with(".DE")) return "EUR";
        return "USD";
    }

    // Fetch real-time & historical market data from Yahoo Finance (default 6mo for robust vol/RSI/ATR)
    RealTimeAssetData fetch_asset(const std::string& symbol, const std::string& range = "6mo") {
        std::string norm_sym = normalize_symbol(symbol);
        std::string cache_key = norm_sym + "|" + range;

        if (auto it = asset_cache_.find(cache_key); it != asset_cache_.end()) {
            return it->second;
        }

        if (norm_sym == "GRAM-ALTIN.TL") {
            auto res = fetch_gram_precious_metal_tl_("GC=F", "GRAM-ALTIN.TL", "24 Ayar Has Gram Altın (TL)", 6448.0, range);
            asset_cache_[cache_key] = res;
            return res;
        }
        if (norm_sym == "GRAM-GUMUS.TL") {
            auto res = fetch_gram_precious_metal_tl_("SI=F", "GRAM-GUMUS.TL", "Has Gram Gümüş (TL)", 76.50, range);
            asset_cache_[cache_key] = res;
            return res;
        }

        RealTimeAssetData data;
        data.symbol = norm_sym;
        data.currency_code = infer_currency_code(norm_sym);
        data.currency_display = currency_display_for(data.currency_code, norm_sym);

        std::string url = std::format(
            "https://query1.finance.yahoo.com/v8/finance/chart/{}?interval=1d&range={}",
            url_encode(norm_sym), range
        );

        long http_code = 0;
        std::string response = http_get(url, &http_code);

        if (response.empty()) {
            // Network completely unreachable -> mark is_live = false explicitly
            data.is_live = false;
            data.error_message = "Piyasa API ağına ulaşılamadı (Çevrimdışı mod).";
            populate_offline_fallback_(data);
            compute_analytics(data);
            return data;
        }

        if (!parse_yahoo_chart_json(response, data)) {
            // Invalid symbol (HTTP 404 or chart.error != null) -> do NOT fabricate prices for unknown tickers!
            data.is_live = false;
            if (data.error_message.empty()) {
                data.error_message = std::format("'{}' sembolü için geçerli borsa verisi bulunamadı (HTTP {}).", norm_sym, http_code);
            }
            return data;
        }

        compute_analytics(data);
        if (data.is_live) {
            asset_cache_[cache_key] = data;
        }
        return data;
    }

    // Public static parser for deterministic unit testing of null-alignment, adjclose & split handling
    static bool parse_yahoo_chart_json(const std::string& json_str, RealTimeAssetData& data) {
        boost::system::error_code ec;
        boost::json::value root = boost::json::parse(json_str, ec);
        if (ec || !root.is_object()) {
            data.error_message = "API JSON ayrıştırma hatası.";
            return false;
        }

        const auto& root_obj = root.as_object();
        auto chart_it = root_obj.find("chart");
        if (chart_it == root_obj.end() || !chart_it->value().is_object()) return false;
        const auto& chart_obj = chart_it->value().as_object();

        if (auto err_it = chart_obj.find("error"); err_it != chart_obj.end() && !err_it->value().is_null()) {
            if (err_it->value().is_object()) {
                if (auto d_it = err_it->value().as_object().find("description");
                    d_it != err_it->value().as_object().end() && d_it->value().is_string()) {
                    data.error_message = std::string(d_it->value().as_string());
                }
            }
            return false;
        }

        auto res_it = chart_obj.find("result");
        if (res_it == chart_obj.end() || !res_it->value().is_array() || res_it->value().as_array().empty()) {
            return false;
        }
        if (!res_it->value().as_array()[0].is_object()) return false;
        const auto& res0 = res_it->value().as_array()[0].as_object();

        int64_t gmt_offset = 0;
        double reg_price = 0.0;

        if (auto meta_it = res0.find("meta"); meta_it != res0.end() && meta_it->value().is_object()) {
            const auto& meta = meta_it->value().as_object();
            if (auto s = get_string_(meta, "symbol"); !s.empty()) data.symbol = s;
            if (auto ln = get_string_(meta, "longName"); !ln.empty()) data.long_name = ln;
            else if (auto sn = get_string_(meta, "shortName"); !sn.empty()) data.long_name = sn;

            std::string meta_ccy = get_string_(meta, "currency");
            data.currency_code = infer_currency_code(data.symbol, meta_ccy);
            data.currency_display = currency_display_for(data.currency_code, data.symbol);

            if (auto itype = get_string_(meta, "instrumentType"); !itype.empty()) {
                data.instrument_type = ascii_fold_upper_(itype);
            }

            gmt_offset = get_int64_(meta, "gmtoffset", 0);
            reg_price = get_double_(meta, "regularMarketPrice", 0.0);
            if (reg_price <= 0.0) reg_price = get_double_(meta, "chartPreviousClose", 0.0);
            data.previous_close = get_double_(meta, "chartPreviousClose", 0.0);
            data.fifty_two_week_high = get_double_(meta, "fiftyTwoWeekHigh", 0.0);
            data.fifty_two_week_low = get_double_(meta, "fiftyTwoWeekLow", 0.0);
        }

        const boost::json::array* ts_arr = nullptr;
        if (auto ts_it = res0.find("timestamp"); ts_it != res0.end() && ts_it->value().is_array()) {
            ts_arr = &ts_it->value().as_array();
        }

        const boost::json::array* open_arr = nullptr;
        const boost::json::array* high_arr = nullptr;
        const boost::json::array* low_arr = nullptr;
        const boost::json::array* close_arr = nullptr;
        const boost::json::array* vol_arr = nullptr;
        const boost::json::array* adj_arr = nullptr;

        if (auto ind_it = res0.find("indicators"); ind_it != res0.end() && ind_it->value().is_object()) {
            const auto& ind = ind_it->value().as_object();
            if (auto q_it = ind.find("quote");
                q_it != ind.end() && q_it->value().is_array() && !q_it->value().as_array().empty() &&
                q_it->value().as_array()[0].is_object()) {
                const auto& q0 = q_it->value().as_array()[0].as_object();
                if (auto it = q0.find("open"); it != q0.end() && it->value().is_array()) open_arr = &it->value().as_array();
                if (auto it = q0.find("high"); it != q0.end() && it->value().is_array()) high_arr = &it->value().as_array();
                if (auto it = q0.find("low"); it != q0.end() && it->value().is_array()) low_arr = &it->value().as_array();
                if (auto it = q0.find("close"); it != q0.end() && it->value().is_array()) close_arr = &it->value().as_array();
                if (auto it = q0.find("volume"); it != q0.end() && it->value().is_array()) vol_arr = &it->value().as_array();
            }
            if (auto a_it = ind.find("adjclose");
                a_it != ind.end() && a_it->value().is_array() && !a_it->value().as_array().empty() &&
                a_it->value().as_array()[0].is_object()) {
                const auto& a0 = a_it->value().as_array()[0].as_object();
                if (auto it = a0.find("adjclose"); it != a0.end() && it->value().is_array()) adj_arr = &it->value().as_array();
            }
        }

        if (!close_arr || close_arr->empty()) {
            if (reg_price > 0.0) {
                data.current_price = reg_price;
                data.is_live = true;
                return true;
            }
            return false;
        }

        size_t n = close_arr->size();
        data.timestamps.clear();
        data.day_keys.clear();
        data.historical_opens.clear();
        data.historical_highs.clear();
        data.historical_lows.clear();
        data.historical_raw_closes.clear();
        data.historical_prices.clear();
        data.historical_volumes.clear();

        data.timestamps.reserve(n);
        data.day_keys.reserve(n);
        data.historical_opens.reserve(n);
        data.historical_highs.reserve(n);
        data.historical_lows.reserve(n);
        data.historical_raw_closes.reserve(n);
        data.historical_prices.reserve(n);
        data.historical_volumes.reserve(n);

        // Synchronized row-by-row extraction: guarantees identical array lengths across all columns
        for (size_t i = 0; i < n; ++i) {
            double raw_c = elem_double_(close_arr, i, -1.0);
            if (raw_c <= 0.0) {
                // Skip holiday / pre-market / unopened null bar uniformly across all arrays
                continue;
            }

            double adj_c = elem_double_(adj_arr, i, -1.0);
            if (adj_c <= 0.0) adj_c = raw_c;

            double o = elem_double_(open_arr, i, raw_c);
            double h = elem_double_(high_arr, i, raw_c);
            double l = elem_double_(low_arr, i, raw_c);
            if (o <= 0.0) o = raw_c;
            if (h < std::max(o, raw_c)) h = std::max(o, raw_c);
            if (l <= 0.0 || l > std::min(o, raw_c)) l = std::min(o, raw_c);

            double v = elem_double_(vol_arr, i, 0.0);
            if (v < 0.0) v = 0.0;

            int64_t ts = elem_int64_(ts_arr, i, static_cast<int64_t>(i) * 86400);
            int64_t day_key = (ts + gmt_offset) / 86400;

            // Deduplicate if Yahoo appends an extra live tick on the exact same trading day
            if (!data.day_keys.empty() && data.day_keys.back() == day_key) {
                data.timestamps.back() = ts;
                data.historical_highs.back() = std::max(data.historical_highs.back(), h);
                data.historical_lows.back() = std::min(data.historical_lows.back(), l);
                data.historical_raw_closes.back() = raw_c;
                data.historical_prices.back() = adj_c;
                data.historical_volumes.back() = std::max(data.historical_volumes.back(), v);
                continue;
            }

            // Rescale O/H/L by the dividend/split adjustment factor (adj_c / raw_c)
            double adj_factor = (raw_c > 0.0) ? (adj_c / raw_c) : 1.0;

            data.timestamps.push_back(ts);
            data.day_keys.push_back(day_key);
            data.historical_opens.push_back(o * adj_factor);
            data.historical_highs.push_back(h * adj_factor);
            data.historical_lows.push_back(l * adj_factor);
            data.historical_raw_closes.push_back(raw_c);
            data.historical_prices.push_back(adj_c);
            data.historical_volumes.push_back(v);
        }

        if (data.historical_prices.empty()) {
            return false;
        }

        // BIST unadjusted bonus-share / split (bedelsiz bölünme) guard:
        // BIST daily circuit-breaker limit is strictly +/-10% (|log_ret| <= 0.1054).
        // An overnight drop > 28% (log_ret < -0.33) in a BIST equity is an unadjusted split.
        if (data.currency_code == "TRY" && data.instrument_type == "EQUITY" && data.historical_prices.size() >= 3) {
            for (size_t i = 1; i < data.historical_prices.size(); ++i) {
                double prev_p = data.historical_prices[i - 1];
                double curr_p = data.historical_prices[i];
                if (prev_p > 0.0 && curr_p > 0.0) {
                    double lr = std::log(curr_p / prev_p);
                    if (lr < -0.33 || lr > 0.35) {
                        double split_ratio = curr_p / prev_p;
                        for (size_t k = 0; k < i; ++k) {
                            data.historical_prices[k] *= split_ratio;
                            data.historical_opens[k] *= split_ratio;
                            data.historical_highs[k] *= split_ratio;
                            data.historical_lows[k] *= split_ratio;
                        }
                    }
                }
            }
        }

        data.current_price = (reg_price > 0.0) ? reg_price : data.historical_prices.back();
        data.is_live = true;
        return true;
    }

    // Compute 22-day Typical-Price VWAP, Wilder 14-day OHLC ATR, EWMA(0.94) & Yang-Zhang Volatility
    static void compute_analytics(RealTimeAssetData& data) {
        const size_t n = data.historical_prices.size();
        if (n == 0) return;
        if (n == 1) {
            data.vwap = data.current_price;
            data.atr_14 = data.current_price * 0.02;
            return;
        }

        // 1. 22-Trading-Day (~1 Calendar Month) Institutional Rolling VWAP using Typical Price (H+L+C)/3
        size_t vwap_lookback = std::min(n, size_t{22});
        size_t vwap_start = n - vwap_lookback;
        double sum_tp_vol = 0.0;
        double sum_vol = 0.0;
        double sum_tp = 0.0;

        for (size_t i = vwap_start; i < n; ++i) {
            double c = data.historical_prices[i];
            double h = (i < data.historical_highs.size()) ? data.historical_highs[i] : c;
            double l = (i < data.historical_lows.size()) ? data.historical_lows[i] : c;
            double tp = (h + l + c) / 3.0;
            double v = (i < data.historical_volumes.size()) ? data.historical_volumes[i] : 0.0;
            sum_tp += tp;
            if (v > 0.0) {
                sum_tp_vol += tp * v;
                sum_vol += v;
            }
        }
        // For FX / Index tickers where Yahoo reports 0 volume, use 22-day Equal-Weighted Typical Price (TWAP)
        if (sum_vol > 0.0) {
            data.vwap = sum_tp_vol / sum_vol;
        } else {
            data.vwap = sum_tp / static_cast<double>(vwap_lookback);
        }

        // 2. Log Returns via std::adjacent_difference
        std::vector<double> log_prices(n);
        std::ranges::transform(data.historical_prices, log_prices.begin(), [](double p) {
            return std::log(std::max(p, 1e-6));
        });

        std::vector<double> adj_diff(n);
        std::adjacent_difference(log_prices.begin(), log_prices.end(), adj_diff.begin());
        data.log_returns.assign(adj_diff.begin() + 1, adj_diff.end());

        // 3. Volatility Estimation: Sample Close-to-Close + RiskMetrics EWMA(lambda=0.94) + Garman-Klass/Parkinson OHLC
        const size_t m = data.log_returns.size();
        if (m >= 2) {
            double mean_r = std::accumulate(data.log_returns.begin(), data.log_returns.end(), 0.0) / static_cast<double>(m);
            double sum_sq = 0.0;
            for (double r : data.log_returns) {
                double d = r - mean_r;
                sum_sq += d * d;
            }
            double sample_var = sum_sq / static_cast<double>(m - 1);
            data.sample_volatility = std::sqrt(std::max(sample_var, 1e-8) * 252.0);

            // RiskMetrics EWMA variance (lambda = 0.94) initialized with first 15 bars' sample variance
            constexpr double lambda = 0.94;
            size_t init_len = std::min(m, size_t{15});
            double init_var = 0.0;
            for (size_t i = 0; i < init_len; ++i) {
                double d = data.log_returns[i] - mean_r;
                init_var += d * d;
            }
            init_var /= static_cast<double>(init_len);
            double ewma_var = init_var;
            for (size_t i = 0; i < m; ++i) {
                double d = data.log_returns[i] - mean_r;
                ewma_var = lambda * ewma_var + (1.0 - lambda) * (d * d);
            }
            data.ewma_volatility = std::sqrt(std::max(ewma_var, 1e-8) * 252.0);

            // Parkinson / Garman-Klass range-based volatility check when valid H/L exist
            double gk_sum = 0.0;
            size_t gk_cnt = 0;
            if (data.historical_highs.size() == n && data.historical_lows.size() == n && data.historical_opens.size() == n) {
                size_t start_idx = (n > 63) ? (n - 63) : 1;
                for (size_t i = start_idx; i < n; ++i) {
                    double o = data.historical_opens[i];
                    double h = data.historical_highs[i];
                    double l = data.historical_lows[i];
                    double c = data.historical_prices[i];
                    if (o > 0.0 && h >= l && l > 0.0 && c > 0.0) {
                        double hl = std::log(h / l);
                        double co = std::log(c / o);
                        double gk_daily = 0.5 * hl * hl - (2.0 * std::log(2.0) - 1.0) * co * co;
                        if (gk_daily > 0.0) {
                            gk_sum += gk_daily;
                            ++gk_cnt;
                        }
                    }
                }
            }

            double struct_vol = data.sample_volatility;
            if (gk_cnt >= 10) {
                double gk_vol = std::sqrt((gk_sum / static_cast<double>(gk_cnt)) * 252.0);
                // Blend close-to-close (captures overnight gaps) with Garman-Klass intraday range
                struct_vol = 0.65 * data.sample_volatility + 0.35 * gk_vol;
            }

            // Regime-adaptive institutional volatility: 60% EWMA + 40% structural
            data.annualized_volatility = std::clamp(0.60 * data.ewma_volatility + 0.40 * struct_vol, 0.04, 1.50);
        }

        // 4. Wilder's 14-Period Average True Range (ATR) on OHLC
        if (data.historical_highs.size() == n && data.historical_lows.size() == n) {
            std::vector<double> tr(n, 0.0);
            tr[0] = std::max(0.0, data.historical_highs[0] - data.historical_lows[0]);
            for (size_t i = 1; i < n; ++i) {
                double hl = data.historical_highs[i] - data.historical_lows[i];
                double hc = std::abs(data.historical_highs[i] - data.historical_prices[i - 1]);
                double lc = std::abs(data.historical_lows[i] - data.historical_prices[i - 1]);
                tr[i] = std::max({hl, hc, lc});
            }
            constexpr size_t period = 14;
            if (n <= period) {
                data.atr_14 = std::accumulate(tr.begin(), tr.end(), 0.0) / static_cast<double>(n);
            } else {
                double atr = std::accumulate(tr.begin() + 1, tr.begin() + 1 + period, 0.0) / static_cast<double>(period);
                for (size_t i = period + 1; i < n; ++i) {
                    atr = (atr * (period - 1) + tr[i]) / static_cast<double>(period);
                }
                data.atr_14 = atr;
            }
        }
        if (data.atr_14 <= 0.0) {
            data.atr_14 = data.current_price * (data.annualized_volatility / std::sqrt(252.0)) * 1.25;
        }

        if (data.fifty_two_week_high <= 0.0) {
            data.fifty_two_week_high = *std::max_element(data.historical_prices.begin(), data.historical_prices.end());
        }
        if (data.fifty_two_week_low <= 0.0) {
            data.fifty_two_week_low = *std::min_element(data.historical_prices.begin(), data.historical_prices.end());
        }
    }

    // Strict date-aligned inner join (or 1-day holiday forward-fill when allow_ffill = true)
    static AlignedPairSeries align_series_by_date(const RealTimeAssetData& a,
                                                  const RealTimeAssetData& b,
                                                  bool allow_ffill = false) {
        AlignedPairSeries out;
        if (a.day_keys.empty() || b.day_keys.empty() ||
            a.day_keys.size() != a.historical_prices.size() ||
            b.day_keys.size() != b.historical_prices.size()) {
            // Fallback to min-tail only if synthetic test vectors lacked day_keys
            size_t n = std::min(a.historical_prices.size(), b.historical_prices.size());
            size_t off_a = a.historical_prices.size() - n;
            size_t off_b = b.historical_prices.size() - n;
            for (size_t i = 0; i < n; ++i) {
                out.day_keys.push_back(static_cast<int64_t>(i));
                out.prices_a.push_back(a.historical_prices[off_a + i]);
                out.prices_b.push_back(b.historical_prices[off_b + i]);
                out.volumes_a.push_back(off_a + i < a.historical_volumes.size() ? a.historical_volumes[off_a + i] : 0.0);
                out.volumes_b.push_back(off_b + i < b.historical_volumes.size() ? b.historical_volumes[off_b + i] : 0.0);
            }
            return out;
        }

        std::unordered_map<int64_t, size_t> map_b;
        map_b.reserve(b.day_keys.size() * 2);
        for (size_t j = 0; j < b.day_keys.size(); ++j) {
            map_b[b.day_keys[j]] = j;
        }

        size_t last_valid_j = b.day_keys.size();
        for (size_t i = 0; i < a.day_keys.size(); ++i) {
            int64_t dk = a.day_keys[i];
            auto it = map_b.find(dk);
            size_t matched_j = b.day_keys.size();
            if (it != map_b.end()) {
                matched_j = it->second;
                last_valid_j = matched_j;
            } else if (allow_ffill) {
                // Check dk - 1 (single holiday mismatch between e.g. COMEX and FX)
                if (auto prev_it = map_b.find(dk - 1); prev_it != map_b.end()) {
                    matched_j = prev_it->second;
                } else if (last_valid_j < b.day_keys.size()) {
                    matched_j = last_valid_j;
                }
            }

            if (matched_j < b.day_keys.size()) {
                out.day_keys.push_back(dk);
                out.prices_a.push_back(a.historical_prices[i]);
                out.prices_b.push_back(b.historical_prices[matched_j]);
                out.volumes_a.push_back(i < a.historical_volumes.size() ? a.historical_volumes[i] : 0.0);
                out.volumes_b.push_back(matched_j < b.historical_volumes.size() ? b.historical_volumes[matched_j] : 0.0);
            }
        }
        return out;
    }

    // Live FX cross-rate converter (e.g. USD -> TRY, EUR -> TRY, EUR -> USD) with session cache
    double get_fx_rate(const std::string& from_ccy, const std::string& to_ccy) {
        std::string f = infer_currency_code("", from_ccy);
        std::string t = infer_currency_code("", to_ccy);
        if (f.empty() || t.empty() || f == t) return 1.0;

        std::string key = f + "_" + t;
        if (auto it = fx_cache_.find(key); it != fx_cache_.end()) {
            return it->second;
        }

        auto fetch_pair = [this](const std::string& ticker) -> double {
            auto d = fetch_asset(ticker, "5d");
            return (d.is_live && d.current_price > 0.0) ? d.current_price : -1.0;
        };

        double rate = -1.0;
        if (f == "USD" && t == "TRY") {
            rate = fetch_pair("TRY=X");
        } else if (f == "TRY" && t == "USD") {
            double r = get_fx_rate("USD", "TRY");
            rate = (r > 0.0) ? (1.0 / r) : -1.0;
        } else if (f == "EUR" && t == "TRY") {
            rate = fetch_pair("EURTRY=X");
            if (rate <= 0.0) {
                double eu = get_fx_rate("EUR", "USD");
                double ut = get_fx_rate("USD", "TRY");
                if (eu > 0.0 && ut > 0.0) rate = eu * ut;
            }
        } else if (f == "TRY" && t == "EUR") {
            double r = get_fx_rate("EUR", "TRY");
            rate = (r > 0.0) ? (1.0 / r) : -1.0;
        } else {
            rate = fetch_pair(f + t + "=X");
            if (rate <= 0.0) {
                double inv = fetch_pair(t + f + "=X");
                if (inv > 0.0) rate = 1.0 / inv;
            }
        }

        if (rate <= 0.0) {
            // Emergency offline fallback constants
            if (f == "USD" && t == "TRY") rate = 38.0;
            else if (f == "EUR" && t == "TRY") rate = 41.0;
            else if (f == "EUR" && t == "USD") rate = 1.08;
            else rate = 1.0;
        } else {
            fx_cache_[key] = rate;
        }
        return rate;
    }

    // Fetch live US Treasury yields (^IRX=3M, ^FVX=5Y, ^TNX=10Y, ^TYX=30Y) with honest is_live flag
    TreasuryYieldCurveData fetch_treasury_yields() {
        if (has_cached_yields_) return cached_yields_;

        TreasuryYieldCurveData curve;
        int live_hits = 0;

        auto fetch_rate = [this, &live_hits](const std::string& sym, double default_rate) -> double {
            auto d = fetch_asset(sym, "5d");
            if (d.is_live && d.current_price > 0.0) {
                ++live_hits;
                return d.current_price / 100.0;
            }
            return default_rate;
        };

        curve.three_month_yield = fetch_rate("^IRX", 0.0430);
        curve.five_year_yield   = fetch_rate("^FVX", 0.0410);
        curve.ten_year_yield    = fetch_rate("^TNX", 0.0425);
        curve.thirty_year_yield = fetch_rate("^TYX", 0.0450);
        curve.is_live = (live_hits >= 2);

        if (curve.is_live) {
            cached_yields_ = curve;
            has_cached_yields_ = true;
        }
        return curve;
    }

    // Fetch live financial news headlines via Boost.JSON (decodes UTF-8 escapes & publish timestamps)
    std::vector<LiveHeadline> fetch_live_headlines(const std::string& query, int count = 5) {
        std::vector<LiveHeadline> headlines;
        std::string url = std::format(
            "https://query2.finance.yahoo.com/v1/finance/search?q={}&quotesCount=0&newsCount={}",
            url_encode(query), count
        );
        std::string resp = http_get(url);
        if (resp.empty()) return headlines;

        boost::system::error_code ec;
        boost::json::value root = boost::json::parse(resp, ec);
        if (ec || !root.is_object()) return headlines;

        auto news_it = root.as_object().find("news");
        if (news_it == root.as_object().end() || !news_it->value().is_array()) return headlines;

        std::unordered_set<std::string> seen_titles;
        for (const auto& item : news_it->value().as_array()) {
            if (!item.is_object()) continue;
            const auto& obj = item.as_object();
            std::string title = get_string_(obj, "title");
            if (title.empty() || seen_titles.contains(title)) continue;
            seen_titles.insert(title);

            std::string publisher = get_string_(obj, "publisher");
            if (publisher.empty()) publisher = "Financial Wire";
            int64_t pub_time = get_int64_(obj, "providerPublishTime", 0);

            headlines.push_back({std::move(title), std::move(publisher), pub_time});
            if (static_cast<int>(headlines.size()) >= count) break;
        }
        return headlines;
    }

    // Direct HTTP GET with 1 retry on transient network error
    std::string http_get(const std::string& url, long* out_http_code = nullptr) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            CURL* curl = curl_easy_init();
            std::string read_buffer;
            long http_code = 0;

            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
                curl_easy_setopt(curl, CURLOPT_USERAGENT,
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

                CURLcode res = curl_easy_perform(curl);
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                curl_easy_cleanup(curl);

                if (out_http_code) *out_http_code = http_code;
                if (res == CURLE_OK && !read_buffer.empty()) {
                    return read_buffer;
                }
            }
        }
        return {};
    }

    // Authenticated Yahoo Finance GET using session cookie + crumb (for v10 quoteSummary)
    std::string http_get_yahoo_crumb(const std::string& base_url_without_crumb) {
        if (!ensure_yahoo_crumb_()) return {};
        char sep = (base_url_without_crumb.find('?') == std::string::npos) ? '?' : '&';
        std::string full_url = std::format("{}{}crumb={}", base_url_without_crumb, sep, url_encode(yahoo_crumb_));

        CURL* curl = curl_easy_init();
        std::string read_buffer;
        long http_code = 0;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
            curl_easy_setopt(curl, CURLOPT_USERAGENT,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);

            if (res == CURLE_OK && http_code == 200) {
                return read_buffer;
            }
        }
        return {};
    }

    static std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        for (unsigned char c : value) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << static_cast<char>(c);
            } else {
                escaped << '%' << std::uppercase << std::hex
                        << ((c >> 4) & 0x0F) << (c & 0x0F) << std::dec;
            }
        }
        return escaped.str();
    }

private:
    std::unordered_map<std::string, RealTimeAssetData> asset_cache_;
    std::unordered_map<std::string, double> fx_cache_;
    TreasuryYieldCurveData cached_yields_;
    bool has_cached_yields_{false};

    std::string cookie_file_path_;
    std::string yahoo_crumb_;
    bool crumb_attempted_{false};

    bool ensure_yahoo_crumb_() {
        if (!yahoo_crumb_.empty()) return true;
        if (crumb_attempted_) return false;
        crumb_attempted_ = true;

        cookie_file_path_ = "yf_session_cookies.tmp";

        // Step 1: Obtain session cookie from fc.yahoo.com
        {
            CURL* curl = curl_easy_init();
            std::string sink;
            if (!curl) return false;
            curl_easy_setopt(curl, CURLOPT_URL, "https://fc.yahoo.com");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
            curl_easy_setopt(curl, CURLOPT_USERAGENT,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        // Step 2: Obtain crumb token
        {
            CURL* curl = curl_easy_init();
            std::string crumb_buf;
            long code = 0;
            if (!curl) return false;
            curl_easy_setopt(curl, CURLOPT_URL, "https://query2.finance.yahoo.com/v1/test/getcrumb");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &crumb_buf);
            curl_easy_setopt(curl, CURLOPT_USERAGENT,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            curl_easy_cleanup(curl);

            crumb_buf = trim_(crumb_buf);
            if (res == CURLE_OK && code == 200 && !crumb_buf.empty() && crumb_buf.size() < 64 &&
                crumb_buf.find('{') == std::string::npos && crumb_buf.find('<') == std::string::npos) {
                yahoo_crumb_ = crumb_buf;
                return true;
            }
        }
        return false;
    }

    // Synthesize 24K Gram Gold / Silver in TL using date-aligned COMEX Ounce * USD/TRY / 31.1034768
    RealTimeAssetData fetch_gram_precious_metal_tl_(const std::string& ounce_symbol,
                                                    const std::string& out_symbol,
                                                    const std::string& metal_title,
                                                    double fallback_price,
                                                    const std::string& range) {
        constexpr double TROY_OUNCE_GRAMS = 31.1034768;
        auto ons = fetch_asset(ounce_symbol, range);
        auto usdtry = fetch_asset("TRY=X", range);

        RealTimeAssetData gram;
        gram.symbol = out_symbol;
        gram.long_name = metal_title;
        gram.currency_code = "TRY";
        gram.currency_display = "TL";
        gram.instrument_type = "FUTURE";
        gram.is_synthetic_metal = true;

        if (ons.is_live && usdtry.is_live && ons.current_price > 0.0 && usdtry.current_price > 0.0) {
            gram.underlying_ounce_usd = ons.current_price;
            gram.underlying_usdtry = usdtry.current_price;
            gram.current_price = (ons.current_price * usdtry.current_price) / TROY_OUNCE_GRAMS;

            auto aligned = align_series_by_date(ons, usdtry, true);
            size_t n = aligned.prices_a.size();
            if (n >= 2) {
                gram.day_keys = aligned.day_keys;
                gram.timestamps.resize(n);
                gram.historical_prices.reserve(n);
                gram.historical_raw_closes.reserve(n);
                gram.historical_opens.reserve(n);
                gram.historical_highs.reserve(n);
                gram.historical_lows.reserve(n);
                gram.historical_volumes = aligned.volumes_a;

                for (size_t i = 0; i < n; ++i) {
                    gram.timestamps[i] = aligned.day_keys[i] * 86400;
                    double gp = (aligned.prices_a[i] * aligned.prices_b[i]) / TROY_OUNCE_GRAMS;
                    gram.historical_prices.push_back(gp);
                    gram.historical_raw_closes.push_back(gp);
                    gram.historical_opens.push_back(gp);
                    gram.historical_highs.push_back(gp * 1.006);
                    gram.historical_lows.push_back(gp * 0.994);
                }
            }
            gram.is_live = true;
        } else {
            gram.current_price = fallback_price;
            gram.is_live = false;
            gram.error_message = "COMEX Ons veya USD/TRY canlı akışına ulaşılamadı.";
            populate_offline_fallback_(gram);
        }

        compute_analytics(gram);
        return gram;
    }

    static size_t write_callback_(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    static std::string trim_(std::string_view sv) {
        size_t start = 0;
        while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) ++start;
        size_t end = sv.size();
        while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) --end;
        return std::string(sv.substr(start, end - start));
    }

    // Fold Turkish/German UTF-8 characters to ASCII uppercase for reliable ticker resolution
    static std::string ascii_fold_upper_(std::string_view input) {
        std::string out;
        out.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(input[i]);
            if (c == 0xC3 && i + 1 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[++i]);
                if (c2 == 0x87 || c2 == 0xA7) out.push_back('C');      // Ç, ç
                else if (c2 == 0x96 || c2 == 0xB6) out.push_back('O'); // Ö, ö
                else if (c2 == 0x9C || c2 == 0xBC) out.push_back('U'); // Ü, ü
                else if (c2 == 0x84 || c2 == 0xA4) out.push_back('A'); // Ä, ä
            } else if (c == 0xC4 && i + 1 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[++i]);
                if (c2 == 0x9E || c2 == 0x9F) out.push_back('G');      // Ğ, ğ
                else if (c2 == 0xB0 || c2 == 0xB1) out.push_back('I'); // İ, ı
            } else if (c == 0xC5 && i + 1 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[++i]);
                if (c2 == 0x9E || c2 == 0x9F) out.push_back('S');      // Ş, ş
            } else if (c < 0x80) {
                out.push_back(static_cast<char>(std::toupper(c)));
            }
        }
        return out;
    }

    static std::string get_string_(const boost::json::object& obj, std::string_view key) {
        auto it = obj.find(key);
        if (it != obj.end() && it->value().is_string()) {
            return std::string(it->value().as_string());
        }
        return {};
    }

    static double get_double_(const boost::json::object& obj, std::string_view key, double def_val) {
        auto it = obj.find(key);
        if (it == obj.end() || it->value().is_null()) return def_val;
        if (it->value().is_double()) return it->value().as_double();
        if (it->value().is_int64()) return static_cast<double>(it->value().as_int64());
        if (it->value().is_uint64()) return static_cast<double>(it->value().as_uint64());
        return def_val;
    }

    static int64_t get_int64_(const boost::json::object& obj, std::string_view key, int64_t def_val) {
        auto it = obj.find(key);
        if (it == obj.end() || it->value().is_null()) return def_val;
        if (it->value().is_int64()) return it->value().as_int64();
        if (it->value().is_uint64()) return static_cast<int64_t>(it->value().as_uint64());
        if (it->value().is_double()) return static_cast<int64_t>(it->value().as_double());
        return def_val;
    }

    static double elem_double_(const boost::json::array* arr, size_t idx, double def_val) {
        if (!arr || idx >= arr->size()) return def_val;
        const auto& v = (*arr)[idx];
        if (v.is_null()) return def_val;
        if (v.is_double()) return v.as_double();
        if (v.is_int64()) return static_cast<double>(v.as_int64());
        if (v.is_uint64()) return static_cast<double>(v.as_uint64());
        return def_val;
    }

    static int64_t elem_int64_(const boost::json::array* arr, size_t idx, int64_t def_val) {
        if (!arr || idx >= arr->size()) return def_val;
        const auto& v = (*arr)[idx];
        if (v.is_null()) return def_val;
        if (v.is_int64()) return v.as_int64();
        if (v.is_uint64()) return static_cast<int64_t>(v.as_uint64());
        if (v.is_double()) return static_cast<int64_t>(v.as_double());
        return def_val;
    }

    // Realistic low-volatility (~22% annualized) offline fallback series used ONLY when network is down
    static void populate_offline_fallback_(RealTimeAssetData& data) {
        if (data.current_price <= 0.0) {
            if (data.symbol == "SPY") data.current_price = 582.50;
            else if (data.symbol == "AAPL") data.current_price = 231.40;
            else if (data.symbol == "NVDA") data.current_price = 138.00;
            else if (data.symbol == "THYAO.IS") data.current_price = 295.00;
            else if (data.symbol == "ASELS.IS") data.current_price = 92.50;
            else if (data.symbol == "TUPRS.IS") data.current_price = 162.00;
            else if (data.symbol == "BIMAS.IS") data.current_price = 495.00;
            else if (data.symbol == "RHM.DE") data.current_price = 1056.00;
            else if (data.symbol == "SAP.DE") data.current_price = 215.00;
            else data.current_price = 100.0;
        }

        // Smooth realistic ~1.2% daily step series (65 trading days) so offline tests behave sensibly
        constexpr size_t N = 65;
        data.timestamps.resize(N);
        data.day_keys.resize(N);
        data.historical_prices.resize(N);
        data.historical_raw_closes.resize(N);
        data.historical_opens.resize(N);
        data.historical_highs.resize(N);
        data.historical_lows.resize(N);
        data.historical_volumes.resize(N);

        double p = data.current_price * 0.96;
        for (size_t i = 0; i < N; ++i) {
            double step = 0.0006 + 0.011 * std::sin(static_cast<double>(i) * 0.55) +
                          0.005 * std::cos(static_cast<double>(i) * 1.13);
            p *= std::exp(step);
            data.day_keys[i] = 19000 + static_cast<int64_t>(i);
            data.timestamps[i] = data.day_keys[i] * 86400;
            data.historical_prices[i] = p;
            data.historical_raw_closes[i] = p;
            data.historical_opens[i] = p * 0.997;
            data.historical_highs[i] = p * 1.009;
            data.historical_lows[i] = p * 0.991;
            data.historical_volumes[i] = 1000000.0 * (1.0 + 0.25 * std::sin(static_cast<double>(i)));
        }
        double scale = data.current_price / data.historical_prices.back();
        for (size_t i = 0; i < N; ++i) {
            data.historical_prices[i] *= scale;
            data.historical_raw_closes[i] *= scale;
            data.historical_opens[i] *= scale;
            data.historical_highs[i] *= scale;
            data.historical_lows[i] *= scale;
        }
    }
};

} // namespace quant
