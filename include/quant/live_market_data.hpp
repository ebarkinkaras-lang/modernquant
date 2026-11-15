#pragma once

#ifndef BOOST_JSON_NO_LIB
#define BOOST_JSON_NO_LIB
#endif

#include "data_telemetry.hpp"
#include <algorithm>
#include <boost/json.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <curl/curl.h>
#include <format>
#include <iostream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace quant {

struct RealTimeAssetData {
  std::string symbol;
  std::string long_name;
  std::string currency_code{"USD"};
  std::string currency_display{"$"};
  std::string instrument_type{
      "EQUITY"}; // EQUITY, ETF, FUTURE, CURRENCY, CRYPTOCURRENCY, INDEX

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
  std::vector<double>
      historical_prices; // Split- & dividend-adjusted closes (adjclose)
  std::vector<double> historical_volumes;

  std::vector<double> log_returns;
  double vwap{0.0}; // 22-trading-day volume-weighted typical price
  double annualized_volatility{
      0.20}; // Regime-adaptive blend: 60% EWMA(0.94) + 40% Yang-Zhang/Sample
  double ewma_volatility{0.20};
  double sample_volatility{0.20};
  double atr_14{0.0}; // True 14-period Wilder OHLC Average True Range

  bool is_live{false};
  bool is_synthetic_metal{false};
  double underlying_ounce_usd{0.0};
  double underlying_usdtry{0.0};
  int64_t regular_market_time{0};
  std::chrono::system_clock::time_point fetched_at{};
  size_t raw_bar_count{0};
  size_t null_bars_skipped{0};
  size_t split_adjustments_applied{0};
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
  std::vector<size_t> indices_a;
  std::vector<size_t> indices_b;
};

class LiveMarketDataClient {
public:
  struct LiveHeadline {
    std::string title;
    std::string publisher;
    int64_t publish_time{0};
  };

  LiveMarketDataClient() { curl_global_init(CURL_GLOBAL_DEFAULT); }

  ~LiveMarketDataClient() {
    if (!cookie_file_path_.empty()) {
      std::remove(cookie_file_path_.c_str());
    }
    curl_global_cleanup();
  }

  // Normalizes UTF-8 Turkish/German/ASCII ticker input into canonical Yahoo
  // symbol
  static std::string normalize_symbol(const std::string &raw_input) {
    std::string s = ascii_fold_upper_(trim_(raw_input));
    if (s.empty())
      return "SPY";

    // Precious Metal Shortcuts (Gram Gold & Gram Silver in TL, Ounce Gold &
    // Silver in USD)
    if (s == "GRAM" || s == "ALTIN" || s == "GRAM-ALTIN" || s == "GRAM ALTIN" ||
        s == "GRAM_ALTIN" || s == "GRAM-ALTIN.TL" || s == "ALTIN.TL" ||
        s == "GAU" || s == "GAUTRY" || s == "GAU-TRY" || s == "XAUTRY") {
      return "GRAM-ALTIN.TL";
    }
    if (s == "GUMUS" || s == "GRAM-GUMUS" || s == "GRAM GUMUS" ||
        s == "GRAM_GUMUS" || s == "GRAM-GUMUS.TL" || s == "GUMUS.TL" ||
        s == "XAGTRY" || s == "XAG-TRY") {
      return "GRAM-GUMUS.TL";
    }
    if (s == "ONS" || s == "ONS-ALTIN" || s == "ONS ALTIN" || s == "XAU" ||
        s == "XAUUSD" || s == "XAU-USD") {
      return "GC=F";
    }
    if (s == "ONS-GUMUS" || s == "ONS GUMUS" || s == "XAG" || s == "XAGUSD" ||
        s == "XAG-USD") {
      return "SI=F";
    }

    // Auto-resolve common bare BIST tickers (e.g. "thyao" -> "THYAO.IS")
    static const std::unordered_set<std::string> known_bist = {
        "THYAO", "ASELS", "TUPRS", "BIMAS", "GARAN", "EREGL", "AKBNK", "KCHOL",
        "SISE",  "SAHOL", "FROTO", "PGSUS", "MGROS", "ENKAI", "TAVHL", "CCOLA",
        "ALARK", "ASTOR", "KRDMD", "PETKM", "TCELL", "TTKOM", "ISCTR", "YKBNK",
        "HALKB", "VAKBN", "TOASO", "ARCLK", "SOKM",  "EKGYO", "SASA",  "HEKTS",
        "GUBRF", "KOZAL", "KOZAA", "IPEKE", "ODAS",  "CANTE", "VESTL", "VESBE",
        "ISGYO", "CIMSA", "AKSEN", "OYAKC", "KONTR", "EUPWR", "GESAN", "SMRTG",
        "ALFAS", "DOAS",  "OTKAR", "ULKER", "AEFES", "AGHOL", "BRSAN", "MAVI",
        "MPARK", "TABGD", "MIATK", "YEOTK", "TSKB",  "SKBNK", "ALBRK"};
    if (s.find('.') == std::string::npos && s.find('-') == std::string::npos &&
        s.find('=') == std::string::npos && !s.starts_with("^")) {
      if (known_bist.contains(s)) {
        return s + ".IS";
      }
    }

    return s;
  }

  void clear_caches() {
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      asset_cache_.clear();
      fx_cache_.clear();
      has_cached_yields_ = false;
    }
    {
      std::lock_guard<std::mutex> lock(crumb_mutex_);
      yahoo_crumb_.clear();
      last_crumb_attempt_at_ = {};
    }
    chart_breaker_.reset();
  }

  static std::string currency_display_for(const std::string &iso_ccy,
                                          const std::string &symbol = "") {
    std::string u = ascii_fold_upper_(iso_ccy);
    if (u == "TRY" || u == "TL" || symbol.ends_with(".IS") ||
        symbol.ends_with(".TL"))
      return "TL";
    if (u == "EUR" || symbol.ends_with(".DE"))
      return "EUR";
    if (u == "GBP")
      return "GBP";
    if (u == "CHF")
      return "CHF";
    return "$";
  }

  static std::string infer_currency_code(const std::string &symbol,
                                         const std::string &meta_ccy = "") {
    if (!meta_ccy.empty()) {
      std::string u = ascii_fold_upper_(meta_ccy);
      if (u == "TL")
        return "TRY";
      return u;
    }
    std::string sym_u = ascii_fold_upper_(symbol);
    if (sym_u.ends_with(".IS") || sym_u.ends_with(".TL"))
      return "TRY";
    if (sym_u.ends_with(".DE"))
      return "EUR";
    return "USD";
  }

  static constexpr std::chrono::seconds kAssetCacheTtl{60};
  static constexpr std::chrono::seconds kFxCacheTtl{60};
  static constexpr std::chrono::seconds kYieldCacheTtl{300};

  // Resolve bare local tickers intelligently (.IS -> .DE) when primary exchange
// returns no quote
RealTimeAssetData fetch_asset_smart(const std::string &symbol,
                                    const std::string &range = "6mo",
                                    bool force_refresh = false) {
  std::string norm_sym = normalize_symbol(symbol);
  auto data = fetch_asset(norm_sym, range, force_refresh);
  if ((!data.is_live || data.current_price <= 0.0) &&
      norm_sym.find('.') == std::string::npos &&
      norm_sym.find('-') == std::string::npos &&
      norm_sym.find('=') == std::string::npos && !norm_sym.starts_with("^")) {
    auto bist_try = fetch_asset(norm_sym + ".IS", range, force_refresh);
    if (bist_try.is_live && bist_try.current_price > 0.0) {
      return bist_try;
    }
    auto xetra_try = fetch_asset(norm_sym + ".DE", range, force_refresh);
    if (xetra_try.is_live && xetra_try.current_price > 0.0) {
      return xetra_try;
    }
  }
  return data;
}

// Fetch real-time & historical market data from Yahoo Finance (default 6mo
// for robust vol/RSI/ATR)
RealTimeAssetData fetch_asset(const std::string &symbol,
                              const std::string &range = "6mo",
                              bool force_refresh = false) {
  std::string norm_sym = normalize_symbol(symbol);
  std::string cache_key = norm_sym + "|" + range;
  const auto now_steady = std::chrono::steady_clock::now();

  if (!force_refresh) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (auto it = asset_cache_.find(cache_key); it != asset_cache_.end()) {
      if ((now_steady - it->second.fetched_at) < kAssetCacheTtl) {
        DataQualityTelemetry::record_fetch(
            "Yahoo v8 Chart", "query1.finance.yahoo.com/v8/finance/chart",
            it->second.data.is_live, std::chrono::milliseconds(0), true,
            it->second.data.is_live ? "" : it->second.data.error_message,
            it->second.data.is_live ? 200 : 0);
        return it->second.data;
      }
    }
  }

if (norm_sym == "GRAM-ALTIN.TL") {
  auto res = fetch_gram_precious_metal_tl_("GC=F", "GRAM-ALTIN.TL",
                                           "24 Ayar Has Gram Altın (TL)",
                                           6448.0, range, force_refresh);
  if (res.is_live) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    asset_cache_[cache_key] = {res, std::chrono::steady_clock::now()};
  }
  return res;
}
if (norm_sym == "GRAM-GUMUS.TL") {
  auto res = fetch_gram_precious_metal_tl_("SI=F", "GRAM-GUMUS.TL",
                                           "Has Gram Gümüş (TL)", 76.50, range,
                                           force_refresh);
  if (res.is_live) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    asset_cache_[cache_key] = {res, std::chrono::steady_clock::now()};
  }
  return res;
}

RealTimeAssetData data;
data.symbol = norm_sym;
data.currency_code = infer_currency_code(norm_sym);
data.currency_display = currency_display_for(data.currency_code, norm_sym);
data.fetched_at = std::chrono::system_clock::now();

std::string url = std::format("https://query1.finance.yahoo.com/v8/finance/"
                              "chart/{}?interval=1d&range={}",
                              url_encode(norm_sym), range);

long http_code = 0;
std::string http_err;
auto t0 = std::chrono::steady_clock::now();
std::string response = http_get(url, &http_code, &http_err);
auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - t0);

if (response.empty()) {
  // Network completely unreachable -> mark is_live = false explicitly
  data.is_live = false;
  data.error_message = "Piyasa API ağına ulaşılamadı (Çevrimdışı mod).";
  DataQualityTelemetry::record_fetch(
      "Yahoo v8 Chart", "query1.finance.yahoo.com/v8/finance/chart", false,
      latency, false, http_err.empty() ? "Network unreachable" : http_err,
      http_code);
  populate_offline_fallback_(data);
  compute_analytics(data);
  return data;
}

if (!parse_yahoo_chart_json(response, data)) {
  // Invalid symbol (HTTP 404 or chart.error != null) -> do NOT fabricate
  // prices for unknown tickers!
  data.is_live = false;
  if (data.error_message.empty()) {
    data.error_message = std::format(
        "'{}' sembolü için geçerli borsa verisi bulunamadı (HTTP {}).",
        norm_sym, http_code);
  }
  DataQualityTelemetry::record_fetch(
      "Yahoo v8 Chart", "query1.finance.yahoo.com/v8/finance/chart", false,
      latency, false, data.error_message, http_code);
  return data;
}

DataQualityTelemetry::record_fetch("Yahoo v8 Chart",
                                   "query1.finance.yahoo.com/v8/finance/chart",
                                   true, latency, false, "",
                                   http_code > 0 ? http_code : 200);

data.fetched_at = std::chrono::system_clock::now();
compute_analytics(data);
if (data.is_live) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  asset_cache_[cache_key] = {data, std::chrono::steady_clock::now()};
}
return data;
}

// Public static parser for deterministic unit testing of null-alignment,
// adjclose & split handling
static bool parse_yahoo_chart_json(const std::string &json_str,
                                   RealTimeAssetData &data) {
  boost::system::error_code ec;
  boost::json::value root = boost::json::parse(json_str, ec);
  if (ec || !root.is_object()) {
    data.error_message = "API JSON ayrıştırma hatası.";
    return false;
  }

  const auto &root_obj = root.as_object();
  auto chart_it = root_obj.find("chart");
  if (chart_it == root_obj.end() || !chart_it->value().is_object())
    return false;
  const auto &chart_obj = chart_it->value().as_object();

  if (auto err_it = chart_obj.find("error");
      err_it != chart_obj.end() && !err_it->value().is_null()) {
    if (err_it->value().is_object()) {
      if (auto d_it = err_it->value().as_object().find("description");
          d_it != err_it->value().as_object().end() &&
          d_it->value().is_string()) {
        data.error_message = std::string(d_it->value().as_string());
      }
    }
    return false;
  }

  auto res_it = chart_obj.find("result");
  if (res_it == chart_obj.end() || !res_it->value().is_array() ||
      res_it->value().as_array().empty()) {
    return false;
  }
  if (!res_it->value().as_array()[0].is_object())
    return false;
  const auto &res0 = res_it->value().as_array()[0].as_object();

  int64_t gmt_offset = 0;
  int64_t reg_time = 0;
  double reg_price = 0.0;
  double meta_prev = 0.0;
  double chart_prev = 0.0;

  if (auto meta_it = res0.find("meta");
      meta_it != res0.end() && meta_it->value().is_object()) {
    const auto &meta = meta_it->value().as_object();
    if (auto s = get_string_(meta, "symbol"); !s.empty())
      data.symbol = s;
    if (auto ln = get_string_(meta, "longName"); !ln.empty())
      data.long_name = ln;
    else if (auto sn = get_string_(meta, "shortName"); !sn.empty())
      data.long_name = sn;

    std::string meta_ccy = get_string_(meta, "currency");
    data.currency_code = infer_currency_code(data.symbol, meta_ccy);
    data.currency_display =
        currency_display_for(data.currency_code, data.symbol);

    if (auto itype = get_string_(meta, "instrumentType"); !itype.empty()) {
      data.instrument_type = ascii_fold_upper_(itype);
    }

    gmt_offset = get_int64_(meta, "gmtoffset", 0);
    reg_time = get_int64_(meta, "regularMarketTime", 0);
    data.regular_market_time = reg_time;
    reg_price = get_double_(meta, "regularMarketPrice", 0.0);
    meta_prev = get_double_(meta, "previousClose", 0.0);
    chart_prev = get_double_(meta, "chartPreviousClose", 0.0);
    data.previous_close = (meta_prev > 0.0) ? meta_prev : chart_prev;
    data.fifty_two_week_high = get_double_(meta, "fiftyTwoWeekHigh", 0.0);
    data.fifty_two_week_low = get_double_(meta, "fiftyTwoWeekLow", 0.0);
  }

  const boost::json::array *ts_arr = nullptr;
  if (auto ts_it = res0.find("timestamp");
      ts_it != res0.end() && ts_it->value().is_array()) {
    ts_arr = &ts_it->value().as_array();
  }

  const boost::json::array *open_arr = nullptr;
  const boost::json::array *high_arr = nullptr;
  const boost::json::array *low_arr = nullptr;
  const boost::json::array *close_arr = nullptr;
  const boost::json::array *vol_arr = nullptr;
  const boost::json::array *adj_arr = nullptr;

  if (auto ind_it = res0.find("indicators");
      ind_it != res0.end() && ind_it->value().is_object()) {
    const auto &ind = ind_it->value().as_object();
    if (auto q_it = ind.find("quote");
        q_it != ind.end() && q_it->value().is_array() &&
        !q_it->value().as_array().empty() &&
        q_it->value().as_array()[0].is_object()) {
      const auto &q0 = q_it->value().as_array()[0].as_object();
      if (auto it = q0.find("open"); it != q0.end() && it->value().is_array())
        open_arr = &it->value().as_array();
      if (auto it = q0.find("high"); it != q0.end() && it->value().is_array())
        high_arr = &it->value().as_array();
      if (auto it = q0.find("low"); it != q0.end() && it->value().is_array())
        low_arr = &it->value().as_array();
      if (auto it = q0.find("close"); it != q0.end() && it->value().is_array())
        close_arr = &it->value().as_array();
      if (auto it = q0.find("volume"); it != q0.end() && it->value().is_array())
        vol_arr = &it->value().as_array();
    }
    if (auto a_it = ind.find("adjclose");
        a_it != ind.end() && a_it->value().is_array() &&
        !a_it->value().as_array().empty() &&
        a_it->value().as_array()[0].is_object()) {
      const auto &a0 = a_it->value().as_array()[0].as_object();
      if (auto it = a0.find("adjclose");
          it != a0.end() && it->value().is_array())
        adj_arr = &it->value().as_array();
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
  data.raw_bar_count = n;
  data.null_bars_skipped = 0;
  data.split_adjustments_applied = 0;
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

  const int64_t reg_day_key =
      (reg_time > 0) ? ((reg_time + gmt_offset) / 86400) : -1;

  // Synchronized row-by-row extraction: guarantees identical array lengths
  // across all columns
  for (size_t i = 0; i < n; ++i) {
    int64_t ts = elem_int64_(ts_arr, i, static_cast<int64_t>(i) * 86400);
    int64_t day_key = (ts + gmt_offset) / 86400;

    double raw_c = elem_double_(close_arr, i, -1.0);
    double adj_c = elem_double_(adj_arr, i, -1.0);
    double o_probe = elem_double_(open_arr, i, -1.0);
    double h_probe = elem_double_(high_arr, i, -1.0);
    double l_probe = elem_double_(low_arr, i, -1.0);
    double v_probe = elem_double_(vol_arr, i, 0.0);

    if (raw_c <= 0.0) {
      // Recover live active intraday bar when Yahoo leaves today's final
      // close null alongside regularMarketPrice
      bool is_trailing_live_bar =
          (i + 1 == n) && (reg_price > 0.0) && (reg_day_key > 0) &&
          (day_key == reg_day_key) &&
          (o_probe > 0.0 || h_probe > 0.0 || l_probe > 0.0 || v_probe > 0.0);
      if (is_trailing_live_bar) {
        raw_c = reg_price;
        adj_c = reg_price;
      } else {
        // Skip holiday / pre-market / unopened null bar uniformly across all
        // arrays
        data.null_bars_skipped++;
        continue;
      }
    }

    if (adj_c <= 0.0)
      adj_c = raw_c;

    double o = (o_probe > 0.0) ? o_probe : raw_c;
    double h = (h_probe > 0.0) ? h_probe : raw_c;
    double l = (l_probe > 0.0) ? l_probe : raw_c;
    if (h < std::max(o, raw_c))
      h = std::max(o, raw_c);
    if (l <= 0.0 || l > std::min(o, raw_c))
      l = std::min(o, raw_c);

    double v = std::max(0.0, v_probe);

    // Rescale O/H/L by the dividend/split adjustment factor (adj_c / raw_c)
    double adj_factor = (raw_c > 0.0) ? (adj_c / raw_c) : 1.0;

    // Deduplicate if Yahoo appends an extra live tick on the exact same
    // trading day
    if (!data.day_keys.empty() && data.day_keys.back() == day_key) {
      data.timestamps.back() = ts;
      data.historical_highs.back() =
          std::max(data.historical_highs.back(), h * adj_factor);
      data.historical_lows.back() =
          std::min(data.historical_lows.back(), l * adj_factor);
      data.historical_raw_closes.back() = raw_c;
      data.historical_prices.back() = adj_c;
      data.historical_volumes.back() =
          std::max(data.historical_volumes.back(), v);
      continue;
    }

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
    if (reg_price > 0.0) {
      data.current_price = reg_price;
      data.previous_close = (meta_prev > 0.0) ? meta_prev : chart_prev;
      data.is_live = true;
      return true;
    }
    return false;
  }

  // BIST unadjusted bonus-share / split (bedelsiz bölünme) guard:
  // BIST daily circuit-breaker limit is strictly +/-10% (|log_ret| <=
  // 0.1054). An overnight drop > 28% (log_ret < -0.33) in a BIST equity is an
  // unadjusted split.
  if (data.currency_code == "TRY" && data.instrument_type == "EQUITY" &&
      data.historical_prices.size() >= 3) {
    for (size_t i = 1; i < data.historical_prices.size(); ++i) {
      double prev_p = data.historical_prices[i - 1];
      double curr_p = data.historical_prices[i];
      if (prev_p > 0.0 && curr_p > 0.0) {
        double lr = std::log(curr_p / prev_p);
        if (lr < -0.33 || lr > 0.35) {
          data.split_adjustments_applied++;
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

  data.current_price =
      (reg_price > 0.0) ? reg_price : data.historical_prices.back();
  if (meta_prev > 0.0) {
    data.previous_close = meta_prev;
  } else if (data.historical_prices.size() >= 2) {
    bool last_is_today =
        (reg_day_key > 0 && data.day_keys.back() == reg_day_key) ||
        (reg_price > 0.0 &&
         std::abs(reg_price - data.historical_prices.back()) <=
             data.historical_prices.back() * 1e-4);
    data.previous_close =
        last_is_today
            ? data.historical_prices[data.historical_prices.size() - 2]
            : data.historical_prices.back();
  } else {
    data.previous_close =
        (chart_prev > 0.0) ? chart_prev : data.historical_prices.back();
  }
  data.is_live = true;
  return true;
}

// Compute 22-day Typical-Price VWAP, Wilder 14-day OHLC ATR, EWMA(0.94) &
// Yang-Zhang Volatility
static void compute_analytics(RealTimeAssetData &data) {
  const size_t n = data.historical_prices.size();
  if (n == 0)
    return;
  if (n == 1) {
    data.vwap = data.current_price;
    data.atr_14 = data.current_price * 0.02;
    return;
  }

  // 1. 22-Trading-Day (~1 Calendar Month) Institutional Rolling VWAP using
  // Typical Price (H+L+C)/3
  size_t vwap_lookback = std::min(n, size_t{22});
  size_t vwap_start = n - vwap_lookback;
  double sum_tp_vol = 0.0;
  double sum_vol = 0.0;
  double sum_tp = 0.0;

  for (size_t i = vwap_start; i < n; ++i) {
    double c = data.historical_prices[i];
    double h =
        (i < data.historical_highs.size()) ? data.historical_highs[i] : c;
    double l = (i < data.historical_lows.size()) ? data.historical_lows[i] : c;
    double tp = (h + l + c) / 3.0;
    double v =
        (i < data.historical_volumes.size()) ? data.historical_volumes[i] : 0.0;
    sum_tp += tp;
    if (v > 0.0) {
      sum_tp_vol += tp * v;
      sum_vol += v;
    }
  }
  // For FX / Index tickers where Yahoo reports 0 volume, use 22-day
  // Equal-Weighted Typical Price (TWAP)
  if (sum_vol > 0.0) {
    data.vwap = sum_tp_vol / sum_vol;
  } else {
    data.vwap = sum_tp / static_cast<double>(vwap_lookback);
  }

  // 2. Log Returns via std::adjacent_difference
  std::vector<double> log_prices(n);
  std::ranges::transform(data.historical_prices, log_prices.begin(),
                         [](double p) { return std::log(std::max(p, 1e-6)); });

  std::vector<double> adj_diff(n);
  std::adjacent_difference(log_prices.begin(), log_prices.end(),
                           adj_diff.begin());
  data.log_returns.assign(adj_diff.begin() + 1, adj_diff.end());

  // 3. Volatility Estimation: Sample Close-to-Close + RiskMetrics
  // EWMA(lambda=0.94) + Garman-Klass/Parkinson OHLC
  const size_t m = data.log_returns.size();
  if (m >= 2) {
    double mean_r =
        std::accumulate(data.log_returns.begin(), data.log_returns.end(), 0.0) /
        static_cast<double>(m);
    double sum_sq = 0.0;
    for (double r : data.log_returns) {
      double d = r - mean_r;
      sum_sq += d * d;
    }
    double sample_var = sum_sq / static_cast<double>(m - 1);
    data.sample_volatility = std::sqrt(std::max(sample_var, 1e-8) * 252.0);

    // RiskMetrics EWMA variance (lambda = 0.94) initialized with first 15
    // bars' sample variance
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

    // Parkinson / Garman-Klass range-based volatility check when valid H/L
    // exist
    double gk_sum = 0.0;
    size_t gk_cnt = 0;
    if (data.historical_highs.size() == n && data.historical_lows.size() == n &&
        data.historical_opens.size() == n) {
      size_t start_idx = (n > 63) ? (n - 63) : 1;
      for (size_t i = start_idx; i < n; ++i) {
        double o = data.historical_opens[i];
        double h = data.historical_highs[i];
        double l = data.historical_lows[i];
        double c = data.historical_prices[i];
        if (o > 0.0 && h >= l && l > 0.0 && c > 0.0) {
          double hl = std::log(h / l);
          double co = std::log(c / o);
          double gk_daily =
              0.5 * hl * hl - (2.0 * std::log(2.0) - 1.0) * co * co;
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
      // Blend close-to-close (captures overnight gaps) with Garman-Klass
      // intraday range
      struct_vol = 0.65 * data.sample_volatility + 0.35 * gk_vol;
    }

    // Regime-adaptive institutional volatility: 60% EWMA + 40% structural
    data.annualized_volatility =
        std::clamp(0.60 * data.ewma_volatility + 0.40 * struct_vol, 0.04, 1.50);
  }

  // 4. Wilder's 14-Period Average True Range (ATR) on OHLC
  if (data.historical_highs.size() == n && data.historical_lows.size() == n) {
    std::vector<double> tr(n, 0.0);
    tr[0] = std::max(0.0, data.historical_highs[0] - data.historical_lows[0]);
    for (size_t i = 1; i < n; ++i) {
      double hl = data.historical_highs[i] - data.historical_lows[i];
      double hc =
          std::abs(data.historical_highs[i] - data.historical_prices[i - 1]);
      double lc =
          std::abs(data.historical_lows[i] - data.historical_prices[i - 1]);
      tr[i] = std::max({hl, hc, lc});
    }
    constexpr size_t period = 14;
    if (n <= period) {
      data.atr_14 =
          std::accumulate(tr.begin(), tr.end(), 0.0) / static_cast<double>(n);
    } else {
      double atr =
          std::accumulate(tr.begin() + 1, tr.begin() + 1 + period, 0.0) /
          static_cast<double>(period);
      for (size_t i = period + 1; i < n; ++i) {
        atr = (atr * (period - 1) + tr[i]) / static_cast<double>(period);
      }
      data.atr_14 = atr;
    }
  }
  if (data.atr_14 <= 0.0) {
    data.atr_14 = data.current_price *
                  (data.annualized_volatility / std::sqrt(252.0)) * 1.25;
  }

  if (data.fifty_two_week_high <= 0.0) {
    data.fifty_two_week_high = *std::max_element(data.historical_prices.begin(),
                                                 data.historical_prices.end());
  }
  if (data.fifty_two_week_low <= 0.0) {
    data.fifty_two_week_low = *std::min_element(data.historical_prices.begin(),
                                                data.historical_prices.end());
  }
}

// Strict date-aligned inner join (or 1-day holiday forward-fill when
// allow_ffill = true)
static AlignedPairSeries align_series_by_date(const RealTimeAssetData &a,
                                              const RealTimeAssetData &b,
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
      out.volumes_a.push_back(off_a + i < a.historical_volumes.size()
                                  ? a.historical_volumes[off_a + i]
                                  : 0.0);
      out.volumes_b.push_back(off_b + i < b.historical_volumes.size()
                                  ? b.historical_volumes[off_b + i]
                                  : 0.0);
      out.indices_a.push_back(off_a + i);
      out.indices_b.push_back(off_b + i);
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
      out.volumes_a.push_back(
          i < a.historical_volumes.size() ? a.historical_volumes[i] : 0.0);
      out.volumes_b.push_back(matched_j < b.historical_volumes.size()
                                  ? b.historical_volumes[matched_j]
                                  : 0.0);
      out.indices_a.push_back(i);
      out.indices_b.push_back(matched_j);
    }
  }
  return out;
}

// Live FX cross-rate converter (e.g. USD -> TRY, EUR -> TRY, EUR -> USD) with
// TTL cache
double get_fx_rate(const std::string &from_ccy, const std::string &to_ccy,
                   bool force_refresh = false) {
  return get_fx_entry_(from_ccy, to_ccy, force_refresh).rate;
}

// Fetch live US Treasury yields (^IRX=3M, ^FVX=5Y, ^TNX=10Y, ^TYX=30Y) with
// honest is_live flag & TTL
TreasuryYieldCurveData fetch_treasury_yields(bool force_refresh = false) {
  const auto now_steady = std::chrono::steady_clock::now();
  if (!force_refresh) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (has_cached_yields_ &&
        (now_steady - yields_fetched_at_) < kYieldCacheTtl) {
      DataQualityTelemetry::record_fetch(
          "Yahoo Treasury Yields",
          "query1.finance.yahoo.com/v8/finance/chart/^IRX,^FVX,^TNX,^TYX",
          cached_yields_.is_live, std::chrono::milliseconds(0), true, "",
          cached_yields_.is_live ? 200 : 0);
      return cached_yields_;
    }
  }

  TreasuryYieldCurveData curve;
  int live_hits = 0;
  auto t0 = std::chrono::steady_clock::now();
  auto fetch_rate = [this, &live_hits,
                     force_refresh](const std::string &sym,
                                    double default_rate) -> double {
    auto d = fetch_asset(sym, "5d", force_refresh);
    if (d.is_live && d.current_price > 0.0) {
      ++live_hits;
      return d.current_price / 100.0;
    }
    return default_rate;
  };

  curve.three_month_yield = fetch_rate("^IRX", 0.0430);
  curve.five_year_yield = fetch_rate("^FVX", 0.0410);
  curve.ten_year_yield = fetch_rate("^TNX", 0.0425);
  curve.thirty_year_yield = fetch_rate("^TYX", 0.0450);
  curve.is_live = (live_hits >= 2);
  auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0);

  DataQualityTelemetry::record_fetch(
      "Yahoo Treasury Yields",
      "query1.finance.yahoo.com/v8/finance/chart/^IRX,^FVX,^TNX,^TYX",
      curve.is_live, latency, false,
      curve.is_live ? ""
                    : std::format("Only {}/4 yield tickers live", live_hits),
      curve.is_live ? 200 : 503);

  if (curve.is_live) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_yields_ = curve;
    yields_fetched_at_ = std::chrono::steady_clock::now();
    has_cached_yields_ = true;
  }
  return curve;
}

static std::string decode_xml_entities_(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  auto append_utf8 = [&](uint32_t cp) {
    if (cp <= 0x7F) {
      out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
      out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  };
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '&') {
      auto semi = input.find(';', i + 1);
      if (semi != std::string_view::npos && semi - i <= 10) {
        std::string_view ent = input.substr(i + 1, semi - i - 1);
        if (ent == "amp") { out.push_back('&'); i = semi; continue; }
        if (ent == "lt") { out.push_back('<'); i = semi; continue; }
        if (ent == "gt") { out.push_back('>'); i = semi; continue; }
        if (ent == "quot") { out.push_back('"'); i = semi; continue; }
        if (ent == "apos") { out.push_back('\''); i = semi; continue; }
        if (ent == "nbsp") { out.push_back(' '); i = semi; continue; }
        if (!ent.empty() && ent[0] == '#') {
          uint32_t cp = 0;
          bool valid = true;
          if (ent.size() >= 2 && (ent[1] == 'x' || ent[1] == 'X')) {
            for (size_t k = 2; k < ent.size(); ++k) {
              char c = ent[k];
              if (c >= '0' && c <= '9') cp = cp * 16 + static_cast<uint32_t>(c - '0');
              else if (c >= 'a' && c <= 'f') cp = cp * 16 + static_cast<uint32_t>(10 + c - 'a');
              else if (c >= 'A' && c <= 'F') cp = cp * 16 + static_cast<uint32_t>(10 + c - 'A');
              else { valid = false; break; }
            }
          } else if (ent.size() >= 2) {
            for (size_t k = 1; k < ent.size(); ++k) {
              char c = ent[k];
              if (c >= '0' && c <= '9') cp = cp * 10 + static_cast<uint32_t>(c - '0');
              else { valid = false; break; }
            }
          } else {
            valid = false;
          }
          if (valid && cp > 0) {
            append_utf8(cp);
            i = semi;
            continue;
          }
        }
      }
    }
    out.push_back(input[i]);
  }
  return out;
}

static std::string extract_xml_tag_(std::string_view block,
                                    std::string_view tag_name) {
  std::string open_prefix = std::format("<{}", tag_name);
  std::string close_tag = std::format("</{}>", tag_name);
  size_t pos = 0;
  while (true) {
    pos = block.find(open_prefix, pos);
    if (pos == std::string_view::npos) return {};
    size_t after = pos + open_prefix.size();
    if (after < block.size() && (block[after] == '>' || std::isspace(static_cast<unsigned char>(block[after])))) {
      break;
    }
    pos = after;
  }
  size_t gt = block.find('>', pos + open_prefix.size());
  if (gt == std::string_view::npos) return {};
  size_t end = block.find(close_tag, gt + 1);
  if (end == std::string_view::npos || end <= gt + 1) return {};
  std::string_view inner = block.substr(gt + 1, end - (gt + 1));
  // Strip optional CDATA wrapper
  if (inner.size() >= 12 && inner.starts_with("<![CDATA[") && inner.ends_with("]]>")) {
    inner = inner.substr(9, inner.size() - 12);
  }
  return decode_xml_entities_(inner);
}

static int64_t parse_rss_pubdate_(std::string_view dt) {
  // Parses RFC-822 UTC date e.g. "Sun, 15 Nov 2026 10:28:14 GMT" or "+0000"
  std::istringstream iss{std::string(dt)};
  std::string tok;
  std::vector<std::string> parts;
  while (iss >> tok) parts.push_back(tok);
  int day = 0, month = 0, year = 0, hh = 0, mm = 0, ss = 0;
  int tz_offset_sec = 0;
  size_t i = (!parts.empty() && parts[0].back() == ',') ? 1 : 0;
  if (i + 3 >= parts.size()) return 0;
  try {
    day = std::stoi(parts[i]);
    const std::string& mon = parts[i + 1];
    static const std::unordered_map<std::string, int> months = {
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6},
        {"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };
    auto mit = months.find(mon.substr(0, 3));
    if (mit == months.end()) return 0;
    month = mit->second;
    year = std::stoi(parts[i + 2]);
    if (parts[i + 3].size() >= 8 && parts[i + 3][2] == ':' && parts[i + 3][5] == ':') {
      hh = std::stoi(parts[i + 3].substr(0, 2));
      mm = std::stoi(parts[i + 3].substr(3, 2));
      ss = std::stoi(parts[i + 3].substr(6, 2));
    }
    if (i + 4 < parts.size()) {
      const std::string& tz = parts[i + 4];
      if (tz.size() == 5 && (tz[0] == '+' || tz[0] == '-')) {
        int off_h = std::stoi(tz.substr(1, 2));
        int off_m = std::stoi(tz.substr(3, 2));
        tz_offset_sec = ((tz[0] == '-') ? -1 : 1) * (off_h * 3600 + off_m * 60);
      }
    }
  } catch (...) {
    return 0;
  }
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
  int y = year - (month <= 2 ? 1 : 0);
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(day) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const int64_t days = era * 146097 + static_cast<int64_t>(doe) - 719468;
  return days * 86400 + hh * 3600 + mm * 60 + ss - tz_offset_sec;
}

// Fetch live RSS 2.0 headlines (supports Yahoo Finance Ticker RSS & Google News RSS in TR/DE/EN)
std::vector<LiveHeadline> fetch_rss_headlines(const std::string &rss_url,
                                              int count = 6,
                                              bool strip_trailing_source = true) {
  std::vector<LiveHeadline> headlines;
  long http_code = 0;
  std::string http_err;
  std::string xml = http_get(rss_url, &http_code, &http_err);
  if (xml.empty()) return headlines;

  std::unordered_set<std::string> seen_titles;
  size_t pos = 0;
  while (static_cast<int>(headlines.size()) < count) {
    size_t item_start = xml.find("<item>", pos);
    if (item_start == std::string::npos) item_start = xml.find("<item ", pos);
    if (item_start == std::string::npos) break;
    size_t item_end = xml.find("</item>", item_start);
    if (item_end == std::string::npos) break;

    std::string_view item_block(xml.data() + item_start, item_end - item_start + 7);
    pos = item_end + 7;

    std::string title = extract_xml_tag_(item_block, "title");
    std::string publisher = extract_xml_tag_(item_block, "source");
    std::string pub_str = extract_xml_tag_(item_block, "pubDate");
    int64_t pub_time = parse_rss_pubdate_(pub_str);

    if (strip_trailing_source && !publisher.empty()) {
      std::string suffix = " - " + publisher;
      if (title.size() > suffix.size() && title.ends_with(suffix)) {
        title.resize(title.size() - suffix.size());
      }
    }
    if (publisher.empty()) {
      publisher = (rss_url.find("yahoo.com") != std::string::npos) ? "Yahoo Finance RSS" : "Google News RSS";
    }
    if (title.empty() || seen_titles.contains(title)) continue;
    seen_titles.insert(title);
    headlines.push_back({std::move(title), std::move(publisher), pub_time});
  }
  return headlines;
}

// Fetch live financial news headlines via Boost.JSON + Yahoo Ticker RSS fusion
std::vector<LiveHeadline> fetch_live_headlines(const std::string &query,
                                               int count = 5,
                                               bool allow_rss_fallback = true) {
  std::vector<LiveHeadline> headlines;
  const int fetch_n = std::clamp(count * 2, count, 12);
  std::string url = std::format("https://query2.finance.yahoo.com/v1/finance/"
                                "search?q={}&quotesCount=0&newsCount={}",
                                url_encode(query), fetch_n);
  long http_code = 0;
  std::string http_err;
  auto t0 = std::chrono::steady_clock::now();
  std::string resp = http_get(url, &http_code, &http_err);
  auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0);

  std::unordered_set<std::string> seen_titles;
  bool yahoo_json_ok = false;

  if (!resp.empty()) {
    boost::system::error_code ec;
    boost::json::value root = boost::json::parse(resp, ec);
    if (!ec && root.is_object()) {
      auto news_it = root.as_object().find("news");
      if (news_it != root.as_object().end() && news_it->value().is_array()) {
        yahoo_json_ok = true;
        for (const auto &item : news_it->value().as_array()) {
          if (!item.is_object()) continue;
          const auto &obj = item.as_object();
          std::string title = get_string_(obj, "title");
          if (title.empty() || seen_titles.contains(title)) continue;
          seen_titles.insert(title);

          std::string publisher = get_string_(obj, "publisher");
          if (publisher.empty()) publisher = "Financial Wire";
          int64_t pub_time = get_int64_(obj, "providerPublishTime", 0);

          headlines.push_back({std::move(title), std::move(publisher), pub_time});
          if (static_cast<int>(headlines.size()) >= fetch_n) break;
        }
      }
    }
  }

  // Always merge Yahoo Ticker RSS for ticker symbols so clickbait listicles don't drown out wire news
  if (allow_rss_fallback && !query.empty() && query.find(' ') == std::string::npos) {
    std::string rss_url = std::format(
        "https://feeds.finance.yahoo.com/rss/2.0/headline?s={}&region=US&lang=en-US",
        url_encode(query));
    auto rss_items = fetch_rss_headlines(rss_url, fetch_n);
    for (auto &it : rss_items) {
      if (seen_titles.contains(it.title)) continue;
      seen_titles.insert(it.title);
      headlines.push_back(std::move(it));
    }
  }

  if (static_cast<int>(headlines.size()) > fetch_n) {
    headlines.resize(static_cast<size_t>(fetch_n));
  }

  if (yahoo_json_ok || !headlines.empty()) {
    DataQualityTelemetry::record_fetch(
        "Yahoo v1 News Search", "query2.finance.yahoo.com/v1/finance/search",
        true, latency, false, "", http_code > 0 ? http_code : 200);
  } else {
    DataQualityTelemetry::record_fetch(
        "Yahoo v1 News Search", "query2.finance.yahoo.com/v1/finance/search",
        false, latency, false,
        http_err.empty() ? "Empty news response" : http_err, http_code);
  }
  return headlines;
}

// Direct HTTP GET with Ch.5 Token-Bucket Rate Limiting, Circuit Breaker & 1
// transient retry
std::string http_get(const std::string &url, long *out_http_code = nullptr,
                     std::string *out_error = nullptr) {
  const bool is_chart_ep = (url.find("/v8/finance/chart") != std::string::npos);
  if (is_chart_ep && !chart_breaker_.allow_request()) {
    DataQualityTelemetry::record_circuit_state("Yahoo v8 Chart",
                                               CircuitState::Open, false);
    if (out_http_code)
      *out_http_code = 503;
    if (out_error)
      *out_error = "Circuit breaker OPEN (fast failover)";
    return {};
  }

  if (!rate_limiter_.try_acquire()) {
    DataQualityTelemetry::record_rate_limit_wait();
    rate_limiter_.acquire(std::chrono::milliseconds(200));
  }

  std::string last_err;
  long last_http_code = 0;
  bool had_transport_failure = false;

  for (int attempt = 0; attempt < 2; ++attempt) {
    CURL *curl = curl_easy_init();
    std::string read_buffer;
    long http_code = 0;

    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
      curl_easy_setopt(
          curl, CURLOPT_USERAGENT,
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
          "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

      CURLcode res = curl_easy_perform(curl);
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
      curl_easy_cleanup(curl);

      last_http_code = http_code;
      if (out_http_code)
        *out_http_code = http_code;
      if (res == CURLE_OK && !read_buffer.empty()) {
        bool transient_http = (http_code == 429 || http_code >= 500);
        if (!transient_http || attempt == 1) {
          if (is_chart_ep) {
            if (transient_http) {
              bool tripped = chart_breaker_.record_failure();
              DataQualityTelemetry::record_circuit_state(
                  "Yahoo v8 Chart", chart_breaker_.state(), tripped);
            } else {
              chart_breaker_.record_success();
            }
          }
          if (out_error)
            out_error->clear();
          return read_buffer;
        }
        had_transport_failure = true;
        last_err = std::format("Transient HTTP {}", http_code);
        continue;
      }
      had_transport_failure =
          (res != CURLE_OK || http_code == 429 || http_code >= 500);
      if (res != CURLE_OK) {
        last_err = curl_easy_strerror(res);
      } else if (http_code >= 400) {
        last_err = std::format("HTTP {}", http_code);
      } else {
        last_err = "Empty response";
      }
    } else {
      had_transport_failure = true;
      last_err = "curl_easy_init failed";
    }
  }
  if (is_chart_ep && had_transport_failure && last_http_code != 404) {
    bool tripped = chart_breaker_.record_failure();
    DataQualityTelemetry::record_circuit_state("Yahoo v8 Chart",
                                               chart_breaker_.state(), tripped);
  }
  if (out_error)
    *out_error = last_err;
  return {};
}

// Authenticated Yahoo Finance GET using session cookie + crumb (thread-safe
// serialized crumb/cookie access)
std::string http_get_yahoo_crumb(const std::string &base_url_without_crumb,
                                 long *out_http_code = nullptr,
                                 std::string *out_error = nullptr) {
  if (!rate_limiter_.try_acquire()) {
    DataQualityTelemetry::record_rate_limit_wait();
    rate_limiter_.acquire(std::chrono::milliseconds(200));
  }

  std::lock_guard<std::mutex> crumb_lock(crumb_mutex_);
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!ensure_yahoo_crumb_(attempt > 0)) {
      if (out_http_code)
        *out_http_code = last_crumb_http_code_;
      if (out_error)
        *out_error =
            last_crumb_error_.empty() ? "Crumb auth failed" : last_crumb_error_;
      return {};
    }
    char sep =
        (base_url_without_crumb.find('?') == std::string::npos) ? '?' : '&';
    std::string full_url = std::format("{}{}crumb={}", base_url_without_crumb,
                                       sep, url_encode(yahoo_crumb_));

    CURL *curl = curl_easy_init();
    std::string read_buffer;
    long http_code = 0;
    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
      curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
      curl_easy_setopt(
          curl, CURLOPT_USERAGENT,
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
          "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

      CURLcode res = curl_easy_perform(curl);
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
      curl_easy_cleanup(curl);

      if (out_http_code)
        *out_http_code = http_code;
      if (res == CURLE_OK && http_code == 200) {
        if (out_error)
          out_error->clear();
        return read_buffer;
      }
      if ((http_code == 401 || http_code == 403) && attempt == 0) {
        // Crumb/cookie expired -> force refresh once
        yahoo_crumb_.clear();
        continue;
      }
      if (out_error) {
        if (res != CURLE_OK)
          *out_error = curl_easy_strerror(res);
        else
          *out_error = std::format("HTTP {}", http_code);
      }
      return {};
    } else if (out_error) {
      *out_error = "curl_easy_init failed";
      return {};
    }
  }
  return {};
}

static std::string url_encode(const std::string &value) {
  std::ostringstream escaped;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << static_cast<char>(c);
    } else {
      escaped << '%' << std::uppercase << std::hex << ((c >> 4) & 0x0F)
              << (c & 0x0F) << std::dec;
    }
  }
  return escaped.str();
}

private:
struct AssetCacheEntry {
  RealTimeAssetData data;
  std::chrono::steady_clock::time_point fetched_at{};
};

struct FxCacheEntry {
  double rate{1.0};
  bool is_live{false};
  std::string source;
  std::chrono::steady_clock::time_point fetched_at{};
};

mutable std::mutex cache_mutex_;
mutable std::mutex crumb_mutex_;
TokenBucketRateLimiter rate_limiter_{24.0, 32.0};
EndpointCircuitBreaker chart_breaker_{5, std::chrono::seconds(15)};

std::unordered_map<std::string, AssetCacheEntry> asset_cache_;
std::unordered_map<std::string, FxCacheEntry> fx_cache_;
TreasuryYieldCurveData cached_yields_;
std::chrono::steady_clock::time_point yields_fetched_at_{};
bool has_cached_yields_{false};

std::string cookie_file_path_;
std::string yahoo_crumb_;
std::chrono::steady_clock::time_point last_crumb_attempt_at_{};
long last_crumb_http_code_{0};
std::string last_crumb_error_;

FxCacheEntry get_fx_entry_(const std::string &from_ccy,
                           const std::string &to_ccy,
                           bool force_refresh = false) {
  std::string f = infer_currency_code("", from_ccy);
  std::string t = infer_currency_code("", to_ccy);
  if (f.empty() || t.empty() || f == t)
    return {1.0, true, "identity", std::chrono::steady_clock::now()};

  std::string key = f + "_" + t;
  std::string pair = f + "/" + t;
  const auto now_steady = std::chrono::steady_clock::now();
  if (!force_refresh) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (auto it = fx_cache_.find(key); it != fx_cache_.end()) {
      if ((now_steady - it->second.fetched_at) < kFxCacheTtl) {
        DataQualityTelemetry::record_fx_rate(
            pair, it->second.rate, it->second.is_live, it->second.source);
        return it->second;
      }
    }
  }

auto fetch_pair = [this, force_refresh](const std::string &ticker) -> double {
  auto d = fetch_asset(ticker, "5d", force_refresh);
  return (d.is_live && d.current_price > 0.0) ? d.current_price : -1.0;
};

FxCacheEntry entry;
if (f == "USD" && t == "TRY") {
  double r = fetch_pair("TRY=X");
  if (r > 0.0)
    entry = {r, true, "live", now_steady};
} else if (f == "TRY" && t == "USD") {
  auto r = get_fx_entry_("USD", "TRY", force_refresh);
  if (r.is_live && r.rate > 0.0)
    entry = {1.0 / r.rate, true, "live (inv)", now_steady};
} else if (f == "EUR" && t == "TRY") {
  double r = fetch_pair("EURTRY=X");
  if (r > 0.0) {
    entry = {r, true, "live", now_steady};
  } else {
    auto eu = get_fx_entry_("EUR", "USD", force_refresh);
    auto ut = get_fx_entry_("USD", "TRY", force_refresh);
    if (eu.is_live && ut.is_live && eu.rate > 0.0 && ut.rate > 0.0) {
      entry = {eu.rate * ut.rate, true, "live (cross)", now_steady};
    }
  }
} else if (f == "TRY" && t == "EUR") {
  auto r = get_fx_entry_("EUR", "TRY", force_refresh);
  if (r.is_live && r.rate > 0.0)
    entry = {1.0 / r.rate, true, "live (inv)", now_steady};
} else {
  double r = fetch_pair(f + t + "=X");
  if (r > 0.0) {
    entry = {r, true, "live", now_steady};
  } else {
    double inv = fetch_pair(t + f + "=X");
    if (inv > 0.0)
      entry = {1.0 / inv, true, "live (inv)", now_steady};
  }
}

if (!entry.is_live || entry.rate <= 0.0) {
  // Emergency offline fallback constants
  double fb = 1.0;
  if (f == "USD" && t == "TRY")
    fb = 38.0;
  else if (f == "EUR" && t == "TRY")
    fb = 41.0;
  else if (f == "EUR" && t == "USD")
    fb = 1.08;
  else if (f == "TRY" && t == "USD")
    fb = 1.0 / 38.0;
  else if (f == "TRY" && t == "EUR")
    fb = 1.0 / 41.0;
  entry = {fb, false, "fallback", now_steady};
  DataQualityTelemetry::record_fx_rate(pair, entry.rate, false, "fallback");
} else {
  entry.fetched_at = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    fx_cache_[key] = entry;
  }
  DataQualityTelemetry::record_fx_rate(pair, entry.rate, true, entry.source);
}
return entry;
}

bool ensure_yahoo_crumb_(bool force_retry = false) {
  if (!force_retry && !yahoo_crumb_.empty())
    return true;
  const auto now_steady = std::chrono::steady_clock::now();
  if (!force_retry && last_crumb_attempt_at_.time_since_epoch().count() != 0 &&
      (now_steady - last_crumb_attempt_at_) < std::chrono::seconds(30)) {
    return false;
  }
  last_crumb_attempt_at_ = now_steady;

  cookie_file_path_ = "yf_session_cookies.tmp";

  // Step 1: Obtain session cookie from fc.yahoo.com
  {
    CURL *curl = curl_easy_init();
    std::string sink;
    if (!curl) {
      last_crumb_error_ = "curl_easy_init failed";
      return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, "https://fc.yahoo.com");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(
        curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      last_crumb_error_ =
          std::string("Cookie step failed: ") + curl_easy_strerror(res);
    }
    curl_easy_cleanup(curl);
  }

  // Step 2: Obtain crumb token
  {
    CURL *curl = curl_easy_init();
    std::string crumb_buf;
    long code = 0;
    if (!curl) {
      last_crumb_error_ = "curl_easy_init failed";
      return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://query2.finance.yahoo.com/v1/test/getcrumb");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path_.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path_.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &crumb_buf);
    curl_easy_setopt(
        curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    last_crumb_http_code_ = code;
    crumb_buf = trim_(crumb_buf);
    if (res == CURLE_OK && code == 200 && !crumb_buf.empty() &&
        crumb_buf.size() < 64 && crumb_buf.find('{') == std::string::npos &&
        crumb_buf.find('<') == std::string::npos) {
      yahoo_crumb_ = crumb_buf;
      last_crumb_error_.clear();
      return true;
    }
    if (res != CURLE_OK) {
      last_crumb_error_ =
          std::string("Crumb fetch failed: ") + curl_easy_strerror(res);
    } else {
      last_crumb_error_ = std::format("Crumb auth rejected (HTTP {})", code);
    }
  }
  return false;
}

// Synthesize 24K Gram Gold / Silver in TL using date-aligned COMEX Ounce *
// USD/TRY / 31.1034768
RealTimeAssetData fetch_gram_precious_metal_tl_(const std::string &ounce_symbol,
                                                const std::string &out_symbol,
                                                const std::string &metal_title,
                                                double fallback_price,
                                                const std::string &range,
                                                bool force_refresh = false) {
  constexpr double TROY_OUNCE_GRAMS = 31.1034768;
  auto ons = fetch_asset(ounce_symbol, range, force_refresh);
  auto usdtry = fetch_asset("TRY=X", range, force_refresh);

  RealTimeAssetData gram;
  gram.symbol = out_symbol;
  gram.long_name = metal_title;
  gram.currency_code = "TRY";
  gram.currency_display = "TL";
  gram.instrument_type = "FUTURE";
  gram.is_synthetic_metal = true;
  gram.fetched_at = std::chrono::system_clock::now();

  if (ons.is_live && usdtry.is_live && ons.current_price > 0.0 &&
      usdtry.current_price > 0.0) {
    const auto now_steady = std::chrono::steady_clock::now();
    DataQualityTelemetry::record_fx_rate("USD/TRY", usdtry.current_price, true,
                                         "live");
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      if (force_refresh || !fx_cache_.contains("USD_TRY") ||
          (now_steady - fx_cache_["USD_TRY"].fetched_at) >= kFxCacheTtl) {
        fx_cache_["USD_TRY"] = {usdtry.current_price, true, "live", now_steady};
      }
    }
    gram.underlying_ounce_usd = ons.current_price;
    gram.underlying_usdtry = usdtry.current_price;
    gram.regular_market_time =
        std::max(ons.regular_market_time, usdtry.regular_market_time);
    gram.current_price =
        (ons.current_price * usdtry.current_price) / TROY_OUNCE_GRAMS;

    auto aligned = align_series_by_date(ons, usdtry, true);
    size_t n = aligned.prices_a.size();
    if (n >= 2) {
      gram.raw_bar_count = n;
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
        double gp =
            (aligned.prices_a[i] * aligned.prices_b[i]) / TROY_OUNCE_GRAMS;
        double fx_gram = aligned.prices_b[i] / TROY_OUNCE_GRAMS;
        double go = gp, gh = gp * 1.004, gl = gp * 0.996;
        if (i < aligned.indices_a.size()) {
          size_t ia = aligned.indices_a[i];
          if (ia < ons.historical_opens.size() &&
              ons.historical_opens[ia] > 0.0) {
            go = ons.historical_opens[ia] * fx_gram;
          }
          if (ia < ons.historical_highs.size() &&
              ons.historical_highs[ia] > 0.0) {
            gh = ons.historical_highs[ia] * fx_gram;
          }
          if (ia < ons.historical_lows.size() &&
              ons.historical_lows[ia] > 0.0) {
            gl = ons.historical_lows[ia] * fx_gram;
          }
        }
        gram.historical_prices.push_back(gp);
        gram.historical_raw_closes.push_back(gp);
        gram.historical_opens.push_back(go);
        gram.historical_highs.push_back(std::max({gh, go, gp}));
        gram.historical_lows.push_back(std::min({gl, go, gp}));
      }
      if (gram.historical_prices.size() >= 2) {
        gram.previous_close =
            gram.historical_prices[gram.historical_prices.size() - 2];
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

static size_t write_callback_(void *contents, size_t size, size_t nmemb,
                              void *userp) {
  size_t total_size = size * nmemb;
  auto *str = static_cast<std::string *>(userp);
  str->append(static_cast<char *>(contents), total_size);
  return total_size;
}

static std::string trim_(std::string_view sv) {
  size_t start = 0;
  while (start < sv.size() &&
         std::isspace(static_cast<unsigned char>(sv[start])))
    ++start;
  size_t end = sv.size();
  while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1])))
    --end;
  return std::string(sv.substr(start, end - start));
}

// Fold Turkish/German UTF-8 characters to ASCII uppercase for reliable ticker
// resolution
static std::string ascii_fold_upper_(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);
    if (c == 0xC3 && i + 1 < input.size()) {
      unsigned char c2 = static_cast<unsigned char>(input[++i]);
      if (c2 == 0x87 || c2 == 0xA7)
        out.push_back('C'); // Ç, ç
      else if (c2 == 0x96 || c2 == 0xB6)
        out.push_back('O'); // Ö, ö
      else if (c2 == 0x9C || c2 == 0xBC)
        out.push_back('U'); // Ü, ü
      else if (c2 == 0x84 || c2 == 0xA4)
        out.push_back('A'); // Ä, ä
    } else if (c == 0xC4 && i + 1 < input.size()) {
      unsigned char c2 = static_cast<unsigned char>(input[++i]);
      if (c2 == 0x9E || c2 == 0x9F)
        out.push_back('G'); // Ğ, ğ
      else if (c2 == 0xB0 || c2 == 0xB1)
        out.push_back('I'); // İ, ı
    } else if (c == 0xC5 && i + 1 < input.size()) {
      unsigned char c2 = static_cast<unsigned char>(input[++i]);
      if (c2 == 0x9E || c2 == 0x9F)
        out.push_back('S'); // Ş, ş
    } else if (c < 0x80) {
      out.push_back(static_cast<char>(std::toupper(c)));
    }
  }
  return out;
}

static std::string get_string_(const boost::json::object &obj,
                               std::string_view key) {
  auto it = obj.find(key);
  if (it != obj.end() && it->value().is_string()) {
    return std::string(it->value().as_string());
  }
  return {};
}

static double get_double_(const boost::json::object &obj, std::string_view key,
                          double def_val) {
  auto it = obj.find(key);
  if (it == obj.end() || it->value().is_null())
    return def_val;
  if (it->value().is_double())
    return it->value().as_double();
  if (it->value().is_int64())
    return static_cast<double>(it->value().as_int64());
  if (it->value().is_uint64())
    return static_cast<double>(it->value().as_uint64());
  return def_val;
}

static int64_t get_int64_(const boost::json::object &obj, std::string_view key,
                          int64_t def_val) {
  auto it = obj.find(key);
  if (it == obj.end() || it->value().is_null())
    return def_val;
  if (it->value().is_int64())
    return it->value().as_int64();
  if (it->value().is_uint64())
    return static_cast<int64_t>(it->value().as_uint64());
  if (it->value().is_double())
    return static_cast<int64_t>(it->value().as_double());
  return def_val;
}

static double elem_double_(const boost::json::array *arr, size_t idx,
                           double def_val) {
  if (!arr || idx >= arr->size())
    return def_val;
  const auto &v = (*arr)[idx];
  if (v.is_null())
    return def_val;
  if (v.is_double())
    return v.as_double();
  if (v.is_int64())
    return static_cast<double>(v.as_int64());
  if (v.is_uint64())
    return static_cast<double>(v.as_uint64());
  return def_val;
}

static int64_t elem_int64_(const boost::json::array *arr, size_t idx,
                           int64_t def_val) {
  if (!arr || idx >= arr->size())
    return def_val;
  const auto &v = (*arr)[idx];
  if (v.is_null())
    return def_val;
  if (v.is_int64())
    return v.as_int64();
  if (v.is_uint64())
    return static_cast<int64_t>(v.as_uint64());
  if (v.is_double())
    return static_cast<int64_t>(v.as_double());
  return def_val;
}

// Realistic low-volatility (~22% annualized) offline fallback series used
// ONLY when network is down
static void populate_offline_fallback_(RealTimeAssetData &data) {
  if (data.current_price <= 0.0) {
    if (data.symbol == "SPY")
      data.current_price = 582.50;
    else if (data.symbol == "AAPL")
      data.current_price = 231.40;
    else if (data.symbol == "NVDA")
      data.current_price = 138.00;
    else if (data.symbol == "THYAO.IS")
      data.current_price = 295.00;
    else if (data.symbol == "ASELS.IS")
      data.current_price = 92.50;
    else if (data.symbol == "TUPRS.IS")
      data.current_price = 162.00;
    else if (data.symbol == "BIMAS.IS")
      data.current_price = 495.00;
    else if (data.symbol == "RHM.DE")
      data.current_price = 1056.00;
    else if (data.symbol == "SAP.DE")
      data.current_price = 215.00;
    else
      data.current_price = 100.0;
  }

  // Smooth realistic ~1.2% daily step series (65 trading days) so offline
  // tests behave sensibly
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
    data.historical_volumes[i] =
        1000000.0 * (1.0 + 0.25 * std::sin(static_cast<double>(i)));
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
}
;

} // namespace quant
