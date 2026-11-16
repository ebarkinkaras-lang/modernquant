#pragma once

#include "alpha_screener.hpp"
#include "data_quality.hpp"
#include "forecast_engine.hpp"
#include "fundamentals.hpp"
#include "i18n.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
#include "pdf_writer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <format>
#include <future>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace quant {

struct TechnicalSnapshot {
  std::string symbol;
  std::string display_name;
  std::string unit;
  bool is_live{false};
  double spot{0.0};
  double ret_5d_pct{0.0};
  double ret_20d_pct{0.0};
  double ret_60d_pct{0.0};
  double sma_20{0.0};
  double sma_50{0.0};
  double sma_200{0.0};
  bool has_sma200{false};
  double rsi_14{50.0};
  double vwap_22{0.0};
  double vwap_dist_pct{0.0};
  double vwap_z{0.0};
  double atr_14_pct{0.0};
  double ann_vol_pct{0.0};
  double squeeze_ratio{1.0};
  std::string trend_regime;
  std::vector<double> last_60_prices;
};

struct RegionalFundamentalAgg {
  int total_scanned{0};
  int fund_count{0};
  int ebitda_count{0};
  double median_pe{0.0};
  double median_ev_ebitda{0.0};
  double median_pb{0.0};
  double mean_ebitda_margin_pct{0.0};
  double mean_roe_pct{0.0};
  double mean_net_debt_ebitda{0.0};
  int grade_a_count{0};
  int grade_b_count{0};
  int grade_c_count{0};
  double pct_above_vwap{0.0};
  double pct_rsi_bull{0.0};
  double mean_inst_flow{0.0};
  double mean_time_ic{0.0};
  int neg_shock_count{0};
};

struct MacroDriverEntry {
  std::string symbol;
  std::string label;
  std::string unit;
  std::string role_note;
  RealTimeAssetData data;
  TechnicalSnapshot tech;
  ForecastResult forecast_1m;
};

struct RegionDeepReport {
  std::string region_code;
  std::string region_title;
  RealTimeAssetData benchmark_data;
  TechnicalSnapshot benchmark_tech;
  ForecastResult benchmark_forecast_1m;
  std::vector<TechnicalSnapshot> sub_indices_tech;
  std::vector<ForecastResult> sub_indices_fc;
  RegionalMacroSentiment macro_sent;
  std::vector<AlphaCandidate> candidates;
  RegionalFundamentalAgg fund_agg;
};

struct GlobalMacroReportData {
  std::string generated_at;
  TreasuryYieldCurveData yield_curve;
  double yield_spread_bps{0.0};
  double global_macro_score{0.0};
  std::string global_regime_title;
  std::string global_1m_outlook;

  RegionDeepReport us;
  RegionDeepReport xetra;
  RegionDeepReport bist;
  double bist_usd_ret20d_pct{0.0};
  double bist_usd_ret60d_pct{0.0};

  std::vector<MacroDriverEntry> drivers;
  // Correlations [benchmark_idx][driver_idx]
  std::vector<std::string> corr_row_labels;
  std::vector<std::string> corr_col_labels;
  std::vector<std::vector<double>> corr_matrix;
  std::vector<std::vector<int>> corr_samples;
  std::string correlation_summary;
};

class GlobalMacroReportEngine {
public:
  static GlobalMacroReportData collect_report_data(LiveMarketDataClient &client) {
    GlobalMacroReportData rep;
    rep.generated_at = current_timestamp_str_();

    // Fetch US Treasury Yield Curve
    rep.yield_curve = client.fetch_treasury_yields();
    rep.yield_spread_bps =
        (rep.yield_curve.ten_year_yield - rep.yield_curve.three_month_yield) *
        10000.0;

    // Fetch 1Y historical OHLCV for benchmarks + global drivers in bounded async pool
    struct FetchReq {
      std::string symbol;
      std::string range;
    };
    const std::vector<FetchReq> reqs = {
        {"SPY", "1y"},       {"QQQ", "1y"},      {"IWM", "1y"},
        {"^GDAXI", "1y"},    {"XU100.IS", "1y"}, {"^TNX", "1y"},
        {"^IRX", "1y"},      {"^VIX", "1y"},     {"DX-Y.NYB", "1y"},
        {"EURUSD=X", "1y"},  {"BZ=F", "1y"},     {"GC=F", "1y"},
        {"HG=F", "1y"},      {"TRY=X", "1y"},    {"EURTRY=X", "1y"},
        {"BTC-USD", "1y"},
    };
    std::vector<RealTimeAssetData> fetched(reqs.size());
    std::atomic<size_t> next_i{0};
    const size_t n_workers = std::min<size_t>(6, reqs.size());
    std::vector<std::future<void>> pool;
    pool.reserve(n_workers);
    for (size_t w = 0; w < n_workers; ++w) {
      pool.push_back(std::async(std::launch::async, [&]() {
        while (true) {
          size_t idx = next_i.fetch_add(1, std::memory_order_relaxed);
          if (idx >= reqs.size())
            break;
          auto d = client.fetch_asset(reqs[idx].symbol, reqs[idx].range);
          DataQualityDashboard::record_asset_quality(d);
          fetched[idx] = std::move(d);
        }
      }));
    }
    for (auto &f : pool)
      f.get();

    auto find_data = [&](std::string_view sym) -> RealTimeAssetData {
      for (size_t i = 0; i < reqs.size(); ++i) {
        if (reqs[i].symbol == sym)
          return fetched[i];
      }
      return {};
    };

    // Regional Macro Sentiment & News (sequential after OHLCV pool to avoid RSS throttling)
    rep.us.macro_sent = NewsSentimentEngine::build_us_macro_sentiment(client);
    rep.xetra.macro_sent = NewsSentimentEngine::build_xetra_macro_sentiment(client);
    rep.bist.macro_sent = NewsSentimentEngine::build_bist_macro_sentiment(&client);

    // Run 3 Screener Universes (internal bounded worker pools) for bottom-up Fundamentals & Alpha
    ScreeningFilterOptions full_opts;
    full_opts.exclude_crowded = false;
    full_opts.use_sticky_sentiment = true;

    rep.us.candidates = QuantOpportunityScanner::scan_market("US", client, full_opts);
    rep.xetra.candidates =
        QuantOpportunityScanner::scan_market("XETRA", client, full_opts);
    rep.bist.candidates =
        QuantOpportunityScanner::scan_market("BIST", client, full_opts);

    // Populate Regional Reports (Technicals + Bottom-Up Fundamentals + 21-Trading-Day 1M Forecast)
    RealTimeAssetData d_spy = find_data("SPY");
    RealTimeAssetData d_qqq = find_data("QQQ");
    RealTimeAssetData d_iwm = find_data("IWM");
    RealTimeAssetData d_dax = find_data("^GDAXI");
    RealTimeAssetData d_xu100 = find_data("XU100.IS");

    const std::string h1m_label =
        I18n::tr("1 Ay (21 İşG)", "1 Month (21TD)", "1 Monat (21HT)");

    // US
    rep.us.region_code = "US";
    rep.us.region_title = I18n::tr("ABD Borsaları (Wall Street)",
                                   "US Equities (Wall Street)",
                                   "US-Aktienmärkte (Wall Street)");
    rep.us.benchmark_data = d_spy;
    rep.us.benchmark_tech = compute_technical_snapshot_(d_spy, "S&P 500 (SPY)", "$");
    rep.us.benchmark_forecast_1m = QuantForecastEngine::forecast_for_trading_days(
        d_spy, 21, rep.us.macro_sent.overall_sentiment_score, true, nullptr,
        h1m_label, 30, &rep.yield_curve);
    rep.us.sub_indices_tech = {
        rep.us.benchmark_tech,
        compute_technical_snapshot_(d_qqq, "NASDAQ-100 (QQQ)", "$"),
        compute_technical_snapshot_(d_iwm, "Russell 2000 (IWM)", "$"),
    };
    rep.us.sub_indices_fc = {
        rep.us.benchmark_forecast_1m,
        QuantForecastEngine::forecast_for_trading_days(
            d_qqq, 21, rep.us.macro_sent.overall_sentiment_score, true, nullptr,
            h1m_label, 30, &rep.yield_curve),
        QuantForecastEngine::forecast_for_trading_days(
            d_iwm, 21, rep.us.macro_sent.overall_sentiment_score, true, nullptr,
            h1m_label, 30, &rep.yield_curve),
    };
    rep.us.fund_agg = aggregate_fundamentals_(rep.us.candidates);

    // XETRA
    rep.xetra.region_code = "XETRA";
    rep.xetra.region_title = I18n::tr("Almanya Borsası (XETRA / DAX 40)",
                                      "Germany Equities (XETRA / DAX 40)",
                                      "Deutschland (XETRA DAX 40)");
    rep.xetra.benchmark_data = d_dax;
    rep.xetra.benchmark_tech = compute_technical_snapshot_(d_dax, "DAX 40 (^GDAXI)", "pt");
    rep.xetra.benchmark_forecast_1m = QuantForecastEngine::forecast_for_trading_days(
        d_dax, 21, rep.xetra.macro_sent.overall_sentiment_score, true, nullptr,
        h1m_label, 30, &rep.yield_curve);
    rep.xetra.sub_indices_tech = {rep.xetra.benchmark_tech};
    rep.xetra.sub_indices_fc = {rep.xetra.benchmark_forecast_1m};
    rep.xetra.fund_agg = aggregate_fundamentals_(rep.xetra.candidates);

    // BIST
    rep.bist.region_code = "BIST";
    rep.bist.region_title = I18n::tr("Türkiye Borsası (BIST 100)",
                                     "Turkey Equities (BIST 100)",
                                     "Türkei Aktienmarkt (BIST 100)");
    rep.bist.benchmark_data = d_xu100;
    rep.bist.benchmark_tech = compute_technical_snapshot_(d_xu100, "BIST 100 (XU100.IS)", "pt");
    rep.bist.benchmark_forecast_1m = QuantForecastEngine::forecast_for_trading_days(
        d_xu100, 21, rep.bist.macro_sent.overall_sentiment_score, true, nullptr,
        h1m_label, 30, nullptr);
    rep.bist.sub_indices_tech = {rep.bist.benchmark_tech};
    rep.bist.sub_indices_fc = {rep.bist.benchmark_forecast_1m};
    rep.bist.fund_agg = aggregate_fundamentals_(rep.bist.candidates);

    // Compute BIST USD-adjusted returns cleanly via date-key alignment against TRY=X
    RealTimeAssetData d_usdtry = find_data("TRY=X");
    rep.bist_usd_ret20d_pct = compute_fx_adjusted_return_(d_xu100, d_usdtry, 20);
    rep.bist_usd_ret60d_pct = compute_fx_adjusted_return_(d_xu100, d_usdtry, 60);

    // Populate Global Economic Drivers + 1M Forecasts
    struct DriverSpec {
      std::string sym;
      std::string label;
      std::string unit;
      std::string role;
    };
    const std::vector<DriverSpec> driver_specs = {
        {"^TNX", I18n::tr("ABD 10Y Tahvil Faizi", "US 10Y Treasury Yield", "US 10J Staatsanleihe"),
         "%", I18n::tr("Küresel iskonto oranı ve büyüme çarpanı çıpası", "Global discount rate & equity multiple anchor", "Globaler Diskontsatz & Bewertungsanker")},
        {"^IRX", I18n::tr("ABD 3A Hazine Bonosu", "US 3M T-Bill Yield", "US 3M Geldmarktzins"),
         "%", I18n::tr("Fed politika faizi ve kısa vadeli dolar maliyeti", "Fed policy rate & short USD funding cost", "Fed-Leitzins & USD-Finanzierungskosten")},
        {"^VIX", I18n::tr("CBOE VIX Volatilite", "CBOE VIX Volatility", "CBOE VIX Volatilität"),
         "pt", I18n::tr("Küresel risk iştahı ve opsiyon korku barometresi", "Global risk appetite & tail fear gauge", "Globales Risiko- & Angstbarometer")},
        {"DX-Y.NYB", I18n::tr("DXY ABD Dolar Endeksi", "DXY US Dollar Index", "DXY US-Dollar-Index"),
         "pt", I18n::tr("Gelişen piyasa likiditesi ve emtia ters baskısı", "EM liquidity & commodity inverse pressure", "Schwellenländer-Liquidität & Rohstoffdruck")},
        {"EURUSD=X", "EUR / USD", "$",
         I18n::tr("DAX ihracat rekabetçiliği ve Euro akımı", "DAX export competitiveness & Euro flow", "DAX-Exportwettbewerbsfähigkeit")},
        {"BZ=F", I18n::tr("Brent Ham Petrol", "Brent Crude Oil", "Brent Rohöl"),
         "$/bbl", I18n::tr("Küresel enflasyon ve Türkiye/AB enerji faturası", "Global inflation & TR/EU energy import cost", "Globale Inflation & Energiekosten")},
        {"GC=F", I18n::tr("Ons Altın (COMEX)", "Spot/Futures Gold", "Gold (Unze)"),
         "$/oz", I18n::tr("Jeopolitik güvenli liman ve reel faiz göstergesi", "Safe-haven demand & real yield barometer", "Sicherer Hafen & Realzinsindikator")},
        {"HG=F", I18n::tr("Bakır (Dr. Copper)", "Copper Futures", "Kupfer-Futures"),
         "$/lb", I18n::tr("Küresel sanayi üretimi ve imalat öncü sinyali", "Global manufacturing & industrial cycle proxy", "Industriezyklus-Frühindikator")},
        {"TRY=X", "USD / TRY", "TL",
         I18n::tr("BIST kur riski, enflasyon ve ihracatçı marjları", "BIST FX risk, inflation & exporter margins", "BIST Wechselkursrisiko & Margen")},
        {"BTC-USD", "Bitcoin (BTC-USD)", "$",
         I18n::tr("Yüksek beta küresel perakende/likidite risk iştahı", "High-beta global speculative liquidity gauge", "Globaler Liquiditäts- & Risikoindikator")},
    };

    rep.drivers.reserve(driver_specs.size());
    for (const auto &ds : driver_specs) {
      MacroDriverEntry e;
      e.symbol = ds.sym;
      e.label = ds.label;
      e.unit = ds.unit;
      e.role_note = ds.role;
      e.data = find_data(ds.sym);
      e.tech = compute_technical_snapshot_(e.data, ds.label, ds.unit);
      e.forecast_1m = QuantForecastEngine::forecast_for_trading_days(
          e.data, 21, 0.0, false, nullptr, h1m_label, 30, nullptr);
      rep.drivers.push_back(std::move(e));
    }

    // Compute 6-Month Date-Aligned Cross-Asset Correlation Matrix
    const std::vector<std::pair<std::string, RealTimeAssetData>> eq_rows = {
        {I18n::tr("ABD S&P 500 (SPY)", "US S&P 500 (SPY)", "US S&P 500 (SPY)"), d_spy},
        {I18n::tr("ABD NASDAQ (QQQ)", "US NASDAQ (QQQ)", "US NASDAQ (QQQ)"), d_qqq},
        {I18n::tr("ABD Russell (IWM)", "US Russell (IWM)", "US Russell (IWM)"), d_iwm},
        {I18n::tr("Almanya DAX (^GDAXI)", "Germany DAX (^GDAXI)", "Deutschland DAX (^GDAXI)"), d_dax},
        {I18n::tr("Türkiye BIST 100", "Turkey BIST 100", "Türkei BIST 100"), d_xu100},
    };
    const std::vector<std::pair<std::string, RealTimeAssetData>> mac_cols = {
        {I18n::tr("ABD 10Y", "US 10Y", "US 10J"), find_data("^TNX")},
        {"VIX", find_data("^VIX")},
        {"DXY", find_data("DX-Y.NYB")},
        {"Brent", find_data("BZ=F")},
        {"Altın / Gold", find_data("GC=F")},
        {I18n::tr("Bakır", "Copper", "Kupfer"), find_data("HG=F")},
        {"USD/TRY", d_usdtry},
    };
    rep.corr_row_labels.reserve(eq_rows.size());
    rep.corr_col_labels.reserve(mac_cols.size());
    for (const auto &c : mac_cols)
      rep.corr_col_labels.push_back(c.first == "Altın / Gold"
                                        ? I18n::tr("Altın", "Gold", "Gold")
                                        : c.first);

    for (const auto &r : eq_rows) {
      rep.corr_row_labels.push_back(r.first);
      std::vector<double> row_vals(mac_cols.size(), 0.0);
      std::vector<int> row_n(mac_cols.size(), 0);
      for (size_t j = 0; j < mac_cols.size(); ++j) {
        auto [corr, cnt] = compute_date_aligned_corr_(r.second, mac_cols[j].second, 130);
        row_vals[j] = corr;
        row_n[j] = cnt;
      }
      rep.corr_matrix.push_back(std::move(row_vals));
      rep.corr_samples.push_back(std::move(row_n));
    }

    synthesize_world_economy_outlook_(rep);
    return rep;
  }

  static bool generate_pdf_report(const GlobalMacroReportData &rep,
                                  const std::string &output_path) {
    PdfDocument pdf;
    pdf.set_header_footer_meta(
        I18n::tr("MODERNQUANT C++20  │  KÜRESEL PİYASALAR VE 1 AYLIK MAKRO RAPOR",
                 "MODERNQUANT C++20  │  GLOBAL MARKETS & 1-MONTH MACRO REPORT",
                 "MODERNQUANT C++20  │  GLOBALE MÄRKTE & 1-MONATS-MAKROBERICHT"),
        rep.generated_at,
        I18n::tr("Yalnızca kantitatif araştırma amaçlıdır — yatırım tavsiyesi niteliği taşımaz.",
                 "For quantitative research only — not personalized investment advice.",
                 "Nur zu quantitativen Forschungszwecken — keine Anlageberatung."));

    render_page1_executive_and_macro_(pdf, rep);

    pdf.new_page();
    render_page2_charts_and_correlations_(pdf, rep);

    pdf.new_page();
    render_page3_us_and_xetra_(pdf, rep);

    pdf.new_page();
    render_page4_bist_and_synthesis_(pdf, rep);

    pdf.finalize_page_numbers(I18n::tr("Sayfa", "Page", "Seite"));
    return pdf.save(output_path);
  }

  static void render_terminal_summary(const GlobalMacroReportData &rep,
                                      const std::string &pdf_path) {
    auto &te = ThemeEngine::instance();
    const auto &pal = te.palette();
    std::cout << "\n" << pal.glow_rgb
              << "╔══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
              << std::format("║  📄 {:<87}  ║\n",
                             I18n::tr("KÜRESEL BORSALAR (ABD/XETRA/BIST) & DÜNYA EKONOMİSİ 1 AYLIK RAPOR ÖZETİ",
                                      "GLOBAL EQUITIES (US/XETRA/BIST) & WORLD MACRO 1-MONTH PDF SUMMARY",
                                      "GLOBALE MÄRKTE (USA/XETRA/BIST) & WELTWIRTSCHAFT 1M-BERICHT"))
              << "╠══════════════════════════════════════════════════════════════════════════════════════════════╣"
              << pal.reset_code << "\n";
    std::cout << std::format("  {}: {}  │  {}: {:+.2f} ({})\n",
                             I18n::tr("Tarih", "Date", "Datum"), rep.generated_at,
                             I18n::tr("Küresel Rejim Skoru", "Global Regime Score", "Globales Makro-Score"),
                             rep.global_macro_score, rep.global_regime_title);
    std::cout << std::format("  {}: {} %{:.2f} │ {} %{:.2f} │ {}: {:+.0f} bps\n",
                             I18n::tr("ABD Getiri Eğrisi", "US Treasury Curve", "US-Zinskurve"),
                             I18n::tr("3A", "3M", "3M"),
                             rep.yield_curve.three_month_yield * 100.0,
                             I18n::tr("10Y", "10Y", "10J"),
                             rep.yield_curve.ten_year_yield * 100.0,
                             I18n::tr("10Y-3A Eğim", "10Y-3M Slope", "10J-3M Spread"),
                             rep.yield_spread_bps);
    std::cout << "──────────────────────────────────────────────────────────────────────────────────────────────\n";
    for (const auto *r : {&rep.us, &rep.xetra, &rep.bist}) {
      std::cout << std::format(
          "  {:<28} Spot: {:>9.2f}  │ {}: {:>+6.2f}% │ {}: {:>+6.2f}% [{:+.1f}%..{:+.1f}%] │ {}: {:.1f}x\n",
          r->region_code + " (" + r->benchmark_tech.symbol + ")",
          r->benchmark_tech.spot,
          I18n::tr("20G", "20D", "20T"), r->benchmark_tech.ret_20d_pct,
          I18n::tr("1Ay Hedef", "1M Target", "1M Ziel"),
          r->benchmark_forecast_1m.expected_return_pct,
          r->benchmark_forecast_1m.bear_return_pct,
          r->benchmark_forecast_1m.bull_return_pct,
          I18n::tr("Med F/K", "Med P/E", "Med KGV"),
          r->fund_agg.median_pe);
    }
    std::cout << "──────────────────────────────────────────────────────────────────────────────────────────────\n";
    std::cout << pal.green_rgb
              << std::format("  ✔ {}: {}\n",
                             I18n::tr("PDF Raporu başarıyla oluşturuldu",
                                      "PDF report successfully generated",
                                      "PDF-Bericht erfolgreich erstellt"),
                             pdf_path)
              << pal.reset_code << "\n";
  }

private:
  // Color palette matching institutional equity research reports
  static constexpr PdfRgb kNavy{PdfRgb::hex(0x0F172A)};
  static constexpr PdfRgb kSlateDark{PdfRgb::hex(0x1E293B)};
  static constexpr PdfRgb kSlateMid{PdfRgb::hex(0x475569)};
  static constexpr PdfRgb kSlateMuted{PdfRgb::hex(0x64748B)};
  static constexpr PdfRgb kBgSoft{PdfRgb::hex(0xF8FAFC)};
  static constexpr PdfRgb kBgAlt{PdfRgb::hex(0xF1F5F9)};
  static constexpr PdfRgb kBorder{PdfRgb::hex(0xCBD5E1)};
  static constexpr PdfRgb kWhite{PdfRgb::hex(0xFFFFFF)};
  static constexpr PdfRgb kGold{PdfRgb::hex(0xD97706)};
  static constexpr PdfRgb kGreen{PdfRgb::hex(0x15803D)};
  static constexpr PdfRgb kGreenBg{PdfRgb::hex(0xDCFCE7)};
  static constexpr PdfRgb kRed{PdfRgb::hex(0xB91C1C)};
  static constexpr PdfRgb kRedBg{PdfRgb::hex(0xFEE2E2)};
  static constexpr PdfRgb kBlue{PdfRgb::hex(0x1D4ED8)};
  static constexpr PdfRgb kBlueCone{PdfRgb::hex(0xDBEAFE)};
  static constexpr PdfRgb kAmberBg{PdfRgb::hex(0xFEF3C7)};

  static std::string current_timestamp_str_() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &tt);
#else
    localtime_r(&tt, &tm_now);
#endif
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}",
                       tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                       tm_now.tm_hour, tm_now.tm_min);
  }

  static double median_of_(std::vector<double> vals) {
    if (vals.empty())
      return 0.0;
    std::sort(vals.begin(), vals.end());
    size_t mid = vals.size() / 2;
    if (vals.size() % 2 == 1)
      return vals[mid];
    return 0.5 * (vals[mid - 1] + vals[mid]);
  }

  static double compute_rsi_(const std::vector<double> &p, size_t period = 14) {
    if (p.size() <= period)
      return 50.0;
    double avg_g = 0.0, avg_l = 0.0;
    for (size_t i = 1; i <= period; ++i) {
      double diff = p[i] - p[i - 1];
      if (diff >= 0.0)
        avg_g += diff;
      else
        avg_l += -diff;
    }
    avg_g /= static_cast<double>(period);
    avg_l /= static_cast<double>(period);
    for (size_t i = period + 1; i < p.size(); ++i) {
      double diff = p[i] - p[i - 1];
      double g = (diff > 0.0) ? diff : 0.0;
      double l = (diff < 0.0) ? -diff : 0.0;
      avg_g = (avg_g * (period - 1) + g) / period;
      avg_l = (avg_l * (period - 1) + l) / period;
    }
    if (avg_l <= 1e-12)
      return 100.0;
    double rs = avg_g / avg_l;
    return 100.0 - (100.0 / (1.0 + rs));
  }

  static TechnicalSnapshot compute_technical_snapshot_(const RealTimeAssetData &d,
                                                       const std::string &name,
                                                       const std::string &unit) {
    TechnicalSnapshot t;
    t.symbol = d.symbol;
    t.display_name = name;
    t.unit = unit;
    t.is_live = d.is_live;

    const auto &p = d.historical_prices;
    t.spot = (d.current_price > 0.0)
                 ? d.current_price
                 : (!p.empty() ? p.back() : 0.0);
    if (p.size() < 2 || t.spot <= 0.0)
      return t;

    auto ret_n = [&](size_t n) -> double {
      if (p.size() > n && p[p.size() - 1 - n] > 0.0) {
        return ((t.spot - p[p.size() - 1 - n]) / p[p.size() - 1 - n]) * 100.0;
      }
      return 0.0;
    };
    t.ret_5d_pct = ret_n(5);
    t.ret_20d_pct = ret_n(20);
    t.ret_60d_pct = ret_n(60);

    auto sma_n = [&](size_t n) -> double {
      if (p.size() < n)
        return t.spot;
      double s = std::accumulate(p.end() - static_cast<std::ptrdiff_t>(n), p.end(), 0.0);
      return s / static_cast<double>(n);
    };
    t.sma_20 = sma_n(std::min<size_t>(20, p.size()));
    t.sma_50 = sma_n(std::min<size_t>(50, p.size()));
    if (p.size() >= 180) {
      t.has_sma200 = true;
      t.sma_200 = sma_n(std::min<size_t>(200, p.size()));
    } else {
      t.sma_200 = sma_n(p.size());
    }

    t.rsi_14 = compute_rsi_(p, 14);
    t.vwap_22 = (d.vwap > 0.0) ? d.vwap : t.sma_20;
    if (t.vwap_22 > 0.0) {
      t.vwap_dist_pct = ((t.spot - t.vwap_22) / t.vwap_22) * 100.0;
    }

    size_t lb22 = std::min<size_t>(22, p.size());
    double m22 = sma_n(lb22);
    double v22 = 0.0;
    for (size_t i = p.size() - lb22; i < p.size(); ++i) {
      double diff = p[i] - m22;
      v22 += diff * diff;
    }
    double sd22 = std::sqrt(v22 / static_cast<double>(lb22));
    if (sd22 > 1e-6)
      t.vwap_z = (t.spot - t.vwap_22) / sd22;

    t.ann_vol_pct = d.annualized_volatility * 100.0;
    if (d.atr_14 > 0.0) {
      t.atr_14_pct = (d.atr_14 / t.spot) * 100.0;
    }

    if (p.size() >= 22) {
      std::vector<double> lr;
      lr.reserve(21);
      for (size_t i = p.size() - 21; i < p.size(); ++i) {
        if (p[i - 1] > 0.0 && p[i] > 0.0)
          lr.push_back(std::log(p[i] / p[i - 1]));
      }
      auto stdev_slice = [&](size_t start, size_t end) {
        size_t len = end - start;
        if (len < 2)
          return 0.0;
        double sum = std::accumulate(lr.begin() + start, lr.begin() + end, 0.0);
        double mu = sum / static_cast<double>(len);
        double acc = 0.0;
        for (size_t i = start; i < end; ++i)
          acc += (lr[i] - mu) * (lr[i] - mu);
        return std::sqrt(acc / static_cast<double>(len));
      };
      if (lr.size() >= 20) {
        double s20 = stdev_slice(0, lr.size());
        double s5 = stdev_slice(lr.size() - 5, lr.size());
        if (s20 > 1e-8)
          t.squeeze_ratio = s5 / s20;
      }
    }

    if (t.spot > t.sma_20 && t.sma_20 > t.sma_50 &&
        (!t.has_sma200 || t.spot > t.sma_200)) {
      t.trend_regime = I18n::tr("Güçlü Boğa Trendi", "Strong Bull Trend", "Starker Aufwärtstrend");
    } else if (t.spot > t.sma_50 && (!t.has_sma200 || t.spot > t.sma_200)) {
      t.trend_regime = I18n::tr("Pozitif Trend / Konsolidasyon", "Constructive Uptrend", "Konstruktiver Aufwärtstrend");
    } else if (t.spot < t.sma_20 && t.spot < t.sma_50) {
      t.trend_regime = I18n::tr("Zayıf / Düzeltme Baskısı", "Bearish / Correction", "Korrektur / Schwäche");
    } else {
      t.trend_regime = I18n::tr("Yatay / Seçici Denge", "Sideways / Selective", "Seitwärts / Selektiv");
    }

    size_t tail_n = std::min<size_t>(60, p.size());
    t.last_60_prices.assign(p.end() - static_cast<std::ptrdiff_t>(tail_n), p.end());
    return t;
  }

  static RegionalFundamentalAgg aggregate_fundamentals_(
      const std::vector<AlphaCandidate> &cands) {
    RegionalFundamentalAgg agg;
    agg.total_scanned = static_cast<int>(cands.size());
    if (cands.empty())
      return agg;

    std::vector<double> pes, evebs, pbs;
    double sum_margin = 0.0, sum_roe = 0.0, sum_ndeb = 0.0;
    int cnt_margin = 0, cnt_roe = 0, cnt_ndeb = 0;
    int vwap_up = 0, rsi_up = 0, ic_cnt = 0;
    double sum_flow = 0.0, sum_ic = 0.0;

    for (const auto &c : cands) {
      if (c.vwap_dist_pct > 0.0)
        ++vwap_up;
      if (c.rsi_14 >= 50.0)
        ++rsi_up;
      sum_flow += c.institutional_flow;
      if (c.predictability_samples >= 12) {
        sum_ic += c.predictability_ic;
        ++ic_cnt;
      }
      if (c.has_live_news && c.is_event_shock && c.sticky_sentiment_score <= -0.28)
        ++agg.neg_shock_count;

      const auto &f = c.fundamentals;
      if (!f.is_available)
        continue;
      ++agg.fund_count;
      if (!f.quality_grade.empty()) {
        if (f.quality_grade[0] == 'A')
          ++agg.grade_a_count;
        else if (f.quality_grade[0] == 'B')
          ++agg.grade_b_count;
        else
          ++agg.grade_c_count;
      }
      if (std::isfinite(f.pe_ratio) && f.pe_ratio > 0.5 && f.pe_ratio < 150.0)
        pes.push_back(f.pe_ratio);
      if (std::isfinite(f.pb_ratio) && f.pb_ratio > 0.05 && f.pb_ratio < 80.0)
        pbs.push_back(f.pb_ratio);
      if (f.has_ebitda && std::isfinite(f.ev_to_ebitda) &&
          f.ev_to_ebitda > 0.5 && f.ev_to_ebitda < 80.0) {
        evebs.push_back(f.ev_to_ebitda);
        ++agg.ebitda_count;
      }
      if (std::isfinite(f.ebitda_margin_pct)) {
        sum_margin += f.ebitda_margin_pct;
        ++cnt_margin;
      }
      if (std::isfinite(f.roe_pct)) {
        sum_roe += f.roe_pct;
        ++cnt_roe;
      }
      if (std::isfinite(f.net_debt_to_ebitda) &&
          std::abs(f.net_debt_to_ebitda) < 25.0) {
        sum_ndeb += f.net_debt_to_ebitda;
        ++cnt_ndeb;
      }
    }

    agg.median_pe = median_of_(std::move(pes));
    agg.median_ev_ebitda = median_of_(std::move(evebs));
    agg.median_pb = median_of_(std::move(pbs));
    if (cnt_margin > 0)
      agg.mean_ebitda_margin_pct = sum_margin / cnt_margin;
    if (cnt_roe > 0)
      agg.mean_roe_pct = sum_roe / cnt_roe;
    if (cnt_ndeb > 0)
      agg.mean_net_debt_ebitda = sum_ndeb / cnt_ndeb;
    agg.pct_above_vwap = 100.0 * static_cast<double>(vwap_up) / cands.size();
    agg.pct_rsi_bull = 100.0 * static_cast<double>(rsi_up) / cands.size();
    agg.mean_inst_flow = sum_flow / cands.size();
    if (ic_cnt > 0)
      agg.mean_time_ic = sum_ic / ic_cnt;
    return agg;
  }

  static double compute_fx_adjusted_return_(const RealTimeAssetData &eq,
                                            const RealTimeAssetData &fx,
                                            size_t lookback_days) {
    if (eq.day_keys.size() != eq.historical_prices.size() ||
        fx.day_keys.size() != fx.historical_prices.size() ||
        eq.day_keys.size() <= lookback_days) {
      return 0.0;
    }
    std::unordered_map<int64_t, double> fx_map;
    fx_map.reserve(fx.day_keys.size());
    for (size_t i = 0; i < fx.day_keys.size(); ++i) {
      if (fx.historical_prices[i] > 0.0)
        fx_map[fx.day_keys[i]] = fx.historical_prices[i];
    }
    std::vector<double> usd_idx;
    usd_idx.reserve(eq.day_keys.size());
    for (size_t i = 0; i < eq.day_keys.size(); ++i) {
      auto it = fx_map.find(eq.day_keys[i]);
      if (it != fx_map.end() && it->second > 0.0 && eq.historical_prices[i] > 0.0) {
        usd_idx.push_back(eq.historical_prices[i] / it->second);
      }
    }
    if (usd_idx.size() <= lookback_days)
      return 0.0;
    double old_u = usd_idx[usd_idx.size() - 1 - lookback_days];
    double new_u = usd_idx.back();
    return (old_u > 0.0) ? (((new_u - old_u) / old_u) * 100.0) : 0.0;
  }

  static std::pair<double, int>
  compute_date_aligned_corr_(const RealTimeAssetData &a,
                             const RealTimeAssetData &b, size_t max_bars) {
    if (a.day_keys.size() != a.historical_prices.size() ||
        b.day_keys.size() != b.historical_prices.size()) {
      return {0.0, 0};
    }
    std::unordered_map<int64_t, double> mb;
    mb.reserve(b.day_keys.size());
    for (size_t i = 0; i < b.day_keys.size(); ++i) {
      if (b.historical_prices[i] > 0.0)
        mb[b.day_keys[i]] = b.historical_prices[i];
    }
    std::vector<double> pa, pb;
    for (size_t i = 0; i < a.day_keys.size(); ++i) {
      auto it = mb.find(a.day_keys[i]);
      if (it != mb.end() && a.historical_prices[i] > 0.0) {
        pa.push_back(a.historical_prices[i]);
        pb.push_back(it->second);
      }
    }
    if (pa.size() < 25)
      return {0.0, 0};
    size_t start = (pa.size() > max_bars + 1) ? (pa.size() - max_bars - 1) : 1;
    std::vector<double> ra, rb;
    for (size_t i = start; i < pa.size(); ++i) {
      ra.push_back(std::log(pa[i] / pa[i - 1]));
      rb.push_back(std::log(pb[i] / pb[i - 1]));
    }
    int n = static_cast<int>(ra.size());
    if (n < 15)
      return {0.0, n};
    double ma = std::accumulate(ra.begin(), ra.end(), 0.0) / n;
    double mb_v = std::accumulate(rb.begin(), rb.end(), 0.0) / n;
    double num = 0.0, da = 0.0, db = 0.0;
    for (size_t i = 0; i < ra.size(); ++i) {
      double xa = ra[i] - ma;
      double xb = rb[i] - mb_v;
      num += xa * xb;
      da += xa * xa;
      db += xb * xb;
    }
    double den = std::sqrt(da * db);
    if (den <= 1e-12)
      return {0.0, n};
    return {std::clamp(num / den, -1.0, 1.0), n};
  }

  static const MacroDriverEntry *find_driver_(const GlobalMacroReportData &rep,
                                              std::string_view sym) {
    for (const auto &d : rep.drivers) {
      if (d.symbol == sym)
        return &d;
    }
    return nullptr;
  }

  static void synthesize_world_economy_outlook_(GlobalMacroReportData &rep) {
    auto *d_tnx = find_driver_(rep, "^TNX");
    auto *d_vix = find_driver_(rep, "^VIX");
    auto *d_dxy = find_driver_(rep, "DX-Y.NYB");
    auto *d_oil = find_driver_(rep, "BZ=F");
    auto *d_copper = find_driver_(rep, "HG=F");
    auto *d_gold = find_driver_(rep, "GC=F");

    double vix = d_vix ? d_vix->tech.spot : 20.0;
    double cu_20 = d_copper ? d_copper->tech.ret_20d_pct : 0.0;
    double oil_20 = d_oil ? d_oil->tech.ret_20d_pct : 0.0;
    double dxy_20 = d_dxy ? d_dxy->tech.ret_20d_pct : 0.0;
    double tnx_20 = d_tnx ? d_tnx->tech.ret_20d_pct : 0.0;
    double gold_20 = d_gold ? d_gold->tech.ret_20d_pct : 0.0;

    // Quantitative 5-pillar composite Global Macro Risk-On/Off score [-1.0, +1.0]
    double s_vol = std::clamp((20.0 - vix) / 10.0, -1.0, 0.8);
    double s_growth = std::clamp(cu_20 / 10.0, -0.8, 0.8);
    double s_curve = std::clamp(rep.yield_spread_bps / 120.0, -0.5, 0.5);
    double s_dollar = std::clamp(-dxy_20 / 4.0, -0.7, 0.7);
    double s_oil_shock = std::clamp(-oil_20 / 15.0, -0.7, 0.5);
    double avg_news = (rep.us.macro_sent.overall_sentiment_score +
                       rep.xetra.macro_sent.overall_sentiment_score +
                       rep.bist.macro_sent.overall_sentiment_score) /
                      3.0;

    rep.global_macro_score = std::clamp(
        0.25 * s_vol + 0.25 * s_growth + 0.15 * s_curve + 0.15 * s_dollar +
            0.10 * s_oil_shock + 0.10 * avg_news,
        -1.0, 1.0);

    if (rep.global_macro_score >= 0.22 && vix < 20.0) {
      rep.global_regime_title =
          I18n::tr("YAPICI BÜYÜME / RİSK-ON DÖNGÜSÜ",
                   "CONSTRUCTIVE EXPANSION / RISK-ON",
                   "KONSTRUKTIVES WACHSTUM / RISK-ON");
    } else if (oil_20 >= 6.0 && tnx_20 >= 3.0) {
      rep.global_regime_title =
          I18n::tr("ENERJİ & FAİZ BASKILI SEÇİCİ REJİM",
                   "INFLATION & RATE-TIGHTENING REGIME",
                   "INFLATIONS- & ZINSDRUCK-REGIME");
    } else if (rep.global_macro_score <= -0.22 || vix >= 24.0) {
      rep.global_regime_title =
          I18n::tr("DEFANSİF / YÜKSEK OYNAKLIK (RİSK-OFF)",
                   "DEFENSIVE / ELEVATED STRESS (RISK-OFF)",
                   "DEFENSIV / ERHÖHTER STRESS (RISK-OFF)");
    } else {
      rep.global_regime_title =
          I18n::tr("DENGELİ / MAKRO SEÇİCİ ROTASYON",
                   "BALANCED / SELECTIVE MACRO ROTATION",
                   "AUSGEWOGENE / SELEKTIVE MAKRO-ROTATION");
    }

    rep.global_1m_outlook = I18n::tr(
        std::format(
            "Önümüzdeki 1 ay (21 İş Günü) küresel fiyatlama beş ana eksene oturuyor: "
            "(1) ABD getiri eğrisi 10Y %{:.2f} / 3A %{:.2f} ({:+.0f} bps) eğimle fonlama maliyetini "
            "belirlerken; (2) VIX ({:.1f}) ve DXY ({:.1f}, 20G %{:+.1f}) gelişen piyasa likidite iştahını "
            "yönetiyor. (3) Bakır 20G %{:+.1f} sanayi ivmesini ölçerken, Brent Petrol 20G %{:+.1f} ve Altın "
            "20G %{:+.1f} enflasyon/jeopolitik risk primini yansıtıyor. Model sentezi önümüzdeki ay "
            "ABD ({:+.1f}%), XETRA ({:+.1f}%) ve BIST ({:+.1f}%) projeksiyonlarıyla seçici kalite ve "
            "bilanço gücü odaklı konumlanmayı işaret ediyor.",
            rep.yield_curve.ten_year_yield * 100.0,
            rep.yield_curve.three_month_yield * 100.0, rep.yield_spread_bps, vix,
            d_dxy ? d_dxy->tech.spot : 100.0, dxy_20, cu_20, oil_20, gold_20,
            rep.us.benchmark_forecast_1m.expected_return_pct,
            rep.xetra.benchmark_forecast_1m.expected_return_pct,
            rep.bist.benchmark_forecast_1m.expected_return_pct),
        std::format(
            "Over the next month (21 trading days), global markets hinge on five live drivers: "
            "US 10Y {:.2f}% / 3M {:.2f}% ({:+.0f} bps slope), VIX at {:.1f}, DXY at {:.1f} (20D {:+.1f}%), "
            "Copper 20D {:+.1f}% (industrial cycle), and Brent {:+.1f}% / Gold {:+.1f}% (inflation & safe-haven). "
            "Base-case 1M index projections point to {:+.1f}% for US SPY, {:+.1f}% for DAX, and {:+.1f}% for BIST 100.",
            rep.yield_curve.ten_year_yield * 100.0,
            rep.yield_curve.three_month_yield * 100.0, rep.yield_spread_bps, vix,
            d_dxy ? d_dxy->tech.spot : 100.0, dxy_20, cu_20, oil_20, gold_20,
            rep.us.benchmark_forecast_1m.expected_return_pct,
            rep.xetra.benchmark_forecast_1m.expected_return_pct,
            rep.bist.benchmark_forecast_1m.expected_return_pct),
        std::format(
            "Für die nächsten 21 Handelstage bestimmen fünf Live-Faktoren das Bild: US-10J {:.2f}% ({:+.0f} bps Curve), "
            "VIX {:.1f}, DXY 20T {:+.1f}%, Kupfer 20T {:+.1f}%, Brent {:+.1f}% & Gold {:+.1f}%. "
            "1M-Basisprognosen: US SPY {:+.1f}%, DAX {:+.1f}%, BIST {:+.1f}%.",
            rep.yield_curve.ten_year_yield * 100.0, rep.yield_spread_bps, vix,
            dxy_20, cu_20, oil_20, gold_20,
            rep.us.benchmark_forecast_1m.expected_return_pct,
            rep.xetra.benchmark_forecast_1m.expected_return_pct,
            rep.bist.benchmark_forecast_1m.expected_return_pct));

    // Identify strongest macro correlation pair dynamically for Page 2 callout
    double max_abs = 0.0;
    std::string best_r, best_c;
    double best_val = 0.0;
    for (size_t i = 0; i < rep.corr_matrix.size(); ++i) {
      for (size_t j = 0; j < rep.corr_matrix[i].size(); ++j) {
        if (std::abs(rep.corr_matrix[i][j]) > max_abs) {
          max_abs = std::abs(rep.corr_matrix[i][j]);
          best_val = rep.corr_matrix[i][j];
          best_r = rep.corr_row_labels[i];
          best_c = rep.corr_col_labels[j];
        }
      }
    }
    rep.correlation_summary = I18n::tr(
        std::format("Son 6 aylık tarih-hizalı günlük getiri korelasyonlarında en baskın "
                    "makro aktarım kanalı {} <-> {} (r = {:+.2f}) olarak ölçüldü. "
                    "Negatif VIX korelasyonu stres dönemlerinde hisse korelasyonlarının "
                    "arttığını, BIST tarafında ise USD/TRY ile Brent/DXY dinamiklerinin kritik kaldığını teyit eder.",
                    best_r, best_c, best_val),
        std::format("Over 6 months of date-aligned daily log returns, the strongest macro "
                    "transmission channel is {} <-> {} (r = {:+.2f}).",
                    best_r, best_c, best_val),
        std::format("Stärkster 6M-Makro-Transmissionskanal über tagesgleiche Renditen: {} <-> {} (r = {:+.2f}).",
                    best_r, best_c, best_val));
  }

  static void draw_section_title_(PdfDocument &pdf, double x, double y,
                                  double w, const std::string &title,
                                  PdfRgb accent = kGold) {
    pdf.fill_rect(x, y, 4.0, 15.0, accent);
    pdf.draw_text(x + 10.0, y + 11.5, title, 11.0, true, kNavy);
    pdf.draw_line(x, y + 18.0, x + w, y + 18.0, kBorder, 0.75);
  }

  static PdfRgb dir_color_(ForecastDirection d) {
    if (d == ForecastDirection::StrongBullish || d == ForecastDirection::Bullish)
      return kGreen;
    if (d == ForecastDirection::StrongBearish || d == ForecastDirection::Bearish)
      return kRed;
    return kSlateMid;
  }

  static std::string short_dir_(ForecastDirection d) {
    switch (d) {
    case ForecastDirection::StrongBullish:
      return I18n::tr("▲ GÜÇLÜ YUKARI", "▲ STRONG BULL", "▲ STARK BULL");
    case ForecastDirection::Bullish:
      return I18n::tr("▲ YUKARI", "▲ BULLISH", "▲ BULLISCH");
    case ForecastDirection::StrongBearish:
      return I18n::tr("▼ GÜÇLÜ AŞAĞI", "▼ STRONG BEAR", "▼ STARK BÄR");
    case ForecastDirection::Bearish:
      return I18n::tr("▼ AŞAĞI", "▼ BEARISH", "▼ BÄRISCH");
    default:
      return I18n::tr("◆ DENGELİ", "◆ NEUTRAL", "◆ NEUTRAL");
    }
  }

  static void draw_region_kpi_card_(PdfDocument &pdf, double x, double y,
                                    double w, double h,
                                    const RegionDeepReport &r) {
    pdf.fill_and_stroke_rect(x, y, w, h, kBgSoft, kBorder, 0.8);
    PdfRgb accent = dir_color_(r.benchmark_forecast_1m.direction);
    pdf.fill_rect(x, y, w, 4.0, accent);

    pdf.draw_text(x + 9.0, y + 18.0,
                  pdf.fit_text(r.region_title, w - 18.0, 8.8, true), 8.8, true,
                  kNavy);
    pdf.draw_text(x + 9.0, y + 34.0,
                  std::format("{:.2f} {}", r.benchmark_tech.spot,
                              r.benchmark_tech.unit),
                  13.0, true, kSlateDark);
    pdf.draw_text(x + w - 9.0, y + 34.0,
                  std::format("{} {:+.1f}%", I18n::tr("20G", "20D", "20T"),
                              r.benchmark_tech.ret_20d_pct),
                  8.5, true,
                  r.benchmark_tech.ret_20d_pct >= 0.0 ? kGreen : kRed,
                  PdfAlign::Right);

    pdf.draw_line(x + 8.0, y + 42.0, x + w - 8.0, y + 42.0, kBorder, 0.5);

    pdf.draw_text(x + 9.0, y + 56.0,
                  I18n::tr("1Ay Hedef:", "1M Target:", "1M Ziel:"),
                  7.8, false, kSlateMuted);
    pdf.draw_text(
        x + w - 9.0, y + 56.0,
        std::format("{:+.2f}%  [{:+.1f}..{:+.1f}%]",
                    r.benchmark_forecast_1m.expected_return_pct,
                    r.benchmark_forecast_1m.bear_return_pct,
                    r.benchmark_forecast_1m.bull_return_pct),
        7.8, true, accent, PdfAlign::Right);

    pdf.draw_text(x + 9.0, y + 70.0,
                  I18n::tr("Teknik:", "Tech:", "Technik:"), 7.8,
                  false, kSlateMuted);
    pdf.draw_text(x + w - 9.0, y + 70.0,
                  pdf.fit_text(std::format("RSI {:.1f} | {}",
                                           r.benchmark_tech.rsi_14,
                                           r.benchmark_tech.trend_regime),
                               w - 54.0, 7.3, false),
                  7.3, false, kSlateDark, PdfAlign::Right);

    pdf.draw_text(x + 9.0, y + 84.0,
                  I18n::tr("Temel:", "Valuation:", "Bewertung:"), 7.8,
                  false, kSlateMuted);
    pdf.draw_text(
        x + w - 9.0, y + 84.0,
        std::format("{} {:.1f}x | {} {:.1f}x",
                    I18n::tr("F/K", "P/E", "KGV"), r.fund_agg.median_pe,
                    I18n::tr("FD/F", "EV/EB", "EV/EB"), r.fund_agg.median_ev_ebitda),
        7.5, true, kSlateDark, PdfAlign::Right);

    pdf.draw_text(x + 9.0, y + 98.0,
                  I18n::tr("Makro Haber:", "Sentiment:", "Sentiment:"), 8.0,
                  false, kSlateMuted);
    PdfRgb sc = (r.macro_sent.overall_sentiment_score >= 0.1)
                    ? kGreen
                    : (r.macro_sent.overall_sentiment_score <= -0.1 ? kRed
                                                                    : kSlateMid);
    pdf.draw_text(x + w - 9.0, y + 98.0,
                  std::format("{:+.2f} ({}: {})",
                              r.macro_sent.overall_sentiment_score,
                              I18n::tr("Şok Veto", "Shock Veto", "Schock-Veto"),
                              r.fund_agg.neg_shock_count),
                  7.8, true, sc, PdfAlign::Right);
  }

  static void render_page1_executive_and_macro_(PdfDocument &pdf,
                                                const GlobalMacroReportData &rep) {
    constexpr double L = 34.0;
    constexpr double W = PdfDocument::kA4Width - 2.0 * L;

    // Cover Banner
    pdf.fill_rect(0.0, 0.0, PdfDocument::kA4Width, 74.0, kNavy);
    pdf.fill_rect(0.0, 74.0, PdfDocument::kA4Width, 3.0, kGold);
    pdf.draw_text(
        L, 27.0,
        I18n::tr("KÜRESEL PİYASALAR, DÜNYA EKONOMİSİ VE 1 AYLIK TAHMİN RAPORU",
                 "GLOBAL EQUITIES, WORLD MACRO & 1-MONTH OUTLOOK REPORT",
                 "GLOBALE AKTIENMÄRKTE, WELTWIRTSCHAFT & 1-MONATS-PROGNOSE"),
        14.5, true, kWhite);
    pdf.draw_text(
        L, 46.0,
        I18n::tr("ABD (Wall Street)  •  Almanya (XETRA DAX 40)  •  Türkiye (BIST 100)  •  Canlı Kantitatif Analiz",
                 "US (Wall Street)  •  Germany (XETRA DAX 40)  •  Turkey (BIST 100)  •  Live Quantitative Research",
                 "USA (Wall Street)  •  Deutschland (XETRA DAX 40)  •  Türkei (BIST 100)"),
        9.0, false, PdfRgb::hex(0xCBD5E1));
    pdf.draw_text(L, 62.0,
                  std::format("ModernQuantSuite C++20  │  {}", rep.generated_at),
                  8.0, false, PdfRgb::hex(0xFBBF24));

    // 3 Regional Cards
    double card_w = (W - 20.0) / 3.0;
    draw_region_kpi_card_(pdf, L, 88.0, card_w, 108.0, rep.us);
    draw_region_kpi_card_(pdf, L + card_w + 10.0, 88.0, card_w, 108.0, rep.xetra);
    draw_region_kpi_card_(pdf, L + 2.0 * (card_w + 10.0), 88.0, card_w, 108.0, rep.bist);

    // World Economy 1-Month Diagnosis Box
    draw_section_title_(
        pdf, L, 210.0, W,
        I18n::tr("DÜNYA EKONOMİSİ MAKRO REJİMİ & ÖNÜMÜZDEKİ 1 AY ÖNGÖRÜSÜ",
                 "WORLD ECONOMY MACRO REGIME & 1-MONTH STRATEGIC OUTLOOK",
                 "WELTWIRTSCHAFTLICHES MAKRO-REGIME & 1-MONATS-AUSBLICK"),
        kGold);

    PdfRgb reg_col = (rep.global_macro_score >= 0.20)
                         ? kGreen
                         : (rep.global_macro_score <= -0.20 ? kRed : kGold);
    pdf.fill_and_stroke_rect(L, 234.0, W, 122.0, kBgSoft, kBorder, 0.75);
    pdf.fill_rect(L, 234.0, 5.0, 122.0, reg_col);
    pdf.draw_text(L + 14.0, 249.0,
                  std::format("{}: {}  ({:+.2f} / 1.00)",
                              I18n::tr("Aktif Küresel Makro Rejim",
                                       "Active Global Regime",
                                       "Aktives Makro-Regime"),
                              rep.global_regime_title, rep.global_macro_score),
                  9.8, true, reg_col);
    pdf.draw_text(
        L + 14.0, 262.0,
        std::format("{}: {} %{:.2f}  │  {} %{:.2f}  │  {} %{:.2f}  │  {} %{:.2f}  ({}: {:+.0f} bps)",
                    I18n::tr("ABD Hazine Getiri Eğrisi", "US Treasury Curve", "US-Zinskurve"),
                    I18n::tr("3A", "3M", "3M"), rep.yield_curve.three_month_yield * 100.0,
                    I18n::tr("5Y", "5Y", "5J"), rep.yield_curve.five_year_yield * 100.0,
                    I18n::tr("10Y", "10Y", "10J"), rep.yield_curve.ten_year_yield * 100.0,
                    I18n::tr("30Y", "30Y", "30J"), rep.yield_curve.thirty_year_yield * 100.0,
                    I18n::tr("10Y-3A Eğim", "10Y-3M Slope", "10J-3M Spread"),
                    rep.yield_spread_bps),
        7.8, false, kSlateMid);
    pdf.draw_line(L + 14.0, 269.0, L + W - 10.0, 269.0, kBorder, 0.5);
    pdf.draw_wrapped_text(L + 14.0, 282.0, W - 24.0, rep.global_1m_outlook, 8.2,
                          11.3, false, kSlateDark);

    // Global Economy Key Variables Table
    draw_section_title_(
        pdf, L, 366.0, W,
        I18n::tr("DÜNYA EKONOMİSİNİ ETKİLEYEN DEĞİŞKENLER VE 1 AYLIK (21 İŞG) TAHMİNLERİ",
                 "GLOBAL ECONOMIC DRIVERS & 1-MONTH (21 TRADING DAYS) PROJECTIONS",
                 "GLOBALE WIRTSCHAFTSVARIABLEN & 1-MONATS-PROGNOSEN"),
        kBlue);

    double ty = 390.0;
    const double row_h = 34.0;
    // Table Header
    pdf.fill_rect(L, ty, W, 22.0, kSlateDark);
    const std::array<double, 6> cols = {L + 6.0, L + 132.0, L + 196.0,
                                        L + 262.0, L + 314.0, L + 412.0};
    pdf.draw_text(cols[0], ty + 14.5, I18n::tr("Değişken", "Variable", "Variable"), 7.8, true, kWhite);
    pdf.draw_text(cols[1], ty + 14.5, I18n::tr("Son Değer", "Spot", "Kurs"), 7.8, true, kWhite);
    pdf.draw_text(cols[2], ty + 14.5, I18n::tr("5G / 20G %", "5D / 20D %", "5T / 20T %"), 7.8, true, kWhite);
    pdf.draw_text(cols[3], ty + 14.5, "Vol (%)", 7.8, true, kWhite);
    pdf.draw_text(cols[4], ty + 14.5, I18n::tr("1 Ay Hedef (±1σ)", "1M Target (±1σ)", "1M Ziel"), 7.8, true, kWhite);
    pdf.draw_text(cols[5], ty + 14.5, I18n::tr("Küresel Rolü", "Macro Channel", "Makro-Rolle"), 7.8, true, kWhite);
    ty += 22.0;

    for (size_t i = 0; i < rep.drivers.size() && i < 10; ++i) {
      const auto &d = rep.drivers[i];
      pdf.fill_rect(L, ty, W, row_h, (i % 2 == 0) ? kBgSoft : kBgAlt);
      pdf.draw_line(L, ty + row_h, L + W, ty + row_h, kBorder, 0.4);

      pdf.draw_text(cols[0], ty + 13.0, pdf.fit_text(d.label, 122.0, 8.0, true), 8.0, true, kNavy);
      pdf.draw_text(cols[0], ty + 24.5, d.symbol, 7.0, false, kSlateMuted);

      int prec = (d.tech.spot < 20.0) ? 3 : 2;
      std::string spot_s = (prec == 3)
                               ? std::format("{:.3f} {}", d.tech.spot, d.unit)
                               : std::format("{:.2f} {}", d.tech.spot, d.unit);
      pdf.draw_text(cols[1], ty + 18.0, spot_s, 8.0, true, kSlateDark);

      PdfRgb rcol = (d.tech.ret_20d_pct >= 0.0) ? kGreen : kRed;
      pdf.draw_text(cols[2], ty + 13.0,
                    std::format("{} {:+.1f}%", I18n::tr("20G", "20D", "20T"),
                                d.tech.ret_20d_pct),
                    7.8, true, rcol);
      pdf.draw_text(cols[2], ty + 24.5,
                    std::format("{}  {:+.1f}%", I18n::tr("5G", "5D", "5T"),
                                d.tech.ret_5d_pct),
                    7.0, false, kSlateMuted);

      pdf.draw_text(cols[3], ty + 18.0,
                    std::format("{:.1f}%", d.tech.ann_vol_pct), 8.0, false,
                    kSlateDark);

      PdfRgb fcol = dir_color_(d.forecast_1m.direction);
      std::string tgt_s = (prec == 3)
                              ? std::format("{:.3f} ({:+.1f}%)", d.forecast_1m.target_price,
                                            d.forecast_1m.expected_return_pct)
                              : std::format("{:.2f} ({:+.1f}%)", d.forecast_1m.target_price,
                                            d.forecast_1m.expected_return_pct);
      pdf.draw_text(cols[4], ty + 13.0, tgt_s, 7.8, true, fcol);
      std::string band_s = (prec == 3)
                               ? std::format("{:.3f} - {:.3f}", d.forecast_1m.bear_support,
                                             d.forecast_1m.bull_target)
                               : std::format("{:.2f} - {:.2f}", d.forecast_1m.bear_support,
                                             d.forecast_1m.bull_target);
      pdf.draw_text(cols[4], ty + 24.5, band_s, 7.0, false, kSlateMuted);

      pdf.draw_wrapped_text(cols[5], ty + 12.5, (L + W) - cols[5] - 6.0,
                            d.role_note, 7.0, 9.5, false, kSlateDark);
      ty += row_h;
    }

    // Bottom Page 1 Footer
    pdf.draw_line(L, PdfDocument::kA4Height - 28.0, L + W,
                  PdfDocument::kA4Height - 28.0, kBorder, 0.5);
    pdf.draw_text(L, PdfDocument::kA4Height - 18.0,
                  I18n::tr("Yalnızca kantitatif araştırma amaçlıdır — yatırım tavsiyesi niteliği taşımaz.",
                           "For quantitative research only — not personalized investment advice.",
                           "Nur zu quantitativen Forschungszwecken — keine Anlageberatung."),
                  7.5, false, kSlateMuted);
  }

  static void draw_forecast_cone_panel_(PdfDocument &pdf, double x, double y,
                                        double w, double h,
                                        const RegionDeepReport &r) {
    pdf.fill_and_stroke_rect(x, y, w, h, kWhite, kBorder, 0.75);
    pdf.fill_rect(x, y, w, 20.0, kSlateDark);
    pdf.draw_text(x + 7.0, y + 13.5,
                  pdf.fit_text(r.benchmark_tech.display_name, w - 14.0, 8.0, true),
                  8.0, true, kWhite);

    const auto &hist = r.benchmark_tech.last_60_prices;
    if (hist.size() < 5 || r.benchmark_tech.spot <= 0.0)
      return;

    const double px0 = x + 28.0;
    const double px1 = x + w - 10.0;
    const double py0 = y + 28.0;
    const double py1 = y + h - 32.0;

    double lo = *std::min_element(hist.begin(), hist.end());
    double hi = *std::max_element(hist.begin(), hist.end());
    lo = std::min(lo, r.benchmark_forecast_1m.bear_support);
    hi = std::max(hi, r.benchmark_forecast_1m.bull_target);
    double pad = std::max((hi - lo) * 0.12, r.benchmark_tech.spot * 0.01);
    lo -= pad;
    hi += pad;
    double span = std::max(1e-6, hi - lo);

    auto map_y = [&](double val) {
      return py1 - ((val - lo) / span) * (py1 - py0);
    };

    // Grid lines
    for (int g = 0; g <= 3; ++g) {
      double gv = lo + (span * g) / 3.0;
      double gy = map_y(gv);
      pdf.draw_line(px0, gy, px1, gy, PdfRgb::hex(0xE2E8F0), 0.4, true);
      std::string lbl = (gv >= 1000.0) ? std::format("{:.0f}", gv)
                                       : std::format("{:.1f}", gv);
      pdf.draw_text(px0 - 3.0, gy + 2.5, lbl, 6.2, false, kSlateMuted,
                    PdfAlign::Right);
    }

    // 72% horizontal width for 60G history, 28% for 21G forecast cone
    const double split_x = px0 + (px1 - px0) * 0.72;
    pdf.draw_line(split_x, py0, split_x, py1, kBorder, 0.6, true);

    // Draw ±1σ Shaded Cone using square-root-time expansion
    const size_t n_fc = 10;
    std::vector<std::pair<double, double>> upper_pts, lower_pts, mid_pts;
    double s0 = r.benchmark_tech.spot;
    for (size_t k = 0; k <= n_fc; ++k) {
      double f = static_cast<double>(k) / n_fc;
      double fx = split_x + f * (px1 - split_x);
      double sqrt_f = std::sqrt(f);
      double v_mid = s0 + (r.benchmark_forecast_1m.target_price - s0) * f;
      double v_up = s0 + (r.benchmark_forecast_1m.bull_target - s0) * sqrt_f;
      double v_dn = s0 + (r.benchmark_forecast_1m.bear_support - s0) * sqrt_f;
      upper_pts.emplace_back(fx, map_y(v_up));
      lower_pts.emplace_back(fx, map_y(v_dn));
      mid_pts.emplace_back(fx, map_y(v_mid));
    }
    std::vector<std::pair<double, double>> poly = upper_pts;
    for (auto it = lower_pts.rbegin(); it != lower_pts.rend(); ++it)
      poly.push_back(*it);
    pdf.fill_polygon(poly, kBlueCone);
    pdf.draw_polyline(upper_pts, PdfRgb::hex(0x93C5FD), 0.7, true);
    pdf.draw_polyline(lower_pts, PdfRgb::hex(0x93C5FD), 0.7, true);
    pdf.draw_polyline(mid_pts, dir_color_(r.benchmark_forecast_1m.direction), 1.4,
                      true);

    // Historical price line
    std::vector<std::pair<double, double>> hpts;
    hpts.reserve(hist.size());
    for (size_t i = 0; i < hist.size(); ++i) {
      double hx = px0 + (static_cast<double>(i) / (hist.size() - 1)) *
                            (split_x - px0);
      hpts.emplace_back(hx, map_y(hist[i]));
    }
    pdf.draw_polyline(hpts, kNavy, 1.25, false);
    pdf.draw_circle(split_x, map_y(s0), 2.3, kGold);

    // Legend / Footer inside panel
    pdf.draw_text(px0, y + h - 18.0,
                  I18n::tr("60G Tarihi", "60D History", "60T Historie"), 6.5,
                  false, kSlateMuted);
    pdf.draw_text(px1, y + h - 18.0,
                  std::format("{} {:+.1f}% [{:.0f}..{:.0f}]",
                              I18n::tr("1Ay", "1M", "1M"),
                              r.benchmark_forecast_1m.expected_return_pct,
                              r.benchmark_forecast_1m.bear_support,
                              r.benchmark_forecast_1m.bull_target),
                  6.8, true, dir_color_(r.benchmark_forecast_1m.direction),
                  PdfAlign::Right);
    pdf.draw_text(px0, y + h - 8.0,
                  std::format("Spot: {:.2f} | {} %{:.0f} | VaR95 {:.1f}%", s0,
                              I18n::tr("Güven", "Conf", "Konf"),
                              r.benchmark_forecast_1m.regime_confidence * 100.0,
                              r.benchmark_forecast_1m.var_95_pct),
                  6.5, false, kSlateDark);
  }

  static void render_page2_charts_and_correlations_(
      PdfDocument &pdf, const GlobalMacroReportData &rep) {
    constexpr double L = 34.0;
    constexpr double W = PdfDocument::kA4Width - 2.0 * L;

    draw_section_title_(
        pdf, L, 46.0, W,
        I18n::tr("3 BÖLGE ENDEKSİ: SON 60 GÜN VE 1 AYLIK (±1σ) TAHMİN KONİLERİ",
                 "3 REGIONAL BENCHMARKS: 60D HISTORY & 1-MONTH (±1σ) FORECAST CONES",
                 "3 REGIONALE INDIZES: 60T HISTORIE & 1M (±1σ) PROGNOSEKEGEL"),
        kBlue);

    double cw = (W - 16.0) / 3.0;
    draw_forecast_cone_panel_(pdf, L, 70.0, cw, 168.0, rep.us);
    draw_forecast_cone_panel_(pdf, L + cw + 8.0, 70.0, cw, 168.0, rep.xetra);
    draw_forecast_cone_panel_(pdf, L + 2.0 * (cw + 8.0), 70.0, cw, 168.0, rep.bist);

    // Vector Horizontal Bar Chart: 20D Realized vs 1M Forecast
    draw_section_title_(
        pdf, L, 254.0, W,
        I18n::tr("20 GÜNLÜK REALİZE MOMENTUM (KOYU) vs 1 AYLIK MODEL BEKLENTİSİ (RENKLİ)",
                 "20-DAY REALIZED MOMENTUM (DARK) VS 1-MONTH MODEL FORECAST (COLORED)",
                 "20T REALISIERTES MOMENTUM VS 1M MODELLPROGNOSE"),
        kGold);

    struct BarItem {
      std::string name;
      double r20;
      double f1m;
    };
    std::vector<BarItem> bars = {
        {I18n::tr("ABD SPY", "US SPY", "US SPY"), rep.us.benchmark_tech.ret_20d_pct, rep.us.benchmark_forecast_1m.expected_return_pct},
        {"DAX 40", rep.xetra.benchmark_tech.ret_20d_pct, rep.xetra.benchmark_forecast_1m.expected_return_pct},
        {"BIST 100", rep.bist.benchmark_tech.ret_20d_pct, rep.bist.benchmark_forecast_1m.expected_return_pct},
    };
    for (const char *sym : {"BZ=F", "GC=F", "HG=F", "DX-Y.NYB", "TRY=X", "^VIX"}) {
      if (const auto *dr = find_driver_(rep, sym)) {
        bars.push_back({dr->symbol, dr->tech.ret_20d_pct, dr->forecast_1m.expected_return_pct});
      }
    }

    double by0 = 280.0;
    double bh_box = 165.0;
    pdf.fill_and_stroke_rect(L, by0, W, bh_box, kBgSoft, kBorder, 0.75);
    double max_abs = 5.0;
    for (const auto &b : bars) {
      max_abs = std::max({max_abs, std::abs(b.r20), std::abs(b.f1m)});
    }
    max_abs = std::min(30.0, std::ceil(max_abs * 1.15));

    const double label_x = L + 8.0;
    const double chart_x0 = L + 88.0;
    const double chart_x1 = L + W - 70.0;
    const double zero_x = 0.5 * (chart_x0 + chart_x1);
    const double half_span = 0.5 * (chart_x1 - chart_x0);

    pdf.draw_line(zero_x, by0 + 8.0, zero_x, by0 + bh_box - 10.0, kSlateMid, 0.7);
    double row_step = (bh_box - 18.0) / static_cast<double>(bars.size());
    for (size_t i = 0; i < bars.size(); ++i) {
      double ry = by0 + 10.0 + static_cast<double>(i) * row_step;
      pdf.draw_text(label_x, ry + 10.0, bars[i].name, 7.5, true, kNavy);

      auto draw_bar = [&](double val, double bar_y, double bar_h, PdfRgb c) {
        double frac = std::clamp(val / max_abs, -1.0, 1.0);
        double bw = std::abs(frac) * half_span;
        double bx = (frac >= 0.0) ? zero_x : (zero_x - bw);
        pdf.fill_rect(bx, bar_y, std::max(1.0, bw), bar_h, c);
      };
      draw_bar(bars[i].r20, ry + 1.5, 5.0, kSlateDark);
      draw_bar(bars[i].f1m, ry + 7.5, 5.0,
               bars[i].f1m >= 0.0 ? kGreen : kRed);

      pdf.draw_text(
          chart_x1 + 6.0, ry + 10.0,
          std::format("{:+.1f}% / {:+.1f}%", bars[i].r20, bars[i].f1m), 6.8,
          false, kSlateDark);
    }

    // Correlation Matrix
    draw_section_title_(
        pdf, L, 462.0, W,
        I18n::tr("6 AYLIK TARİH-HİZALI KÜRESEL MAKRO KORELASYON MATRİSİ",
                 "6-MONTH DATE-ALIGNED CROSS-ASSET MACRO CORRELATION MATRIX",
                 "6-MONATS-MAKRO-KORRELATIONSMATRIX"),
        kGreen);

    double cy = 486.0;
    const double col_label_w = 115.0;
    const double col_w =
        (W - col_label_w) / std::max<size_t>(1, rep.corr_col_labels.size());
    pdf.fill_rect(L, cy, W, 22.0, kSlateDark);
    pdf.draw_text(L + 6.0, cy + 14.5,
                  I18n::tr("Bölgesel Endeks", "Regional Benchmark", "Index"),
                  7.8, true, kWhite);
    for (size_t j = 0; j < rep.corr_col_labels.size(); ++j) {
      pdf.draw_text(L + col_label_w + (j + 0.5) * col_w, cy + 14.5,
                    rep.corr_col_labels[j], 7.5, true, kWhite, PdfAlign::Center);
    }
    cy += 22.0;

    const double crow_h = 26.0;
    for (size_t i = 0; i < rep.corr_row_labels.size(); ++i) {
      pdf.fill_rect(L, cy, col_label_w, crow_h,
                    (i % 2 == 0) ? kBgSoft : kBgAlt);
      pdf.draw_text(L + 6.0, cy + 16.0, rep.corr_row_labels[i], 7.6, true, kNavy);
      for (size_t j = 0; j < rep.corr_col_labels.size(); ++j) {
        double val = rep.corr_matrix[i][j];
        int n_obs = rep.corr_samples[i][j];
        PdfRgb cell_bg = (i % 2 == 0) ? kBgSoft : kBgAlt;
        PdfRgb txt_col = kSlateDark;
        if (val >= 0.30) {
          cell_bg = kGreenBg;
          txt_col = kGreen;
        } else if (val <= -0.30) {
          cell_bg = kRedBg;
          txt_col = kRed;
        }
        double cx = L + col_label_w + j * col_w;
        pdf.fill_rect(cx + 0.5, cy + 0.5, col_w - 1.0, crow_h - 1.0, cell_bg);
        pdf.draw_text(cx + col_w * 0.5, cy + 12.5, std::format("{:+.2f}", val),
                      8.0, true, txt_col, PdfAlign::Center);
        pdf.draw_text(cx + col_w * 0.5, cy + 22.0, std::format("n={}", n_obs),
                      6.0, false, kSlateMuted, PdfAlign::Center);
      }
      pdf.draw_line(L, cy + crow_h, L + W, cy + crow_h, kBorder, 0.4);
      cy += crow_h;
    }

    // Correlation Diagnostic Callout
    cy += 12.0;
    pdf.fill_and_stroke_rect(L, cy, W, 62.0, kBgSoft, kBorder, 0.75);
    pdf.fill_rect(L, cy, 4.0, 62.0, kBlue);
    pdf.draw_text(L + 12.0, cy + 15.0,
                  I18n::tr("Makro Aktarım ve Bulaşıcılık Yorumu",
                           "Macro Transmission & Spillover Synthesis",
                           "Makro-Transmissionssynthese"),
                  8.5, true, kNavy);
    pdf.draw_wrapped_text(L + 12.0, cy + 29.0, W - 22.0,
                          rep.correlation_summary, 7.8, 10.5, false, kSlateDark);
  }

  static void render_region_deep_block_(PdfDocument &pdf, double L, double top_y,
                                        double W, const RegionDeepReport &r,
                                        const std::string &flag_tag,
                                        const std::string &custom_macro_note) {
    draw_section_title_(pdf, L, top_y, W, flag_tag + "  " + r.region_title,
                        dir_color_(r.benchmark_forecast_1m.direction));
    double y = top_y + 24.0;

    // 2 Side-by-side Analytical Cards: Technical & 1M Forecast (Left) | Bottom-Up Fundamentals (Right)
    double half_w = (W - 10.0) * 0.5;
    pdf.fill_and_stroke_rect(L, y, half_w, 128.0, kBgSoft, kBorder, 0.7);
    pdf.fill_and_stroke_rect(L + half_w + 10.0, y, half_w, 128.0, kBgSoft, kBorder,
                             0.7);

    // Left: Index Technical & 1M Forecast
    pdf.draw_text(L + 8.0, y + 14.0,
                  I18n::tr("TEKNİK ANALİZ & 1 AYLIK (21 İŞG) TAHMİN",
                           "TECHNICAL ANALYSIS & 1M FORECAST",
                           "TECHNISCHE ANALYSE & 1M-PROGNOSE"),
                  8.2, true, kBlue);
    double ty = y + 28.0;
    const std::string lbl_20 = I18n::tr("20G", "20D", "20T");
    const std::string lbl_5 = I18n::tr("5G", "5D", "5T");
    const std::string lbl_60 = I18n::tr("60G", "60D", "60T");
    const std::string lbl_1m = I18n::tr("1Ay", "1M", "1M");
    for (size_t i = 0; i < r.sub_indices_tech.size() && i < 3; ++i) {
      const auto &tk = r.sub_indices_tech[i];
      const auto &fc = r.sub_indices_fc[i];
      pdf.draw_text(
          L + 8.0, ty,
          std::format("{} | Spot {:.2f} | {} {:+.1f}% | RSI {:.1f}",
                      tk.symbol, tk.spot, lbl_20, tk.ret_20d_pct, tk.rsi_14),
          7.3, true, kNavy);
      pdf.draw_text(
          L + 8.0, ty + 10.5,
          std::format("SMA20 {:.1f} | SMA50 {:.1f} | {} {:.2f} | {} {:+.1f}% [{:.1f}..{:.1f}]",
                      tk.sma_20, tk.sma_50,
                      I18n::tr("Sıkışma", "Squeeze", "Squeeze"), tk.squeeze_ratio,
                      lbl_1m, fc.expected_return_pct, fc.bear_support, fc.bull_target),
          6.8, false, dir_color_(fc.direction));
      ty += 25.0;
    }
    if (r.sub_indices_tech.size() == 1) {
      const auto &tk = r.benchmark_tech;
      const auto &fc = r.benchmark_forecast_1m;
      pdf.draw_text(
          L + 8.0, ty + 2.0,
          std::format("{} {:+.1f}% | {} {:+.1f}% | 22{} VWAP {:.1f} (Z {:+.2f}) | Vol %{:.1f}",
                      lbl_5, tk.ret_5d_pct, lbl_60, tk.ret_60d_pct,
                      I18n::tr("G", "D", "T"), tk.vwap_22, tk.vwap_z,
                      tk.ann_vol_pct),
          7.0, false, kSlateDark);
      pdf.draw_text(
          L + 8.0, ty + 15.0,
          std::format("{}: %{:.0f} | RSI>50 %{:.0f} | VaR95 {:.1f}%",
                      I18n::tr("VWAP Üstü", "Above VWAP", "Über VWAP"),
                      r.fund_agg.pct_above_vwap, r.fund_agg.pct_rsi_bull,
                      fc.var_95_pct),
          7.0, false, kSlateMid);
    }
    if (!custom_macro_note.empty()) {
      pdf.draw_wrapped_text(L + 8.0, y + 106.0, half_w - 16.0, custom_macro_note,
                            6.8, 9.0, false, kSlateDark);
    }

    // Right: Fundamental Analysis & Sentiment
    double rx = L + half_w + 10.0;
    pdf.draw_text(rx + 8.0, y + 14.0,
                  I18n::tr("TEMEL ANALİZ (TTM BİLANÇO) & MAKRO SENTİMEL",
                           "FUNDAMENTAL VALUATION (TTM) & SENTIMENT",
                           "FUNDAMENTALANALYSE (TTM) & SENTIMENT"),
                  8.2, true, kGold);
    const auto &fa = r.fund_agg;
    pdf.draw_text(rx + 8.0, y + 30.0,
                  std::format("{}: {:.1f}x   │   {}: {:.1f}x",
                              I18n::tr("Medyan F/K", "Median P/E", "Median KGV"),
                              fa.median_pe,
                              I18n::tr("Medyan FD/FAVÖK", "Median EV/EBITDA", "Median EV/EBITDA"),
                              fa.median_ev_ebitda),
                  7.5, true, kNavy);
    pdf.draw_text(rx + 8.0, y + 43.0,
                  std::format("{}: {:.2f}x   │   {}: %{:.1f}",
                              I18n::tr("Medyan PD/DD", "Median P/B", "Median KBV"),
                              fa.median_pb,
                              I18n::tr("Ort. FAVÖK Marjı", "Avg EBITDA Mgn", "Ø EBITDA-Marge"),
                              fa.mean_ebitda_margin_pct),
                  7.3, false, kSlateDark);
    pdf.draw_text(rx + 8.0, y + 56.0,
                  std::format("{}: %{:.1f}   │   {}: {:.2f}x",
                              I18n::tr("Ort. ROE", "Avg ROE", "Ø ROE"),
                              fa.mean_roe_pct,
                              I18n::tr("Net Borç/FAVÖK", "Net Debt/EBITDA", "Nettoversch./EBITDA"),
                              fa.mean_net_debt_ebitda),
                  7.3, false, kSlateDark);
    pdf.draw_text(rx + 8.0, y + 69.0,
                  std::format("{}: {} A+/A  │  {} B  │  {} C  ({}: {:+.3f})",
                              I18n::tr("Kalite", "Quality", "Qualität"),
                              fa.grade_a_count, fa.grade_b_count,
                              fa.grade_c_count,
                              I18n::tr("Time-IC Ort", "Avg Time-IC", "Ø Time-IC"),
                              fa.mean_time_ic),
                  7.1, false, kSlateMid);
    pdf.draw_wrapped_text(rx + 8.0, y + 83.5, half_w - 16.0,
                          r.macro_sent.macro_synthesis, 6.7, 8.8, false,
                          kSlateDark);

    // Top Asymmetric Picks & Event-Shock Warnings Table
    y += 136.0;
    pdf.fill_rect(L, y, W, 17.0, kSlateDark);
    const std::array<double, 7> tc = {L + 5.0,   L + 64.0,  L + 214.0, L + 276.0,
                                      L + 330.0, L + 388.0, L + 454.0};
    pdf.draw_text(tc[0], y + 11.5, I18n::tr("Sembol", "Symbol", "Symbol"), 7.2, true, kWhite);
    pdf.draw_text(tc[1], y + 11.5, I18n::tr("Quant Sınıfı", "Quant Class", "Klasse"), 7.2, true, kWhite);
    pdf.draw_text(tc[2], y + 11.5, std::format("Spot / {}", lbl_20), 7.2, true, kWhite);
    pdf.draw_text(tc[3], y + 11.5, "Net R:R", 7.2, true, kWhite);
    pdf.draw_text(tc[4], y + 11.5, I18n::tr("FD/FAVÖK", "EV/EBITDA", "EV/EBITDA"), 7.2, true, kWhite);
    pdf.draw_text(tc[5], y + 11.5, "Sticky Sent", 7.2, true, kWhite);
    pdf.draw_text(tc[6], y + 11.5, "Time-IC", 7.2, true, kWhite);
    y += 17.0;

    // Show top 4 opportunities + up to 2 Crowded/Shock warnings so risk exclusions are visible
    std::vector<const AlphaCandidate *> selected;
    int top_n = 0, warn_n = 0;
    for (const auto &c : r.candidates) {
      if (c.category != AlphaCategory::CrowdedTradeWarning && top_n < 4) {
        selected.push_back(&c);
        ++top_n;
      }
    }
    for (auto it = r.candidates.rbegin(); it != r.candidates.rend() && warn_n < 2;
         ++it) {
      if (it->category == AlphaCategory::CrowdedTradeWarning) {
        selected.push_back(&(*it));
        ++warn_n;
      }
    }

    for (size_t i = 0; i < selected.size() && i < 5; ++i) {
      const auto *c = selected[i];
      bool is_warn = (c->category == AlphaCategory::CrowdedTradeWarning);
      bool is_shock =
          (c->has_live_news &&
           ((c->is_event_shock && c->sticky_sentiment_score <= -0.28) ||
            (c->sticky_negative_state <= -0.32 && c->sentiment_score <= -0.28) ||
            c->sticky_sentiment_score <= -0.45));
      PdfRgb row_bg = is_shock ? kRedBg : ((i % 2 == 0) ? kBgSoft : kBgAlt);
      pdf.fill_rect(L, y, W, 17.5, row_bg);
      pdf.draw_line(L, y + 17.5, L + W, y + 17.5, kBorder, 0.35);

      pdf.draw_text(tc[0], y + 11.8, c->symbol, 7.2, true,
                    is_warn ? kRed : kNavy);
      pdf.draw_text(tc[1], y + 11.8,
                    pdf.fit_text(c->category_label, 142.0, 6.7, is_shock), 6.7,
                    is_shock, is_shock ? kRed : (is_warn ? kGold : kSlateDark));
      pdf.draw_text(tc[2], y + 11.8,
                    std::format("{:.2f} ({:+.1f}%)", c->current_price,
                                c->return_30d_pct),
                    6.8, false, kSlateDark);
      pdf.draw_text(tc[3], y + 11.8,
                    std::format("1:{:.2f}", c->net_asymmetry_ratio), 7.0, true,
                    c->net_asymmetry_ratio >= 2.0 ? kGreen : kSlateDark);
      std::string ev_s = (c->fundamentals.has_ebitda &&
                          std::isfinite(c->fundamentals.ev_to_ebitda))
                             ? std::format("{:.1f}x ({})", c->fundamentals.ev_to_ebitda,
                                           c->fundamentals.quality_grade)
                             : std::format("{} {:.1f}x",
                                           I18n::tr("F/K", "P/E", "KGV"),
                                           std::isfinite(c->fundamentals.pe_ratio)
                                               ? c->fundamentals.pe_ratio
                                               : 0.0);
      pdf.draw_text(tc[4], y + 11.8, ev_s, 6.8, false, kSlateDark);
      PdfRgb scol = (c->sticky_sentiment_score >= 0.15)
                        ? kGreen
                        : (c->sticky_sentiment_score <= -0.20 ? kRed : kSlateMid);
      pdf.draw_text(tc[5], y + 11.8,
                    c->has_live_news
                        ? std::format("{:+.2f}{}", c->sticky_sentiment_score,
                                      is_shock ? I18n::tr(" [ŞOK]", " [SHOCK]", " [SCHOCK]") : "")
                        : I18n::tr("Nötr", "Neutral", "Neutral"),
                    6.8, is_shock, scol);
      pdf.draw_text(tc[6], y + 11.8,
                    std::format("{:+.3f} (%{:.0f})", c->predictability_ic,
                                c->predictability_hit_rate * 100.0),
                    6.8, false,
                    c->predictability_ic >= 0.15 ? kGreen : kSlateDark);
      y += 17.5;
    }
  }

  static void render_page3_us_and_xetra_(PdfDocument &pdf,
                                         const GlobalMacroReportData &rep) {
    constexpr double L = 34.0;
    constexpr double W = PdfDocument::kA4Width - 2.0 * L;

    render_region_deep_block_(
        pdf, L, 48.0, W, rep.us, I18n::tr("[ABD / US]", "[US / WALL ST]", "[USA / WALL ST]"),
        std::format("{}: {:.2f} - {:.2f} $ │ {}: %{:.0f} │ {}",
                    I18n::tr("1 Ay Hedef Bandı", "1M Target Range", "1M-Zielband"),
                    rep.us.benchmark_forecast_1m.bear_support,
                    rep.us.benchmark_forecast_1m.bull_target,
                    I18n::tr("Güven", "Conf", "Konf"),
                    rep.us.benchmark_forecast_1m.regime_confidence * 100.0,
                    short_dir_(rep.us.benchmark_forecast_1m.direction)));

    render_region_deep_block_(
        pdf, L, 412.0, W, rep.xetra, I18n::tr("[ALMANYA / XETRA]", "[GERMANY / XETRA]", "[DEUTSCHLAND / XETRA]"),
        std::format("{}: {:.0f} - {:.0f} pt │ {}: %{:.0f} │ {}",
                    I18n::tr("1 Ay DAX Hedef Bandı", "1M DAX Target Range", "1M-DAX-Zielband"),
                    rep.xetra.benchmark_forecast_1m.bear_support,
                    rep.xetra.benchmark_forecast_1m.bull_target,
                    I18n::tr("Güven", "Conf", "Konf"),
                    rep.xetra.benchmark_forecast_1m.regime_confidence * 100.0,
                    short_dir_(rep.xetra.benchmark_forecast_1m.direction)));
  }

  static void render_page4_bist_and_synthesis_(PdfDocument &pdf,
                                               const GlobalMacroReportData &rep) {
    constexpr double L = 34.0;
    constexpr double W = PdfDocument::kA4Width - 2.0 * L;

    const auto *d_usdtry = find_driver_(rep, "TRY=X");
    std::string bist_fx_note = std::format(
        "{}: {} {:+.1f}% │ {} {:+.1f}% │ USD/TRY {:.2f} ({} {:.2f})",
        I18n::tr("USD Bazlı BIST 100", "BIST 100 in USD", "BIST 100 in USD"),
        I18n::tr("20G", "20D", "20T"), rep.bist_usd_ret20d_pct,
        I18n::tr("60G", "60D", "60T"), rep.bist_usd_ret60d_pct,
        d_usdtry ? d_usdtry->tech.spot : 0.0,
        I18n::tr("1Ay Hedef", "1M Target", "1M-Ziel"),
        d_usdtry ? d_usdtry->forecast_1m.target_price : 0.0);

    render_region_deep_block_(
        pdf, L, 48.0, W, rep.bist,
        I18n::tr("[TÜRKİYE / BIST]", "[TURKEY / BIST]", "[TÜRKEI / BIST]"),
        bist_fx_note);

    // Olay Şoku / Sürü Riski & 1 Aylık Karşılaştırmalı Stratejik Özet
    double y = 346.0;
    draw_section_title_(
        pdf, L, y, W,
        I18n::tr("HABER / OLAY ŞOKU VETOSU ALAN HİSSELER VE RİSK YÖNETİMİ",
                 "ACTIVE EVENT-SHOCK VETOES & TAIL-RISK WARNINGS",
                 "EREIGNISSCHOCK-VETOS & RISIKOWARNUNGEN"),
        kRed);
    y += 24.0;

    std::vector<const AlphaCandidate *> shocks;
    for (const auto *reg : {&rep.bist, &rep.xetra, &rep.us}) {
      for (const auto &c : reg->candidates) {
        if (c.has_live_news &&
            ((c.is_event_shock && c.sticky_sentiment_score <= -0.28) ||
             (c.sticky_negative_state <= -0.32 && c.sentiment_score <= -0.28) ||
             c.sticky_sentiment_score <= -0.45)) {
          shocks.push_back(&c);
        }
      }
    }
    std::sort(shocks.begin(), shocks.end(),
              [](const AlphaCandidate *a, const AlphaCandidate *b) {
                return a->sticky_sentiment_score < b->sticky_sentiment_score;
              });

    if (shocks.empty()) {
      pdf.fill_and_stroke_rect(L, y, W, 34.0, kGreenBg, kBorder, 0.6);
      pdf.draw_text(
          L + 10.0, y + 20.5,
          I18n::tr("Şu anda taranan BIST, XETRA ve ABD evrenlerinde aktif negatif olay şoku vetosu tespit edilmedi.",
                   "No active negative event-shock vetoes triggered across BIST, XETRA, or US universes.",
                   "Keine negativen Ereignisschock-Vetos in BIST, XETRA und USA aktiv."),
          8.0, true, kGreen);
      y += 46.0;
    } else {
      const size_t show_n = std::min<size_t>(6, shocks.size());
      double box_h = 14.0 + static_cast<double>(show_n) * 19.5;
      pdf.fill_and_stroke_rect(L, y, W, box_h, kRedBg, kRed, 0.7);
      double sy = y + 13.0;
      const std::string spot_sent_lbl = I18n::tr("Anlık", "Spot", "Aktuell");
      for (size_t i = 0; i < show_n; ++i) {
        const auto *s = shocks[i];
        bool bold_row = (i < 3);
        pdf.draw_text(
            L + 10.0, sy,
            pdf.fit_text(
                std::format("! {} ({}): Sticky {:+.2f} │ {} {:+.2f} │ {}",
                            s->symbol, s->market, s->sticky_sentiment_score,
                            spot_sent_lbl, s->sentiment_score, s->quant_diagnosis),
                W - 20.0, 7.1, bold_row),
            7.1, bold_row, kRed);
        sy += 19.5;
      }
      y += box_h + 12.0;
    }

    // 1-Month Strategic Comparison Matrix across US / XETRA / BIST
    draw_section_title_(
        pdf, L, y, W,
        I18n::tr("ÖNÜMÜZDEKİ 1 AY (21 İŞ GÜNÜ) KARŞILAŞTIRMALI BÖLGESEL TAHMİN MATRİSİ",
                 "COMPARATIVE 1-MONTH (21 TRADING DAYS) REGIONAL FORECAST MATRIX",
                 "VERGLEICHENDE 1-MONATS-PROGNOSEMATRIX"),
        kGold);
    y += 24.0;

    pdf.fill_rect(L, y, W, 20.0, kNavy);
    const std::array<double, 6> sc = {L + 6.0,   L + 118.0, L + 186.0,
                                      L + 278.0, L + 362.0, L + 448.0};
    pdf.draw_text(sc[0], y + 13.5, I18n::tr("Piyasa / Endeks", "Market / Index", "Markt / Index"), 7.4, true, kWhite);
    pdf.draw_text(sc[1], y + 13.5, I18n::tr("1Ay Beklenen", "1M Expected", "1M Erwartet"), 7.4, true, kWhite);
    pdf.draw_text(sc[2], y + 13.5, I18n::tr("±1σ Hedef Bandı", "±1σ Target Band", "±1σ Zielband"), 7.4, true, kWhite);
    pdf.draw_text(sc[3], y + 13.5, "95% VaR / CVaR", 7.4, true, kWhite);
    pdf.draw_text(sc[4], y + 13.5, I18n::tr("Med F/K | FD/FAV", "Med P/E | EV/EB", "Med KGV | EV/EB"), 7.4, true, kWhite);
    pdf.draw_text(sc[5], y + 13.5, I18n::tr("Yön Sinyali", "Direction", "Richtung"), 7.4, true, kWhite);
    y += 20.0;

    for (size_t i = 0; i < 3; ++i) {
      const auto *r = (i == 0) ? &rep.us : ((i == 1) ? &rep.xetra : &rep.bist);
      pdf.fill_rect(L, y, W, 24.0, (i % 2 == 0) ? kBgSoft : kBgAlt);
      pdf.draw_line(L, y + 24.0, L + W, y + 24.0, kBorder, 0.4);
      const auto &fc = r->benchmark_forecast_1m;
      pdf.draw_text(sc[0], y + 15.0, r->benchmark_tech.display_name, 7.4, true, kNavy);
      pdf.draw_text(sc[1], y + 15.0,
                    std::format("{:+.2f}%", fc.expected_return_pct), 7.8, true,
                    dir_color_(fc.direction));
      pdf.draw_text(sc[2], y + 15.0,
                    std::format("{:.1f} .. {:.1f}", fc.bear_support,
                                fc.bull_target),
                    7.2, false, kSlateDark);
      pdf.draw_text(sc[3], y + 15.0,
                    std::format("{:.1f}% / {:.1f}%", fc.var_95_pct,
                                fc.cvar_95_pct),
                    7.2, false, kRed);
      pdf.draw_text(sc[4], y + 15.0,
                    std::format("{:.1f}x | {:.1f}x", r->fund_agg.median_pe,
                                r->fund_agg.median_ev_ebitda),
                    7.2, false, kSlateDark);
      pdf.draw_text(sc[5], y + 15.0, short_dir_(fc.direction), 7.0, true,
                    dir_color_(fc.direction));
      y += 24.0;
    }

    // Methodology Box at Bottom of Page 4
    y += 14.0;
    double rem_h = std::max(60.0, PdfDocument::kA4Height - 36.0 - y);
    pdf.fill_and_stroke_rect(L, y, W, rem_h, kBgSoft, kBorder, 0.7);
    pdf.draw_text(L + 10.0, y + 14.0,
                  I18n::tr("Metodoloji & Kantitatif Doğrulama Notu",
                           "Methodology & Quantitative Risk Disclosure",
                           "Methodik & Risikohinweis"),
                  8.2, true, kSlateDark);
    pdf.draw_wrapped_text(
        L + 10.0, y + 27.0, W - 20.0,
        I18n::tr(
            "Bu PDF raporu C++20 ModernQuantSuite tarafından harici Python/tarayıcı bağımlılığı olmadan "
            "canlı verilerden üretilmiştir. Tüm 1 aylık (21 İş Günü) tahminler Grinold-Kahn bilgi oranı "
            "çerçevesi, Kaufman rejim güveni, Heston/Jump asimetrik kuyruk çarpanı ve 6 aylık tarih-hizalı "
            "korelasyonlar üzerinden hesaplanmıştır. BIST şirket çarpanları TMS-29 kayan 12-aylık (TTM) "
            "kâr/FAVÖK verileri ve canlı FX dönüşümüyle hizalanmıştır; MSCI/endeks çıkarılma ve şirket şokları "
            "çift-akışlı yapışkan haber belleği ile filtrelenmiştir.",
            "Generated natively by C++20 ModernQuantSuite from live Yahoo Finance, Treasury yield curve, "
            "and Google News RSS feeds using Grinold-Kahn horizon scaling, Kaufman confidence gating, asymmetric "
            "tail risk cones, and dual-stream sticky sentiment event-shock filtering.",
            "Nativ generiert durch ModernQuantSuite C++20 aus Live-Marktdaten mit Grinold-Kahn-Prognose, "
            "asymmetrischen Tail-Risikokegeln und Dual-Stream-News-Sentiment."),
        7.1, 10.0, false, kSlateMid);
  }
};

} // namespace quant
