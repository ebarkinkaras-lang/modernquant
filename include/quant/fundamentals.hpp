#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "i18n.hpp"
#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <format>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace quant {

enum class AssetClassType {
    Equity,            // Sanayi, Teknoloji, Havacılık, Perakende, Enerji, Holding
    BankOrFinancial,   // Banka, Sigorta, Finansal Kurum (FAVÖK uygulanmaz; ROE, PD/DD, F/K esas alınır)
    CommodityOrFuture, // Altın, Gümüş, Petrol, Vadeli Emtia (Bilanço/FAVÖK yoktur)
    ETFOrFund,         // Endeks / Tahvil Fonları (SPY, QQQ vb.)
    CryptoOrFX,        // Kripto Varlıklar & Döviz Kurları
    IndexOrOther       // Borsa Endeksleri (^GSPC, XU100.IS vb.)
};

struct RawFundamentalInputs {
    std::string symbol;
    std::string company_name;
    std::string sector;
    std::string industry;
    std::string trading_currency{"USD"};
    std::string financial_currency{"USD"};
    std::string latest_report_date;
    AssetClassType asset_class{AssetClassType::Equity};

    double spot_price{0.0};
    double fx_fin_to_trade{1.0}; // 1 unit of financial_currency in trading_currency (e.g. USD->TRY = 50.13)

    double shares_outstanding{0.0};
    double fallback_market_cap_trade{0.0};

    bool has_ebitda{false};
    double ebitda_ttm{0.0};
    bool has_ebitda_yoy{false};
    double ebitda_yoy_growth_pct{0.0};

    bool has_revenue{false};
    double revenue_ttm{0.0};
    bool has_revenue_yoy{false};
    double revenue_yoy_growth_pct{0.0};

    bool has_operating_income{false};
    double operating_income_ttm{0.0};

    bool has_net_income{false};
    double net_income_ttm{0.0};
    bool has_net_income_yoy{false};
    double net_income_yoy_growth_pct{0.0};

    bool has_fcf{false};
    double free_cash_flow_ttm{0.0};
    bool has_ocf{false};
    double operating_cash_flow_ttm{0.0};

    bool has_debt{false};
    double total_debt{0.0};
    bool has_cash{false};
    double cash_and_st_investments{0.0};

    bool has_equity{false};
    double stockholders_equity{0.0};
    bool has_assets{false};
    double total_assets{0.0};

    double forward_pe{std::numeric_limits<double>::quiet_NaN()};
    double dividend_yield_pct{std::numeric_limits<double>::quiet_NaN()};
    double analyst_target_mean{std::numeric_limits<double>::quiet_NaN()};
    std::string recommendation_key;
};

struct FundamentalProfile {
    std::string symbol;
    std::string company_name;
    std::string sector;
    std::string industry;
    std::string trading_currency{"USD"};
    std::string trading_currency_display{"$"};
    std::string financial_currency{"USD"};
    std::string latest_report_date;
    AssetClassType asset_class{AssetClassType::Equity};

    bool is_available{false};
    bool has_ebitda{false};
    double fx_fin_to_trade{1.0};

    // Absolute figures (in financial_currency & trading_currency)
    double market_cap_fin{0.0};
    double market_cap_trade{0.0};
    double enterprise_value_fin{0.0};
    double enterprise_value_trade{0.0};
    double ebitda_ttm_fin{0.0};
    double ebitda_ttm_trade{0.0};
    double revenue_ttm_fin{0.0};
    double net_income_ttm_fin{0.0};
    double free_cash_flow_ttm_fin{0.0};
    double net_debt_fin{0.0};
    double net_debt_trade{0.0};
    double stockholders_equity_fin{0.0};

    // Key Institutional Multiples & Margins (NaN if not applicable / unavailable)
    double ev_to_ebitda{std::numeric_limits<double>::quiet_NaN()};       // FD / FAVÖK
    double net_debt_to_ebitda{std::numeric_limits<double>::quiet_NaN()}; // Net Borç / FAVÖK (<0 => Net Nakit)
    double ebitda_margin_pct{std::numeric_limits<double>::quiet_NaN()};  // FAVÖK Marjı (%)
    double operating_margin_pct{std::numeric_limits<double>::quiet_NaN()};
    double net_margin_pct{std::numeric_limits<double>::quiet_NaN()};
    double ebitda_yoy_growth_pct{std::numeric_limits<double>::quiet_NaN()};
    double revenue_yoy_growth_pct{std::numeric_limits<double>::quiet_NaN()};
    double net_income_yoy_growth_pct{std::numeric_limits<double>::quiet_NaN()};

    double pe_ratio{std::numeric_limits<double>::quiet_NaN()};           // F/K (Trailing P/E)
    double forward_pe{std::numeric_limits<double>::quiet_NaN()};         // İleri F/K
    double pb_ratio{std::numeric_limits<double>::quiet_NaN()};           // PD / DD (Price / Book)
    double roe_pct{std::numeric_limits<double>::quiet_NaN()};            // Özkaynak Kârlılığı (%)
    double roa_pct{std::numeric_limits<double>::quiet_NaN()};            // Aktif Kârlılığı (%)
    double fcf_yield_pct{std::numeric_limits<double>::quiet_NaN()};      // Serbest Nakit Akışı Verimi (%)
    double dividend_yield_pct{std::numeric_limits<double>::quiet_NaN()}; // Temettü Verimi (%)
    double analyst_target_mean{std::numeric_limits<double>::quiet_NaN()};

    // Composite Fundamental Quality & Valuation Score [-1.0, +1.0]
    double fundamental_score{0.0};
    std::string quality_grade{"N/A"};
    std::string summary_line;
    std::string diagnosis;
    std::string accounting_note;
};

class FundamentalEngine {
public:
    static FundamentalProfile fetch_fundamentals(LiveMarketDataClient& client,
                                                 const RealTimeAssetData& asset) {
        static std::unordered_map<std::string, FundamentalProfile> cache;
        std::string key = asset.symbol;
        if (auto it = cache.find(key); it != cache.end()) {
            return it->second;
        }

        AssetClassType cls = classify_asset_type(asset);
        if (cls != AssetClassType::Equity && cls != AssetClassType::BankOrFinancial) {
            FundamentalProfile non_eq = build_non_equity_profile(asset, cls);
            cache[key] = non_eq;
            return non_eq;
        }

        RawFundamentalInputs raw;
        raw.symbol = asset.symbol;
        raw.company_name = asset.long_name.empty() ? asset.symbol : asset.long_name;
        raw.trading_currency = asset.currency_code;
        raw.financial_currency = asset.currency_code;
        raw.spot_price = asset.current_price;
        raw.asset_class = cls;

        // 1. Primary Source: Yahoo fundamentals-timeseries v1 (explicit currencyCode per item, no crumb required)
        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t period2 = now_sec + 86400LL * 30;
        int64_t period1 = now_sec - 86400LL * 365 * 4;

        std::string ts_types =
            "trailingEBITDA,annualEBITDA,quarterlyEBITDA,"
            "trailingTotalRevenue,annualTotalRevenue,"
            "trailingOperatingIncome,trailingNetIncome,annualNetIncome,"
            "trailingFreeCashFlow,trailingOperatingCashFlow,"
            "quarterlyTotalDebt,quarterlyCashCashEquivalentsAndShortTermInvestments,"
            "quarterlyCashAndCashEquivalents,quarterlyStockholdersEquity,"
            "quarterlyTotalAssets,quarterlyOrdinarySharesNumber";

        std::string ts_url = std::format(
            "https://query2.finance.yahoo.com/ws/fundamentals-timeseries/v1/finance/timeseries/{}?type={}&period1={}&period2={}",
            LiveMarketDataClient::url_encode(asset.symbol), ts_types, period1, period2
        );

        std::string ts_resp = client.http_get(ts_url);
        bool ts_ok = false;
        if (!ts_resp.empty()) {
            ts_ok = parse_timeseries_json_(ts_resp, raw);
        }

        // 2. Secondary Enrichment: Yahoo quoteSummary v10 (sector, industry, forwardPE, dividendYield, fallback mcap)
        std::string qs_url = std::format(
            "https://query2.finance.yahoo.com/v10/finance/quoteSummary/{}?modules=summaryDetail,defaultKeyStatistics,financialData,assetProfile,quoteType",
            LiveMarketDataClient::url_encode(asset.symbol)
        );
        std::string qs_resp = client.http_get_yahoo_crumb(qs_url);
        if (!qs_resp.empty()) {
            parse_quote_summary_json_(qs_resp, raw, ts_ok);
        }

        // Re-check bank/financial classification after industry/sector enrichment
        if (is_bank_or_financial_(raw.symbol, raw.sector, raw.industry, raw)) {
            raw.asset_class = AssetClassType::BankOrFinancial;
        }

        // 3. Live FX Cross-Rate Alignment (critical for USD/EUR-reporting BIST companies like THYAO, ENKAI, TAVHL)
        raw.fx_fin_to_trade = client.get_fx_rate(raw.financial_currency, raw.trading_currency);

        FundamentalProfile prof = compute_from_raw(raw);
        if (prof.is_available) {
            cache[key] = prof;
        }
        return prof;
    }

    static AssetClassType classify_asset_type(const RealTimeAssetData& asset) {
        if (asset.is_synthetic_metal || asset.instrument_type == "FUTURE" ||
            asset.symbol.ends_with("=F") || asset.symbol == "GRAM-ALTIN.TL" || asset.symbol == "GRAM-GUMUS.TL") {
            return AssetClassType::CommodityOrFuture;
        }
        if (asset.instrument_type == "ETF" || asset.instrument_type == "MUTUALFUND") {
            return AssetClassType::ETFOrFund;
        }
        if (asset.instrument_type == "CRYPTOCURRENCY" || asset.instrument_type == "CURRENCY" ||
            asset.symbol.ends_with("-USD") || asset.symbol.ends_with("=X")) {
            return AssetClassType::CryptoOrFX;
        }
        if (asset.instrument_type == "INDEX" || asset.symbol.starts_with("^") || asset.symbol == "XU100.IS") {
            return AssetClassType::IndexOrOther;
        }
        if (known_financial_tickers_().contains(asset.symbol)) {
            return AssetClassType::BankOrFinancial;
        }
        return AssetClassType::Equity;
    }

    // Pure deterministic computation of currency-aligned multiples & fundamental quality score
    static FundamentalProfile compute_from_raw(const RawFundamentalInputs& raw) {
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
        FundamentalProfile p;
        p.symbol = raw.symbol;
        p.company_name = raw.company_name.empty() ? raw.symbol : raw.company_name;
        p.sector = raw.sector;
        p.industry = raw.industry;
        p.trading_currency = raw.trading_currency.empty() ? "USD" : raw.trading_currency;
        p.trading_currency_display = LiveMarketDataClient::currency_display_for(p.trading_currency, p.symbol);
        p.financial_currency = raw.financial_currency.empty() ? p.trading_currency : raw.financial_currency;
        p.latest_report_date = raw.latest_report_date;
        p.asset_class = raw.asset_class;
        p.fx_fin_to_trade = (raw.fx_fin_to_trade > 0.0) ? raw.fx_fin_to_trade : 1.0;

        if (raw.shares_outstanding > 0.0 && raw.spot_price > 0.0) {
            p.market_cap_trade = raw.spot_price * raw.shares_outstanding;
        } else if (raw.fallback_market_cap_trade > 0.0) {
            p.market_cap_trade = raw.fallback_market_cap_trade;
        }
        p.market_cap_fin = (p.fx_fin_to_trade > 0.0) ? (p.market_cap_trade / p.fx_fin_to_trade) : p.market_cap_trade;

        bool any_statement = (raw.has_ebitda || raw.has_revenue || raw.has_net_income || raw.has_equity);
        if (!any_statement || p.market_cap_fin <= 0.0) {
            p.is_available = false;
            p.quality_grade = "N/A";
            p.summary_line = I18n::tr(
                "Kurumsal bilanço/FAVÖK verisine ulaşılamadı.",
                "Corporate balance sheet / EBITDA data unavailable.",
                "Unternehmensbilanz / EBITDA-Daten nicht verfügbar."
            );
            return p;
        }

        p.is_available = true;
        p.has_ebitda = (raw.has_ebitda && p.asset_class == AssetClassType::Equity);
        p.ebitda_ttm_fin = raw.ebitda_ttm;
        p.ebitda_ttm_trade = raw.ebitda_ttm * p.fx_fin_to_trade;
        p.revenue_ttm_fin = raw.revenue_ttm;
        p.net_income_ttm_fin = raw.net_income_ttm;
        p.free_cash_flow_ttm_fin = raw.free_cash_flow_ttm;
        p.stockholders_equity_fin = raw.stockholders_equity;

        double debt = raw.has_debt ? raw.total_debt : 0.0;
        double cash = raw.has_cash ? raw.cash_and_st_investments : 0.0;
        p.net_debt_fin = debt - cash;
        p.net_debt_trade = p.net_debt_fin * p.fx_fin_to_trade;

        p.enterprise_value_fin = p.market_cap_fin + p.net_debt_fin;
        p.enterprise_value_trade = p.enterprise_value_fin * p.fx_fin_to_trade;

        // Multiples & Margins (strictly in uniform financial_currency)
        if (p.has_ebitda && p.ebitda_ttm_fin > 0.0) {
            if (p.enterprise_value_fin > 0.0) {
                p.ev_to_ebitda = p.enterprise_value_fin / p.ebitda_ttm_fin;
            }
            if (raw.has_debt || raw.has_cash) {
                p.net_debt_to_ebitda = p.net_debt_fin / p.ebitda_ttm_fin;
            }
        }

        if (raw.has_revenue && raw.revenue_ttm > 0.0) {
            if (p.has_ebitda) {
                p.ebitda_margin_pct = (p.ebitda_ttm_fin / raw.revenue_ttm) * 100.0;
            }
            if (raw.has_operating_income) {
                p.operating_margin_pct = (raw.operating_income_ttm / raw.revenue_ttm) * 100.0;
            }
            if (raw.has_net_income) {
                p.net_margin_pct = (raw.net_income_ttm / raw.revenue_ttm) * 100.0;
            }
        }

        if (raw.has_ebitda_yoy) p.ebitda_yoy_growth_pct = raw.ebitda_yoy_growth_pct;
        if (raw.has_revenue_yoy) p.revenue_yoy_growth_pct = raw.revenue_yoy_growth_pct;
        if (raw.has_net_income_yoy) p.net_income_yoy_growth_pct = raw.net_income_yoy_growth_pct;

        if (raw.has_net_income && raw.net_income_ttm > 0.0 && p.market_cap_fin > 0.0) {
            p.pe_ratio = p.market_cap_fin / raw.net_income_ttm;
        }
        if (std::isfinite(raw.forward_pe) && raw.forward_pe > 0.5 && raw.forward_pe < 150.0) {
            // Guard against vendor currency-mismatch on forwardPE
            if (!std::isfinite(p.pe_ratio) || (raw.forward_pe > 0.15 * p.pe_ratio && raw.forward_pe < 6.0 * p.pe_ratio)) {
                p.forward_pe = raw.forward_pe;
            }
        }

        if (raw.has_equity && raw.stockholders_equity > 0.0 && p.market_cap_fin > 0.0) {
            p.pb_ratio = p.market_cap_fin / raw.stockholders_equity;
            if (raw.has_net_income) {
                p.roe_pct = (raw.net_income_ttm / raw.stockholders_equity) * 100.0;
            }
        }
        if (raw.has_assets && raw.total_assets > 0.0 && raw.has_net_income) {
            p.roa_pct = (raw.net_income_ttm / raw.total_assets) * 100.0;
        }
        if (raw.has_fcf && p.market_cap_fin > 0.0) {
            p.fcf_yield_pct = (raw.free_cash_flow_ttm / p.market_cap_fin) * 100.0;
        }
        if (std::isfinite(raw.dividend_yield_pct) && raw.dividend_yield_pct >= 0.0 && raw.dividend_yield_pct < 35.0) {
            p.dividend_yield_pct = raw.dividend_yield_pct;
        }
        if (std::isfinite(raw.analyst_target_mean) && raw.spot_price > 0.0 &&
            raw.analyst_target_mean > 0.25 * raw.spot_price && raw.analyst_target_mean < 4.0 * raw.spot_price) {
            p.analyst_target_mean = raw.analyst_target_mean;
        } else {
            p.analyst_target_mean = NaN;
        }

        // Accounting & Currency Transparency Notes (especially crucial for BIST!)
        if (p.financial_currency != p.trading_currency) {
            p.accounting_note = I18n::tr(
                std::format("Fonksiyonel bilanço para birimi {} (Kur: 1 {} = {:.2f} {}). Tüm çarpanlar (FD/FAVÖK, F/K, PD/DD) kur uyumlu hesaplanmıştır.",
                            p.financial_currency, p.financial_currency, p.fx_fin_to_trade, p.trading_currency_display),
                std::format("Functional reporting currency is {} (FX: 1 {} = {:.2f} {}). All multiples (EV/EBITDA, P/E, P/B) are FX-aligned.",
                            p.financial_currency, p.financial_currency, p.fx_fin_to_trade, p.trading_currency_display),
                std::format("Funktionale Bilanzwährung ist {} (Kurs: 1 {} = {:.2f} {}). Alle Multiplikatoren sind währungsbereinigt.",
                            p.financial_currency, p.financial_currency, p.fx_fin_to_trade, p.trading_currency_display)
            );
        } else if (p.trading_currency == "TRY" && p.symbol.ends_with(".IS")) {
            p.accounting_note = I18n::tr(
                "BIST TMS-29 enflasyon muhasebesi kapsamında kümülatif dönem kaymasını önlemek için kayan 12-aylık (TTM) veriler baz alınmıştır.",
                "Rolling 12-month (TTM) figures are used to eliminate YTD cumulative distortion under BIST IAS-29 inflation accounting.",
                "Rollierende 12-Monats-Zahlen (TTM) zur Bereinigung kumulativer Effekte unter IAS-29-Inflationsbilanzierung."
            );
        }

        score_and_summarize_(p, raw);
        return p;
    }

    static FundamentalProfile build_non_equity_profile(const RealTimeAssetData& asset, AssetClassType cls) {
        FundamentalProfile p;
        p.symbol = asset.symbol;
        p.company_name = asset.long_name.empty() ? asset.symbol : asset.long_name;
        p.trading_currency = asset.currency_code;
        p.trading_currency_display = asset.currency_display;
        p.financial_currency = asset.currency_code;
        p.asset_class = cls;
        p.is_available = false;
        p.has_ebitda = false;
        p.fundamental_score = 0.0;
        p.quality_grade = "N/A";

        if (cls == AssetClassType::CommodityOrFuture) {
            p.summary_line = I18n::tr(
                "Emtia / Kıymetli Maden (Kurumsal bilanço ve FAVÖK uygulanmaz; reel faiz ve kur baz alınır)",
                "Commodity / Precious Metal (Corporate EBITDA N/A; driven by real yields & FX)",
                "Rohstoff / Edelmetall (Kein Unternehmens-EBITDA; getrieben durch Realzins & Währung)"
            );
        } else if (cls == AssetClassType::ETFOrFund) {
            p.summary_line = I18n::tr(
                "Borsa Yatırım Fonu / Endeks Sepeti (Tekil şirket FAVÖK/Net Borç tablosu uygulanmaz)",
                "Exchange-Traded Fund / Index Basket (Single-company corporate EBITDA/Debt N/A)",
                "ETF / Indexkorb (Einzelunternehmens-EBITDA/Nettoverschuldung nicht anwendbar)"
            );
        } else {
            p.summary_line = I18n::tr(
                "Makro / Döviz / Kripto Varlık (Kurumsal bilanço ve FAVÖK uygulanmaz)",
                "Macro / FX / Crypto Asset (Corporate balance sheet & EBITDA N/A)",
                "Makro- / Devisen- / Krypto-Asset (Keine Unternehmensbilanz / EBITDA)"
            );
        }
        return p;
    }

    static std::string format_compact_money(double val, const std::string& ccy) {
        if (!std::isfinite(val)) return "N/A";
        std::string sign = (val < 0.0) ? "-" : "";
        double a = std::abs(val);
        std::string ccy_disp = (ccy == "TRY") ? "TL" : ccy;
        if (a >= 1e12) {
            return std::format("{}{:.2f} {} {}", sign, a / 1e12, I18n::tr("Trl", "T", "Bio"), ccy_disp);
        }
        if (a >= 1e9) {
            return std::format("{}{:.2f} {} {}", sign, a / 1e9, I18n::tr("Mrd", "B", "Mrd"), ccy_disp);
        }
        if (a >= 1e6) {
            return std::format("{}{:.1f} {} {}", sign, a / 1e6, I18n::tr("Mln", "M", "Mio"), ccy_disp);
        }
        return std::format("{}{:.0f} {}", sign, a, ccy_disp);
    }

    // Renders the dedicated FAVÖK & Fundamental Analysis Box (or Commodity/ETF Macro Context Box)
    static void render_fundamental_card(const FundamentalProfile& prof,
                                        const RealTimeAssetData& asset,
                                        const TreasuryYieldCurveData* yc = nullptr) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        if (prof.asset_class == AssetClassType::Equity && prof.is_available) {
            std::cout << "\n" << pal.glow_rgb;
            std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
            std::string hdr = I18n::tr(
                std::format("║  📊 {} TEMEL ANALİZ & FAVÖK (EBITDA) BİLANÇO EK BİLGİ KARTI", prof.symbol),
                std::format("║  📊 {} FUNDAMENTAL ANALYSIS & EBITDA VALUATION CARD", prof.symbol),
                std::format("║  📊 {} FUNDAMENTALANALYSE & EBITDA-BEWERTUNGSKARTE", prof.symbol)
            );
            int pad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));
            std::cout << hdr << std::string(pad, ' ') << "║\n";
            std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
            std::cout << pal.reset_code;

            std::string sec_info = prof.sector.empty()
                ? I18n::tr("Kurumsal Hisse Senedi", "Corporate Equity", "Unternehmensaktie")
                : (prof.industry.empty() ? prof.sector : (prof.sector + " / " + prof.industry));
            std::string rep_info = prof.latest_report_date.empty() ? "TTM" : ("TTM @ " + prof.latest_report_date);

            std::cout << std::format("   {:<24}: {} ({})\n",
                                     I18n::tr("Sektör & Bilanço Dönemi", "Sector & Report Period", "Sektor & Berichtsstand"),
                                     sec_info, rep_info);
            std::cout << std::format("   {:<24}: {}{}  │  {}: {}\n",
                                     I18n::tr("Piyasa Değeri (MCap)", "Market Cap", "Marktkapitalisierung"),
                                     pal.glow_rgb, format_compact_money(prof.market_cap_trade, prof.trading_currency),
                                     I18n::tr("Firma Değeri (FD/EV)", "Enterprise Value (EV)", "Unternehmenswert (EV)"),
                                     format_compact_money(prof.enterprise_value_trade, prof.trading_currency) + pal.reset_code);

            // FAVÖK (EBITDA) Line with dual-currency clarity when reporting currency != trading currency
            if (prof.has_ebitda) {
                std::string ebitda_str = format_compact_money(prof.ebitda_ttm_fin, prof.financial_currency);
                if (prof.financial_currency != prof.trading_currency) {
                    ebitda_str += std::format(" (≈ {})", format_compact_money(prof.ebitda_ttm_trade, prof.trading_currency));
                }
                std::string margin_str = std::isfinite(prof.ebitda_margin_pct)
                    ? std::format("%{:.1f}", prof.ebitda_margin_pct) : "N/A";
                std::string yoy_str = std::isfinite(prof.ebitda_yoy_growth_pct)
                    ? std::format("{:+.1f}% YoY", prof.ebitda_yoy_growth_pct) : "";

                std::cout << std::format("   {:<24}: {}{}{}  ({}: {}{}{})\n",
                                         I18n::tr("FAVÖK (TTM EBITDA)", "EBITDA (TTM)", "EBITDA (TTM)"),
                                         pal.glow_rgb, ebitda_str, pal.reset_code,
                                         I18n::tr("FAVÖK Marjı", "EBITDA Margin", "EBITDA-Marge"),
                                         margin_str,
                                         yoy_str.empty() ? "" : " │ ", yoy_str);

                std::string ev_ebitda_str = std::isfinite(prof.ev_to_ebitda)
                    ? std::format("{:.2f}x", prof.ev_to_ebitda)
                    : I18n::tr("Negatif/Yok", "Negative/N/A", "Negativ/k.A.");

                std::string nd_ebitda_str = "N/A";
                if (std::isfinite(prof.net_debt_to_ebitda)) {
                    if (prof.net_debt_to_ebitda < 0.0) {
                        nd_ebitda_str = std::format("{}{:.2f}x [NET NAKİT]{}", pal.green_rgb, prof.net_debt_to_ebitda, pal.reset_code);
                    } else {
                        std::string col = (prof.net_debt_to_ebitda <= 2.5) ? pal.green_rgb :
                                          ((prof.net_debt_to_ebitda <= 4.0) ? pal.base_rgb : pal.red_rgb);
                        nd_ebitda_str = std::format("{}{:.2f}x{}", col, prof.net_debt_to_ebitda, pal.reset_code);
                    }
                }

                std::cout << std::format("   {:<24}: {}{}{}  │  {}: {}\n",
                                         I18n::tr("FD / FAVÖK (EV/EBITDA)", "EV / EBITDA", "EV / EBITDA"),
                                         pal.glow_rgb, ev_ebitda_str, pal.reset_code,
                                         I18n::tr("Net Borç / FAVÖK", "Net Debt / EBITDA", "Nettoschuld / EBITDA"),
                                         nd_ebitda_str);
            }

            // Valuation Multiples: P/E, Forward P/E, P/B, Dividend Yield
            std::string pe_str = std::isfinite(prof.pe_ratio) ? std::format("{:.2f}x", prof.pe_ratio) : I18n::tr("Zarar/Yok", "Loss/N/A", "Verlust/k.A.");
            if (std::isfinite(prof.forward_pe)) {
                pe_str += std::format(" ({}: {:.2f}x)", I18n::tr("İleri F/K", "Fwd P/E", "Fwd KGV"), prof.forward_pe);
            }
            std::string pb_str = std::isfinite(prof.pb_ratio) ? std::format("{:.2f}x", prof.pb_ratio) : "N/A";
            std::string div_str = (std::isfinite(prof.dividend_yield_pct) && prof.dividend_yield_pct > 0.01)
                ? std::format("  │  {}: %{:.2f}", I18n::tr("Temettü", "Div. Yield", "Div.-Rend."), prof.dividend_yield_pct)
                : "";

            std::cout << std::format("   {:<24}: {}  │  {}: {}{}\n",
                                     I18n::tr("F/K (P/E Çarpanı)", "P/E Multiple", "KGV (P/E)"),
                                     pe_str,
                                     I18n::tr("PD / DD (P/B)", "P/B Multiple", "KBV (P/B)"),
                                     pb_str, div_str);

            // Profitability & Cash Flow: ROE, Net Margin, FCF
            std::string roe_str = std::isfinite(prof.roe_pct) ? std::format("%{:.1f}", prof.roe_pct) : "N/A";
            std::string nm_str = std::isfinite(prof.net_margin_pct) ? std::format("%{:.1f}", prof.net_margin_pct) : "N/A";
            std::string fcf_str = std::isfinite(prof.fcf_yield_pct)
                ? std::format("{} ({}: {:+.1f}%)",
                              format_compact_money(prof.free_cash_flow_ttm_fin, prof.financial_currency),
                              I18n::tr("FCF Verimi", "FCF Yield", "FCF-Rendite"), prof.fcf_yield_pct)
                : "N/A";

            std::cout << std::format("   {:<24}: {}  │  {}: {}  │  FCF: {}\n",
                                     I18n::tr("Özkaynak Kârlılığı (ROE)", "Return on Equity (ROE)", "Eigenkapitalrendite"),
                                     roe_str,
                                     I18n::tr("Net Marj", "Net Margin", "Nettomarge"),
                                     nm_str, fcf_str);

            std::string grade_col = (prof.fundamental_score >= 0.25) ? pal.green_rgb :
                                    ((prof.fundamental_score <= -0.20) ? pal.red_rgb : pal.glow_rgb);
            std::cout << std::format("   {:<24}: {}{} ({:+.2f} / 1.00){} — {}\n",
                                     I18n::tr("Temel Kalite & Değerleme", "Fundamental Grade", "Fundamental-Note"),
                                     grade_col, prof.quality_grade, prof.fundamental_score, pal.reset_code,
                                     prof.diagnosis);

            if (!prof.accounting_note.empty()) {
                std::cout << pal.dim_rgb << "   ℹ " << prof.accounting_note << pal.reset_code << "\n";
            }

            std::cout << pal.glow_rgb;
            std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
            return;
        }

        if (prof.asset_class == AssetClassType::BankOrFinancial && prof.is_available) {
            std::cout << "\n" << pal.glow_rgb;
            std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
            std::string hdr = I18n::tr(
                std::format("║  🏦 {} BANKA / FİNANS KURUMU TEMEL BİLANÇO EK BİLGİ KARTI", prof.symbol),
                std::format("║  🏦 {} BANK / FINANCIAL INSTITUTION FUNDAMENTAL CARD", prof.symbol),
                std::format("║  🏦 {} BANK- / FINANZINSTITUT-FUNDAMENTALKARTE", prof.symbol)
            );
            int pad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));
            std::cout << hdr << std::string(pad, ' ') << "║\n";
            std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
            std::cout << pal.reset_code;

            std::cout << pal.dim_rgb
                      << I18n::tr("   ℹ Banka ve sigorta bilançolarında FAVÖK (EBITDA) ve FD/FAVÖK yapısal olarak uygulanmaz;\n"
                                  "     değerlemede Özkaynak Kârlılığı (ROE), PD/DD ve F/K çarpanları esas alınır.\n",
                                  "   ℹ EBITDA & EV/EBITDA do not apply structurally to banks/insurers;\n"
                                  "     valuation is governed by Return on Equity (ROE), P/B, and P/E multiples.\n",
                                  "   ℹ EBITDA & EV/EBITDA sind bei Banken/Versicherern strukturell nicht anwendbar;\n"
                                  "     maßgeblich sind Eigenkapitalrendite (ROE), KBV und KGV.\n")
                      << pal.reset_code;

            std::cout << std::format("   {:<24}: {}  │  {}: {}\n",
                                     I18n::tr("Piyasa Değeri", "Market Cap", "Marktkapitalisierung"),
                                     format_compact_money(prof.market_cap_trade, prof.trading_currency),
                                     I18n::tr("Özkaynaklar", "Shareholders' Equity", "Eigenkapital"),
                                     format_compact_money(prof.stockholders_equity_fin, prof.financial_currency));
            std::cout << std::format("   {:<24}: {}  │  F/K: {}  │  PD/DD: {}\n",
                                     I18n::tr("Net Kâr (TTM)", "Net Income (TTM)", "Nettogewinn (TTM)"),
                                     format_compact_money(prof.net_income_ttm_fin, prof.financial_currency),
                                     std::isfinite(prof.pe_ratio) ? std::format("{:.2f}x", prof.pe_ratio) : "N/A",
                                     std::isfinite(prof.pb_ratio) ? std::format("{:.2f}x", prof.pb_ratio) : "N/A");
            std::cout << std::format("   {:<24}: {}  │  ROA: {}  │  {}: {}{} ({:+.2f}){}\n",
                                     "ROE",
                                     std::isfinite(prof.roe_pct) ? std::format("%{:.1f}", prof.roe_pct) : "N/A",
                                     std::isfinite(prof.roa_pct) ? std::format("%{:.2f}", prof.roa_pct) : "N/A",
                                     I18n::tr("Temel Not", "Grade", "Note"),
                                     pal.glow_rgb, prof.quality_grade, prof.fundamental_score, pal.reset_code);
            std::cout << "   • " << prof.diagnosis << "\n";
            std::cout << pal.glow_rgb;
            std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
            return;
        }

        // Commodity / Precious Metal / ETF / Crypto Structural Context Card
        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::string hdr = I18n::tr(
            std::format("║  🥇 {} VARLIK SINIFI & MAKRO YAPI EK BİLGİ ÖZETİ", asset.symbol),
            std::format("║  🥇 {} ASSET CLASS & MACRO STRUCTURE CONTEXT CARD", asset.symbol),
            std::format("║  🥇 {} ANLAGEKLASSE & MAKRO-STRUKTURKARTE", asset.symbol)
        );
        int pad = std::max(0, 79 - ThemeEngine::utf8_visible_length(hdr));
        std::cout << hdr << std::string(pad, ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        std::cout << "   • " << prof.summary_line << "\n";
        if (asset.is_synthetic_metal && asset.underlying_ounce_usd > 0.0 && asset.underlying_usdtry > 0.0) {
            std::cout << std::format("   • {:<22}: ${:.2f} / Ons  ×  USD/TRY {:.4f}  ÷  31.1035 g\n",
                                     I18n::tr("Canlı Sentez Bileşeni", "Live Synthesis Inputs", "Live-Synthese-Faktoren"),
                                     asset.underlying_ounce_usd, asset.underlying_usdtry);
        }
        if (asset.fifty_two_week_high > asset.fifty_two_week_low && asset.fifty_two_week_low > 0.0) {
            double pos_pct = ((asset.current_price - asset.fifty_two_week_low) /
                              (asset.fifty_two_week_high - asset.fifty_two_week_low)) * 100.0;
            pos_pct = std::clamp(pos_pct, 0.0, 100.0);
            std::cout << std::format("   • {:<22}: {:.2f} {} — {:.2f} {} ({}: %{:.1f})\n",
                                     I18n::tr("52-Haftalık Bant", "52-Week Range", "52-Wochen-Spanne"),
                                     asset.fifty_two_week_low, asset.currency_display,
                                     asset.fifty_two_week_high, asset.currency_display,
                                     I18n::tr("Bant Konumu", "Range Pos", "Position"), pos_pct);
        }
        std::cout << std::format("   • {:<22}: {:.2f} {} (%{:.2f} {})\n",
                                 I18n::tr("14-Günlük Gerçek ATR", "14-Day Wilder ATR", "14-Tage Wilder ATR"),
                                 asset.atr_14, asset.currency_display,
                                 (asset.current_price > 0.0 ? (asset.atr_14 / asset.current_price * 100.0) : 0.0),
                                 I18n::tr("günlük ortalama bar", "avg daily range", "Tagesspanne"));
        if (yc && yc->is_live) {
            std::cout << std::format("   • {:<22}: 3M T-Bill %{:.2f}  │  10Y Treasury %{:.2f}\n",
                                     I18n::tr("Küresel Faiz Çıpası", "Global Yield Anchor", "Globaler Zinsanker"),
                                     yc->three_month_yield * 100.0, yc->ten_year_yield * 100.0);
        }
        std::cout << pal.glow_rgb;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n" << pal.reset_code;
    }

private:
    static const std::unordered_set<std::string>& known_financial_tickers_() {
        static const std::unordered_set<std::string> s = {
            "GARAN.IS", "AKBNK.IS", "ISCTR.IS", "YKBNK.IS", "HALKB.IS", "VAKBN.IS",
            "TSKB.IS", "SKBNK.IS", "ALBRK.IS", "ANSGR.IS", "TURSG.IS", "AGESA.IS",
            "ALV.DE", "DBK.DE", "CBK.DE", "MUV2.DE", "HNR1.DE",
            "JPM", "BAC", "WFC", "C", "GS", "MS", "BLK", "SCHW"
        };
        return s;
    }

    static bool is_bank_or_financial_(const std::string& sym,
                                      const std::string& sector,
                                      const std::string& industry,
                                      const RawFundamentalInputs& raw) {
        if (known_financial_tickers_().contains(sym)) return true;
        std::string ind_l = industry;
        std::transform(ind_l.begin(), ind_l.end(), ind_l.begin(), ::tolower);
        std::string sec_l = sector;
        std::transform(sec_l.begin(), sec_l.end(), sec_l.begin(), ::tolower);

        if (ind_l.find("bank") != std::string::npos || ind_l.find("insurance") != std::string::npos ||
            ind_l.find("credit") != std::string::npos || sec_l.find("financial") != std::string::npos) {
            if (!raw.has_ebitda) return true;
        }
        if (!raw.has_ebitda && raw.has_assets && raw.has_equity &&
            raw.stockholders_equity > 0.0 && (raw.total_assets / raw.stockholders_equity) > 6.5) {
            return true;
        }
        return false;
    }

    struct TsPoint {
        std::string date;
        std::string ccy;
        double val{0.0};
    };

    static bool parse_timeseries_json_(const std::string& json_str, RawFundamentalInputs& raw) {
        boost::system::error_code ec;
        boost::json::value root = boost::json::parse(json_str, ec);
        if (ec || !root.is_object()) return false;

        auto ts_it = root.as_object().find("timeseries");
        if (ts_it == root.as_object().end() || !ts_it->value().is_object()) return false;
        auto res_it = ts_it->value().as_object().find("result");
        if (res_it == ts_it->value().as_object().end() || !res_it->value().is_array()) return false;

        std::unordered_map<std::string, std::vector<TsPoint>> series;

        for (const auto& item : res_it->value().as_array()) {
            if (!item.is_object()) continue;
            const auto& obj = item.as_object();
            std::string type_name;
            if (auto m_it = obj.find("meta"); m_it != obj.end() && m_it->value().is_object()) {
                if (auto t_it = m_it->value().as_object().find("type");
                    t_it != m_it->value().as_object().end() && t_it->value().is_array() && !t_it->value().as_array().empty()) {
                    if (t_it->value().as_array()[0].is_string()) {
                        type_name = std::string(t_it->value().as_array()[0].as_string());
                    }
                }
            }
            if (type_name.empty()) continue;

            auto arr_it = obj.find(type_name);
            if (arr_it == obj.end() || !arr_it->value().is_array()) continue;

            std::vector<TsPoint> pts;
            for (const auto& el : arr_it->value().as_array()) {
                if (!el.is_object()) continue;
                const auto& eo = el.as_object();
                auto rv_it = eo.find("reportedValue");
                if (rv_it == eo.end() || !rv_it->value().is_object()) continue;
                auto r_it = rv_it->value().as_object().find("raw");
                if (r_it == rv_it->value().as_object().end() || r_it->value().is_null()) continue;

                double val = 0.0;
                if (r_it->value().is_double()) val = r_it->value().as_double();
                else if (r_it->value().is_int64()) val = static_cast<double>(r_it->value().as_int64());
                else if (r_it->value().is_uint64()) val = static_cast<double>(r_it->value().as_uint64());
                else continue;

                std::string dt;
                if (auto d_it = eo.find("asOfDate"); d_it != eo.end() && d_it->value().is_string()) {
                    dt = std::string(d_it->value().as_string());
                }
                std::string ccy;
                if (auto c_it = eo.find("currencyCode"); c_it != eo.end() && c_it->value().is_string()) {
                    ccy = std::string(c_it->value().as_string());
                }
                pts.push_back({std::move(dt), std::move(ccy), val});
            }
            if (!pts.empty()) {
                series[type_name] = std::move(pts);
            }
        }

        if (series.empty()) return false;

        auto latest = [&](const std::string& key, double& out_val, std::string* out_ccy = nullptr, std::string* out_dt = nullptr) -> bool {
            auto it = series.find(key);
            if (it == series.end() || it->second.empty()) return false;
            const auto& bk = it->second.back();
            out_val = bk.val;
            if (out_ccy && !bk.ccy.empty()) *out_ccy = bk.ccy;
            if (out_dt && !bk.date.empty()) *out_dt = bk.date;
            return true;
        };

        auto calc_yoy = [&](const std::string& trail_key, const std::string& ann_key, const std::string& qtr_key, double& out_pct) -> bool {
            if (auto it = series.find(trail_key); it != series.end() && it->second.size() >= 2) {
                double prev = it->second[it->second.size() - 2].val;
                double curr = it->second.back().val;
                if (prev > 0.0) {
                    out_pct = ((curr - prev) / prev) * 100.0;
                    return true;
                }
            }
            if (auto it = series.find(ann_key); it != series.end() && it->second.size() >= 2) {
                double prev = it->second[it->second.size() - 2].val;
                double curr = it->second.back().val;
                if (prev > 0.0) {
                    out_pct = ((curr - prev) / prev) * 100.0;
                    return true;
                }
            }
            if (auto it = series.find(qtr_key); it != series.end() && it->second.size() >= 5) {
                double prev = it->second[it->second.size() - 5].val;
                double curr = it->second.back().val;
                if (prev > 0.0) {
                    out_pct = ((curr - prev) / prev) * 100.0;
                    return true;
                }
            }
            return false;
        };

        std::string detected_ccy;
        std::string detected_dt;

        // EBITDA (TTM preferred -> 4-quarter sum -> Annual)
        if (latest("trailingEBITDA", raw.ebitda_ttm, &detected_ccy, &detected_dt)) {
            raw.has_ebitda = true;
        } else if (auto q_it = series.find("quarterlyEBITDA"); q_it != series.end() && q_it->second.size() >= 4) {
            double sum4 = 0.0;
            const auto& v = q_it->second;
            for (size_t i = v.size() - 4; i < v.size(); ++i) sum4 += v[i].val;
            raw.ebitda_ttm = sum4;
            raw.has_ebitda = true;
            if (!v.back().ccy.empty()) detected_ccy = v.back().ccy;
            if (!v.back().date.empty()) detected_dt = v.back().date;
        } else if (latest("annualEBITDA", raw.ebitda_ttm, &detected_ccy, &detected_dt)) {
            raw.has_ebitda = true;
        }

        raw.has_ebitda_yoy = calc_yoy("trailingEBITDA", "annualEBITDA", "quarterlyEBITDA", raw.ebitda_yoy_growth_pct);

        if (latest("trailingTotalRevenue", raw.revenue_ttm, &detected_ccy, &detected_dt) ||
            latest("annualTotalRevenue", raw.revenue_ttm, &detected_ccy, &detected_dt)) {
            raw.has_revenue = true;
        }
        raw.has_revenue_yoy = calc_yoy("trailingTotalRevenue", "annualTotalRevenue", "", raw.revenue_yoy_growth_pct);

        if (latest("trailingOperatingIncome", raw.operating_income_ttm, &detected_ccy)) {
            raw.has_operating_income = true;
        }

        if (latest("trailingNetIncome", raw.net_income_ttm, &detected_ccy, &detected_dt) ||
            latest("annualNetIncome", raw.net_income_ttm, &detected_ccy, &detected_dt)) {
            raw.has_net_income = true;
        }
        raw.has_net_income_yoy = calc_yoy("trailingNetIncome", "annualNetIncome", "", raw.net_income_yoy_growth_pct);

        if (latest("trailingFreeCashFlow", raw.free_cash_flow_ttm, &detected_ccy)) {
            raw.has_fcf = true;
        }
        if (latest("trailingOperatingCashFlow", raw.operating_cash_flow_ttm, &detected_ccy)) {
            raw.has_ocf = true;
        }

        if (latest("quarterlyTotalDebt", raw.total_debt, &detected_ccy)) {
            raw.has_debt = true;
        }
        double cash_st = 0.0, cash_only = 0.0;
        bool has_cst = latest("quarterlyCashCashEquivalentsAndShortTermInvestments", cash_st, &detected_ccy);
        bool has_co  = latest("quarterlyCashAndCashEquivalents", cash_only, &detected_ccy);
        if (has_cst || has_co) {
            raw.has_cash = true;
            raw.cash_and_st_investments = std::max(cash_st, cash_only);
        }

        if (latest("quarterlyStockholdersEquity", raw.stockholders_equity, &detected_ccy, &detected_dt)) {
            raw.has_equity = true;
        }
        if (latest("quarterlyTotalAssets", raw.total_assets, &detected_ccy)) {
            raw.has_assets = true;
        }

        double sh = 0.0;
        if (latest("quarterlyOrdinarySharesNumber", sh) && sh > 0.0) {
            raw.shares_outstanding = sh;
        }

        if (!detected_ccy.empty()) raw.financial_currency = detected_ccy;
        if (!detected_dt.empty()) raw.latest_report_date = detected_dt;
        return true;
    }

    static void parse_quote_summary_json_(const std::string& json_str, RawFundamentalInputs& raw, bool ts_already_loaded) {
        boost::system::error_code ec;
        boost::json::value root = boost::json::parse(json_str, ec);
        if (ec || !root.is_object()) return;

        auto qs_it = root.as_object().find("quoteSummary");
        if (qs_it == root.as_object().end() || !qs_it->value().is_object()) return;
        auto res_it = qs_it->value().as_object().find("result");
        if (res_it == qs_it->value().as_object().end() || !res_it->value().is_array() || res_it->value().as_array().empty()) return;
        if (!res_it->value().as_array()[0].is_object()) return;
        const auto& r0 = res_it->value().as_array()[0].as_object();

        auto raw_num = [](const boost::json::object& obj, std::string_view key, double& out) -> bool {
            auto it = obj.find(key);
            if (it == obj.end() || !it->value().is_object()) return false;
            auto rit = it->value().as_object().find("raw");
            if (rit == it->value().as_object().end() || rit->value().is_null()) return false;
            if (rit->value().is_double()) { out = rit->value().as_double(); return true; }
            if (rit->value().is_int64()) { out = static_cast<double>(rit->value().as_int64()); return true; }
            if (rit->value().is_uint64()) { out = static_cast<double>(rit->value().as_uint64()); return true; }
            return false;
        };

        if (auto ap_it = r0.find("assetProfile"); ap_it != r0.end() && ap_it->value().is_object()) {
            const auto& ap = ap_it->value().as_object();
            if (auto s = ap.find("sector"); s != ap.end() && s->value().is_string()) raw.sector = std::string(s->value().as_string());
            if (auto i = ap.find("industry"); i != ap.end() && i->value().is_string()) raw.industry = std::string(i->value().as_string());
        }

        if (auto qt_it = r0.find("quoteType"); qt_it != r0.end() && qt_it->value().is_object()) {
            const auto& qt = qt_it->value().as_object();
            if (auto ln = qt.find("longName"); ln != qt.end() && ln->value().is_string() && !ln->value().as_string().empty()) {
                raw.company_name = std::string(ln->value().as_string());
            } else if (auto sn = qt.find("shortName"); sn != qt.end() && sn->value().is_string() && !sn->value().as_string().empty()) {
                raw.company_name = std::string(sn->value().as_string());
            }
        }

        if (auto sd_it = r0.find("summaryDetail"); sd_it != r0.end() && sd_it->value().is_object()) {
            const auto& sd = sd_it->value().as_object();
            double mcap = 0.0;
            if (raw_num(sd, "marketCap", mcap) && mcap > 0.0) raw.fallback_market_cap_trade = mcap;
            double fpe = 0.0;
            if (raw_num(sd, "forwardPE", fpe) && fpe > 0.0) raw.forward_pe = fpe;
            double dy = 0.0;
            if (raw_num(sd, "dividendYield", dy) && dy >= 0.0) raw.dividend_yield_pct = dy * 100.0;
        }

        if (auto ks_it = r0.find("defaultKeyStatistics"); ks_it != r0.end() && ks_it->value().is_object()) {
            const auto& ks = ks_it->value().as_object();
            double sh = 0.0;
            if (raw.shares_outstanding <= 0.0 && raw_num(ks, "sharesOutstanding", sh) && sh > 0.0) {
                raw.shares_outstanding = sh;
            }
        }

        if (auto fd_it = r0.find("financialData"); fd_it != r0.end() && fd_it->value().is_object()) {
            const auto& fd = fd_it->value().as_object();
            double tgt = 0.0;
            if (raw_num(fd, "targetMeanPrice", tgt) && tgt > 0.0) raw.analyst_target_mean = tgt;
            if (auto rk = fd.find("recommendationKey"); rk != fd.end() && rk->value().is_string()) {
                raw.recommendation_key = std::string(rk->value().as_string());
            }

            // Fallback to financialData ONLY if fundamentals-timeseries returned nothing
            if (!ts_already_loaded) {
                if (auto fc = fd.find("financialCurrency"); fc != fd.end() && fc->value().is_string() && !fc->value().as_string().empty()) {
                    raw.financial_currency = std::string(fc->value().as_string());
                }
                if (!raw.has_ebitda && raw_num(fd, "ebitda", raw.ebitda_ttm)) raw.has_ebitda = true;
                if (!raw.has_revenue && raw_num(fd, "totalRevenue", raw.revenue_ttm)) raw.has_revenue = true;
                if (!raw.has_fcf && raw_num(fd, "freeCashflow", raw.free_cash_flow_ttm)) raw.has_fcf = true;
                if (!raw.has_debt && raw_num(fd, "totalDebt", raw.total_debt)) raw.has_debt = true;
                if (!raw.has_cash && raw_num(fd, "totalCash", raw.cash_and_st_investments)) raw.has_cash = true;
            }
        }
    }

    static void score_and_summarize_(FundamentalProfile& p, const RawFundamentalInputs& raw) {
        if (p.asset_class == AssetClassType::BankOrFinancial) {
            double val_s = 0.0;
            if (std::isfinite(p.pe_ratio)) {
                if (p.pe_ratio <= 5.5) val_s += 0.55;
                else if (p.pe_ratio <= 9.0) val_s += 0.25;
                else if (p.pe_ratio >= 16.0) val_s -= 0.35;
            }
            if (std::isfinite(p.pb_ratio) && std::isfinite(p.roe_pct)) {
                // Bank Justified P/B intuition: high ROE (>20%) at reasonable P/B (<1.6) is attractive
                if (p.roe_pct >= 20.0 && p.pb_ratio <= 1.6) val_s += 0.45;
                else if (p.roe_pct >= 12.0 && p.pb_ratio <= 1.3) val_s += 0.25;
                else if (p.roe_pct < 8.0 && p.pb_ratio > 1.5) val_s -= 0.40;
            }
            p.fundamental_score = std::clamp(val_s, -1.0, 1.0);
            assign_grade_(p);

            p.summary_line = std::format(
                "Banka/Finans (FAVÖK Yok) │ F/K: {} │ PD/DD: {} │ ROE: {} │ Not: {}",
                std::isfinite(p.pe_ratio) ? std::format("{:.1f}x", p.pe_ratio) : "N/A",
                std::isfinite(p.pb_ratio) ? std::format("{:.2f}x", p.pb_ratio) : "N/A",
                std::isfinite(p.roe_pct) ? std::format("%{:.1f}", p.roe_pct) : "N/A",
                p.quality_grade
            );
            p.diagnosis = I18n::tr(
                std::format("Özkaynak kârlılığı (ROE: {}) ve çarpan yapısı (PD/DD: {}, F/K: {}) banka değerleme dengesini belirliyor.",
                            std::isfinite(p.roe_pct) ? std::format("%{:.1f}", p.roe_pct) : "N/A",
                            std::isfinite(p.pb_ratio) ? std::format("{:.2f}x", p.pb_ratio) : "N/A",
                            std::isfinite(p.pe_ratio) ? std::format("{:.1f}x", p.pe_ratio) : "N/A"),
                std::format("Return on Equity (ROE: {}) and book valuation (P/B: {}, P/E: {}) anchor financial quality.",
                            std::isfinite(p.roe_pct) ? std::format("{:.1f}%", p.roe_pct) : "N/A",
                            std::isfinite(p.pb_ratio) ? std::format("{:.2f}x", p.pb_ratio) : "N/A",
                            std::isfinite(p.pe_ratio) ? std::format("{:.1f}x", p.pe_ratio) : "N/A"),
                std::format("Eigenkapitalrendite (ROE: {}) und Buchwert-Multiple (KBV: {}, KGV: {}) bestimmen die Bewertung.",
                            std::isfinite(p.roe_pct) ? std::format("{:.1f}%", p.roe_pct) : "N/A",
                            std::isfinite(p.pb_ratio) ? std::format("{:.2f}x", p.pb_ratio) : "N/A",
                            std::isfinite(p.pe_ratio) ? std::format("{:.1f}x", p.pe_ratio) : "N/A")
            );
            return;
        }

        // Corporate Equity 3-Pillar Scoring:
        // 1. Valuation (EV/FAVÖK adjusted for margin/growth + P/E + FCF Yield): 40%
        // 2. Profitability & FAVÖK Margin / Growth: 35%
        // 3. Balance Sheet Solvency (Net Debt / FAVÖK): 25%
        double val_pillar = 0.0;
        bool is_high_growth_margin = (std::isfinite(p.ebitda_margin_pct) && p.ebitda_margin_pct >= 24.0) ||
                                     (std::isfinite(p.ebitda_yoy_growth_pct) && p.ebitda_yoy_growth_pct >= 20.0) ||
                                     (std::isfinite(p.roe_pct) && p.roe_pct >= 25.0);

        if (p.has_ebitda && p.ebitda_ttm_fin <= 0.0) {
            val_pillar -= 0.75; // Negative EBITDA
        } else if (std::isfinite(p.ev_to_ebitda)) {
            double fair_ev_ebitda = is_high_growth_margin ? 18.0 : 10.5;
            if (p.trading_currency == "TRY") fair_ev_ebitda = is_high_growth_margin ? 12.5 : 8.0;
            val_pillar += std::clamp((fair_ev_ebitda - p.ev_to_ebitda) / fair_ev_ebitda, -0.85, 0.85) * 0.70;
        }
        if (std::isfinite(p.fcf_yield_pct)) {
            if (p.fcf_yield_pct >= 5.0) val_pillar += 0.25;
            else if (p.fcf_yield_pct < -3.0) val_pillar -= 0.15;
        }
        val_pillar = std::clamp(val_pillar, -1.0, 1.0);

        double prof_pillar = 0.0;
        if (std::isfinite(p.ebitda_margin_pct)) {
            if (p.ebitda_margin_pct >= 25.0) prof_pillar += 0.50;
            else if (p.ebitda_margin_pct >= 14.0) prof_pillar += 0.25;
            else if (p.ebitda_margin_pct < 5.0) prof_pillar -= 0.45;
        }
        if (std::isfinite(p.roe_pct)) {
            if (p.roe_pct >= 20.0) prof_pillar += 0.35;
            else if (p.roe_pct >= 10.0) prof_pillar += 0.15;
            else if (p.roe_pct < 0.0) prof_pillar -= 0.45;
        }
        if (std::isfinite(p.ebitda_yoy_growth_pct)) {
            if (p.ebitda_yoy_growth_pct >= 15.0) prof_pillar += 0.25;
            else if (p.ebitda_yoy_growth_pct <= -15.0) prof_pillar -= 0.25;
        }
        prof_pillar = std::clamp(prof_pillar, -1.0, 1.0);

        double lev_pillar = 0.0;
        if (std::isfinite(p.net_debt_to_ebitda)) {
            if (p.net_debt_to_ebitda < 0.0) lev_pillar = 0.85;         // Net Cash fortress
            else if (p.net_debt_to_ebitda <= 1.5) lev_pillar = 0.50;   // Low leverage
            else if (p.net_debt_to_ebitda <= 2.8) lev_pillar = 0.10;   // Manageable leverage
            else if (p.net_debt_to_ebitda <= 4.2) lev_pillar = -0.35;  // Elevated debt
            else lev_pillar = -0.80;                                   // Heavy debt burden
        } else if (raw.has_debt && raw.has_cash && p.net_debt_fin < 0.0) {
            lev_pillar = 0.50;
        }

        p.fundamental_score = std::clamp(0.40 * val_pillar + 0.35 * prof_pillar + 0.25 * lev_pillar, -1.0, 1.0);
        assign_grade_(p);

        std::string favok_disp = p.has_ebitda ? format_compact_money(p.ebitda_ttm_fin, p.financial_currency) : "N/A";
        std::string ev_eb_disp = std::isfinite(p.ev_to_ebitda) ? std::format("{:.1f}x", p.ev_to_ebitda) : "N/A";
        std::string nd_eb_disp = std::isfinite(p.net_debt_to_ebitda)
            ? (p.net_debt_to_ebitda < 0.0 ? std::format("{:.1f}x[Nakit]", p.net_debt_to_ebitda) : std::format("{:.1f}x", p.net_debt_to_ebitda))
            : "N/A";
        std::string mrg_disp = std::isfinite(p.ebitda_margin_pct) ? std::format("%{:.1f}", p.ebitda_margin_pct) : "N/A";
        std::string pb_disp = std::isfinite(p.pb_ratio) ? std::format("{:.2f}x", p.pb_ratio) : "N/A";

        p.summary_line = std::format(
            "FAVÖK: {} ({}) │ FD/FAVÖK: {} │ NetBorç/FAVÖK: {} │ PD/DD: {} │ Not: {}",
            favok_disp, mrg_disp, ev_eb_disp, nd_eb_disp, pb_disp, p.quality_grade
        );

        if (p.fundamental_score >= 0.35) {
            p.diagnosis = I18n::tr(
                "Güçlü FAVÖK üretimi, sağlıklı bilanço kaldıracı ve cazip çarpan iskonto desteği.",
                "Strong EBITDA generation, healthy balance-sheet leverage, and attractive valuation support.",
                "Starke EBITDA-Generierung, solide Bilanzstruktur und attraktive Bewertungsunterstützung."
            );
        } else if (p.fundamental_score >= -0.10) {
            p.diagnosis = I18n::tr(
                "Operasyonel FAVÖK marjı ve değerleme çarpanları makul/adil değer bandında dengeli.",
                "Operational EBITDA margin and valuation multiples are balanced near fair value.",
                "Operative EBITDA-Marge und Bewertungsmultiplikatoren im fairen Gleichgewicht."
            );
        } else {
            p.diagnosis = I18n::tr(
                "Yüksek çarpan primi, zayıf FAVÖK marjı veya yüksek Net Borç/FAVÖK kaldıracı temel risk oluşturuyor.",
                "Rich valuation multiple, weak EBITDA margin, or elevated Net Debt/EBITDA leverage poses risk.",
                "Hohe Bewertung, schwache EBITDA-Marge oder hohe Nettoverschuldung/EBITDA birgt Risiken."
            );
        }
    }

    static void assign_grade_(FundamentalProfile& p) {
        if (p.fundamental_score >= 0.50) p.quality_grade = "A+";
        else if (p.fundamental_score >= 0.25) p.quality_grade = "A";
        else if (p.fundamental_score >= 0.05) p.quality_grade = "B+";
        else if (p.fundamental_score >= -0.15) p.quality_grade = "B";
        else if (p.fundamental_score >= -0.40) p.quality_grade = "C";
        else p.quality_grade = "D";
    }
};

} // namespace quant

