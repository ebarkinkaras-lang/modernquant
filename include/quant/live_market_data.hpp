#pragma once

#include <string>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <sstream>
#include <format>
#include <algorithm>
#include <curl/curl.h>

namespace quant {

struct RealTimeAssetData {
    std::string symbol;
    double current_price{0.0};
    std::vector<double> historical_prices;
    std::vector<double> historical_volumes;
    std::vector<double> log_returns;
    double vwap{0.0};
    double annualized_volatility{0.20};
    bool is_live{false};
    std::string error_message;
};

struct TreasuryYieldCurveData {
    double three_month_yield{0.045}; // ~4.5%
    double five_year_yield{0.041};   // ~4.1%
    double ten_year_yield{0.042};    // ~4.2%
    double thirty_year_yield{0.045}; // ~4.5%
    bool is_live{false};
};

class LiveMarketDataClient {
public:
    LiveMarketDataClient() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~LiveMarketDataClient() {
        curl_global_cleanup();
    }

    // Fetch real-time market data (current price, historical series, volume) from Yahoo Finance
    RealTimeAssetData fetch_asset(const std::string& symbol, const std::string& range = "1mo") {
        std::string norm = symbol;
        std::transform(norm.begin(), norm.end(), norm.begin(), ::toupper);

        // 24 Ayar Has Gram Altın (TL) = (Ons Altın USD [GC=F] * USD/TRY [TRY=X]) / 31.1034768
        if (norm == "GRAM" || norm == "ALTIN" || norm == "GRAM-ALTIN" || norm == "GRAM ALTIN" ||
            norm == "GRAM_ALTIN" || norm == "GRAM-ALTIN.TL" || norm == "ALTIN.TL" ||
            norm == "GAU" || norm == "GAUTRY" || norm == "GAU-TRY" || norm == "XAUTRY" ||
            symbol == "gram altın" || symbol == "altın" || symbol == "gram altin") {
            return fetch_gram_precious_metal_tl_("GC=F", "GRAM-ALTIN.TL", 6448.0, range);
        }

        // Has Gram Gümüş (TL) = (Ons Gümüş USD [SI=F] * USD/TRY [TRY=X]) / 31.1034768
        if (norm == "GUMUS" || norm == "GRAM-GUMUS" || norm == "GRAM GUMUS" || norm == "GRAM_GUMUS" ||
            norm == "GRAM-GUMUS.TL" || norm == "GUMUS.TL" || norm == "XAGTRY" || norm == "XAG-TRY" ||
            symbol == "gümüş" || symbol == "GÜMÜŞ" || symbol == "gram gümüş" ||
            symbol == "GRAM GÜMÜŞ" || symbol == "GRAM-GÜMÜŞ" || symbol == "gram gumus") {
            return fetch_gram_precious_metal_tl_("SI=F", "GRAM-GUMUS.TL", 76.50, range);
        }

        // Ons Altın & Ons Gümüş (USD) kısayolları
        std::string query_sym = symbol;
        if (norm == "ONS" || norm == "ONS-ALTIN" || norm == "ONS ALTIN" || norm == "XAU" || norm == "XAUUSD" || norm == "XAU-USD") {
            query_sym = "GC=F";
        } else if (norm == "ONS-GUMUS" || norm == "ONS GUMUS" || norm == "XAG" || norm == "XAGUSD" || norm == "XAG-USD" ||
                   symbol == "ons gümüş" || symbol == "ONS GÜMÜŞ") {
            query_sym = "SI=F";
        }

        RealTimeAssetData data;
        data.symbol = query_sym;

        std::string encoded_symbol = url_encode_(query_sym);
        std::string url = std::format("https://query1.finance.yahoo.com/v8/finance/chart/{}?interval=1d&range={}",
                                      encoded_symbol, range);

        std::string response = http_get_(url);

        if (response.empty()) {
            data.is_live = false;
            data.error_message = "No network response from market API. Using robust financial simulation fallback.";
            populate_fallback_asset_(data);
            compute_analytics_(data);
            return data;
        }

        // Parse regularMarketPrice
        double price = extract_double_field_(response, "\"regularMarketPrice\":");
        if (price <= 0.0) {
            price = extract_double_field_(response, "\"chartPreviousClose\":");
        }

        // Parse close array: "close":[ ... ]
        auto closes = extract_double_array_(response, "\"close\":[");
        // Parse volume array: "volume":[ ... ]
        auto volumes = extract_double_array_(response, "\"volume\":[");

        if (closes.empty() || price <= 0.0) {
            data.is_live = false;
            data.error_message = "Failed to parse API JSON. Using simulated fallback.";
            populate_fallback_asset_(data);
        } else {
            data.current_price = price;
            data.historical_prices = std::move(closes);
            data.historical_volumes = std::move(volumes);
            data.is_live = true;
        }

        compute_analytics_(data);
        return data;
    }

    // Has Gram Altın / Gram Gümüş (TL) canlı sentezleyici: (Ons USD * TRY=X) / 31.1034768
    RealTimeAssetData fetch_gram_precious_metal_tl_(const std::string& ounce_symbol,
                                                    const std::string& out_symbol,
                                                    double fallback_price,
                                                    const std::string& range = "1mo") {
        constexpr double TROY_OUNCE_GRAMS = 31.1034768;
        auto ons = fetch_asset(ounce_symbol, range);
        auto usdtry = fetch_asset("TRY=X", range);

        RealTimeAssetData gram;
        gram.symbol = out_symbol;

        if (ons.is_live && usdtry.is_live && ons.current_price > 0.0 && usdtry.current_price > 0.0) {
            gram.current_price = (ons.current_price * usdtry.current_price) / TROY_OUNCE_GRAMS;
            size_t n = std::min(ons.historical_prices.size(), usdtry.historical_prices.size());
            if (n >= 2) {
                gram.historical_prices.reserve(n);
                size_t off_ons = ons.historical_prices.size() - n;
                size_t off_fx  = usdtry.historical_prices.size() - n;
                for (size_t i = 0; i < n; ++i) {
                    double g_p = (ons.historical_prices[off_ons + i] * usdtry.historical_prices[off_fx + i]) / TROY_OUNCE_GRAMS;
                    gram.historical_prices.push_back(g_p);
                }
                if (ons.historical_volumes.size() >= n) {
                    size_t off_v = ons.historical_volumes.size() - n;
                    for (size_t i = 0; i < n; ++i) {
                        gram.historical_volumes.push_back(ons.historical_volumes[off_v + i]);
                    }
                }
            }
            gram.is_live = true;
        } else {
            gram.current_price = fallback_price;
            populate_fallback_asset_(gram);
            gram.is_live = false;
        }

        compute_analytics_(gram);
        return gram;
    }

    // Fetch live US Treasury yields (^IRX=3M, ^FVX=5Y, ^TNX=10Y, ^TYX=30Y)
    TreasuryYieldCurveData fetch_treasury_yields() {
        TreasuryYieldCurveData curve;

        auto fetch_rate = [this](const std::string& sym, double default_rate) -> double {
            std::string url = std::format("https://query1.finance.yahoo.com/v8/finance/chart/{}?interval=1d&range=5d",
                                          url_encode_(sym));
            std::string resp = http_get_(url);
            if (!resp.empty()) {
                double p = extract_double_field_(resp, "\"regularMarketPrice\":");
                if (p > 0.0) {
                    return p / 100.0; // Tickers are in percentage points, e.g. 4.25 -> 0.0425
                }
            }
            return default_rate;
        };

        double y3m = fetch_rate("^IRX", 0.0460);
        double y5y = fetch_rate("^FVX", 0.0410);
        double y10y = fetch_rate("^TNX", 0.0425);
        double y30y = fetch_rate("^TYX", 0.0455);

        curve.three_month_yield = y3m;
        curve.five_year_yield = y5y;
        curve.ten_year_yield = y10y;
        curve.thirty_year_yield = y30y;
        curve.is_live = true;

        return curve;
    }

    struct LiveHeadline {
        std::string title;
        std::string publisher;
    };

    // Fetch live financial news headlines from Yahoo Finance search API
    std::vector<LiveHeadline> fetch_live_headlines(const std::string& query, int count = 2) {
        std::vector<LiveHeadline> headlines;
        std::string url = std::format("https://query2.finance.yahoo.com/v1/finance/search?q={}&newsCount={}",
                                      url_encode_(query), count);
        std::string resp = http_get_(url);
        if (resp.empty()) return headlines;

        size_t news_pos = resp.find("\"news\":[");
        if (news_pos == std::string::npos) return headlines;

        size_t cur = news_pos;
        while (headlines.size() < static_cast<size_t>(count)) {
            size_t title_pos = resp.find("\"title\":\"", cur);
            if (title_pos == std::string::npos) break;
            title_pos += 9;
            size_t title_end = resp.find("\"", title_pos);
            if (title_end == std::string::npos) break;

            std::string title = resp.substr(title_pos, title_end - title_pos);

            std::string publisher = "Finans Medyası";
            size_t pub_pos = resp.find("\"publisher\":\"", title_end);
            if (pub_pos != std::string::npos && pub_pos - title_end < 150) {
                pub_pos += 13;
                size_t pub_end = resp.find("\"", pub_pos);
                if (pub_end != std::string::npos) {
                    publisher = resp.substr(pub_pos, pub_end - pub_pos);
                }
            }

            headlines.push_back({std::move(title), std::move(publisher)});
            cur = title_end + 1;
        }
        return headlines;
    }

private:
    static size_t write_callback_(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    std::string http_get_(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string read_buffer;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                read_buffer.clear();
            }
            curl_easy_cleanup(curl);
        }
        return read_buffer;
    }

    std::string url_encode_(const std::string& value) {
        std::ostringstream escaped;
        for (char c : value) {
            if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else if (c == '^') {
                escaped << "%5E";
            } else {
                escaped << '%' << std::hex << std::uppercase << (static_cast<int>(c) & 0xff);
            }
        }
        return escaped.str();
    }

    double extract_double_field_(const std::string& json, const std::string& field) {
        size_t pos = json.find(field);
        if (pos == std::string::npos) return -1.0;
        pos += field.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':')) {
            ++pos;
        }
        size_t end_pos = pos;
        while (end_pos < json.length() && (isdigit(json[end_pos]) || json[end_pos] == '.' || json[end_pos] == '-')) {
            ++end_pos;
        }
        if (end_pos > pos) {
            try {
                return std::stod(json.substr(pos, end_pos - pos));
            } catch (...) {}
        }
        return -1.0;
    }

    std::vector<double> extract_double_array_(const std::string& json, const std::string& key) {
        std::vector<double> result;
        size_t pos = json.find(key);
        if (pos == std::string::npos) return result;
        pos += key.length();
        size_t end_pos = json.find(']', pos);
        if (end_pos == std::string::npos) return result;

        std::string array_str = json.substr(pos, end_pos - pos);
        std::stringstream ss(array_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
            // Trim whitespace
            token.erase(remove_if(token.begin(), token.end(), isspace), token.end());
            if (token != "null" && !token.empty()) {
                try {
                    result.push_back(std::stod(token));
                } catch (...) {}
            }
        }
        return result;
    }

    // Chapter 5: Apply Numeric Algorithms (VWAP & Log Returns)
    void compute_analytics_(RealTimeAssetData& data) {
        if (data.historical_prices.size() < 2) return;

        // 1. 30-Calendar-Day (22-Trading-Day) Rolling VWAP calculation
        if (!data.historical_volumes.empty() &&
            data.historical_volumes.size() == data.historical_prices.size()) {
            size_t n = data.historical_prices.size();
            size_t lookback = std::min(n, size_t{22});
            auto p_start = data.historical_prices.cend() - static_cast<std::ptrdiff_t>(lookback);
            auto v_start = data.historical_volumes.cend() - static_cast<std::ptrdiff_t>(lookback);

            double total_volume = std::accumulate(v_start, data.historical_volumes.cend(), 0.0);
            if (total_volume > 0.0) {
                double raw_wgt = std::inner_product(v_start, data.historical_volumes.cend(),
                                                    p_start, 0.0);
                data.vwap = raw_wgt / total_volume;
            } else {
                data.vwap = data.current_price;
            }
        } else {
            data.vwap = data.current_price;
        }

        // 2. Log Returns calculation via std::adjacent_difference (Chapter 5, Page 190)
        std::vector<double> log_prices(data.historical_prices.size());
        std::ranges::transform(data.historical_prices, log_prices.begin(), [](double p) {
            return std::log(std::max(p, 0.0001));
        });

        std::vector<double> adj_diff(log_prices.size());
        std::adjacent_difference(log_prices.begin(), log_prices.end(), adj_diff.begin());

        if (adj_diff.size() > 1) {
            data.log_returns.assign(adj_diff.begin() + 1, adj_diff.end());

            // 3. Historical Annualized Volatility: sigma = sqrt(252) * sample_std_dev
            double mean_rtn = std::accumulate(data.log_returns.begin(), data.log_returns.end(), 0.0) /
                              data.log_returns.size();
            double sum_sq = 0.0;
            for (double r : data.log_returns) {
                sum_sq += (r - mean_rtn) * (r - mean_rtn);
            }
            double var = sum_sq / (data.log_returns.size() - 1);
            data.annualized_volatility = std::sqrt(var * 252.0);
        }
    }

    void populate_fallback_asset_(RealTimeAssetData& data) {
        if (data.symbol == "SPY") data.current_price = 582.50;
        else if (data.symbol == "AAPL") data.current_price = 231.40;
        else if (data.symbol == "MSFT") data.current_price = 420.10;
        else if (data.symbol == "NVDA") data.current_price = 138.00;
        else if (data.symbol == "THYAO.IS") data.current_price = 267.00;
        else if (data.symbol == "ASELS.IS") data.current_price = 334.50;
        else if (data.symbol == "TUPRS.IS") data.current_price = 142.00;
        else if (data.symbol == "BIMAS.IS") data.current_price = 480.00;
        else if (data.symbol == "GARAN.IS") data.current_price = 115.00;
        else if (data.symbol == "EREGL.IS") data.current_price = 49.50;
        else if (data.symbol == "RHM.DE") data.current_price = 1056.00;
        else if (data.symbol == "SAP.DE") data.current_price = 197.36;
        else if (data.symbol == "SIE.DE") data.current_price = 185.20;
        else if (data.symbol == "ALV.DE") data.current_price = 295.40;
        else data.current_price = 100.0;

        // Generate 30 days of representative price data based on book values
        data.historical_prices = {
            25.5, 28.0, 30.5, 31.0, 27.5, 31.0, 29.5, 28.5, 37.5, 33.5,
            25.5, 31.5, 26.5, 29.5, 32.5, 34.5, 28.5, 35.5, 28.5, 29.0,
            32.0, 23.5, 27.5, 33.5, 28.0, 28.0, 32.5, 31.5, 29.0, 34.5
        };
        // Scale to current price
        double base = data.historical_prices.back();
        for (auto& p : data.historical_prices) {
            p = (p / base) * data.current_price;
        }
        data.historical_volumes = {
            376000, 365000, 344000, 346000, 345000, 336000, 335000,
            339000, 340000, 340000, 343000, 367000, 374000, 350000,
            380000, 390000, 360000, 355000, 370000, 365000, 340000,
            330000, 345000, 360000, 375000, 385000, 390000, 360000,
            350000, 370000
        };
    }
};

} // namespace quant
