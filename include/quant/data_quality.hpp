#pragma once

#include "data_telemetry.hpp"
#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "fundamentals.hpp"
#include "i18n.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <format>
#include <iostream>
#include <iomanip>
#include <cmath>

namespace quant {

class DataQualityDashboard : public DataQualityTelemetry {
public:
    // Record a fetch attempt
    static void record_fetch(const std::string& source, const std::string& endpoint,
                             bool success, std::chrono::milliseconds latency,
                             bool from_cache, const std::string& error = "",
                             long http_code = 0) {
        DataQualityTelemetry::record_fetch(source, endpoint, success, latency, from_cache, error, http_code);
    }

    // Record FX rate fetch
    static void record_fx_rate(const std::string& pair, double rate,
                               bool is_live, const std::string& source) {
        DataQualityTelemetry::record_fx_rate(pair, rate, is_live, source);
    }

    // Record asset data quality
    static void record_asset_quality(const RealTimeAssetData& data,
                                     const FundamentalProfile* fund = nullptr) {
        AssetDataQuality aq;
        aq.symbol = data.symbol;
        aq.is_live = data.is_live;
        aq.total_bars = (data.raw_bar_count > 0)
            ? data.raw_bar_count
            : std::max(data.historical_raw_closes.size(), data.historical_prices.size());
        aq.valid_bars = data.historical_prices.size();
        aq.null_bars_skipped = (data.raw_bar_count > 0)
            ? data.null_bars_skipped
            : (aq.total_bars > aq.valid_bars ? aq.total_bars - aq.valid_bars : 0);

        if (data.split_adjustments_applied > 0) {
            aq.split_adjustments = data.split_adjustments_applied;
        } else if (data.currency_code == "TRY" && data.instrument_type == "EQUITY" &&
                   data.historical_prices.size() >= 3) {
            for (size_t i = 1; i < data.historical_prices.size(); ++i) {
                double prev = data.historical_prices[i - 1];
                double curr = data.historical_prices[i];
                if (prev > 0.0 && curr > 0.0) {
                    double lr = std::log(curr / prev);
                    if (lr < -0.33 || lr > 0.35) aq.split_adjustments++;
                }
            }
        }

        if (!data.day_keys.empty()) {
            aq.first_date = static_cast<double>(data.day_keys.front());
            aq.last_date = static_cast<double>(data.day_keys.back());
        }

        if (fund && fund->is_available) {
            aq.has_fundamentals = true;
            aq.fundamentals_currency = fund->financial_currency;
            aq.fx_rate_used = fund->fx_fin_to_trade;
        } else if (data.is_synthetic_metal && data.underlying_usdtry > 0.0) {
            aq.fx_rate_used = data.underlying_usdtry;
        }

        aq.data_fetch_time = std::chrono::system_clock::now();
        if (fund) aq.fundamentals_fetch_time = aq.data_fetch_time;

        DataQualityTelemetry::record_asset_quality_entry(std::move(aq));
    }

    // Record news fetch
    static void record_news_fetch(const std::string& symbol, bool has_news, size_t count) {
        DataQualityTelemetry::record_news_fetch(symbol, has_news, count);
    }

    // Evaluate all data quality pillars and alerts
    static HealthSummary evaluate_health() {
        return DataQualityTelemetry::evaluate_health();
    }

    // Render the full dashboard
    static void render_dashboard() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        auto now = std::chrono::system_clock::now();
        constexpr int BOX_WIDTH = 101; // ╔ + 99×═ + ╗

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
        std::string hdr = I18n::tr(
            "║  📊 VERİ KALİTE & KAYNAK SAĞLIK PANOSU (DATA QUALITY & SOURCE HEALTH DASHBOARD)",
            "║  📊 DATA QUALITY & SOURCE HEALTH DASHBOARD",
            "║  📊 DATENQUALITÄTS- & QUELLEN-GESUNDHEITSDASHBOARD"
        );
        int pad = std::max(0, (BOX_WIDTH - 1) - ThemeEngine::utf8_visible_length(hdr));
        std::cout << hdr << std::string(static_cast<size_t>(pad), ' ') << "║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;

        // System timestamp
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_c);
        std::string ts = std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                                     now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
                                     now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
        std::cout << std::format("  {} {}\n\n", I18n::tr("Rapor Zamanı:", "Report Time:", "Berichtszeit:"), ts);

        // 1. API Endpoint Health
        render_api_health(pal);
        std::cout << "\n";

        // 2. FX Rate Status
        render_fx_status(pal, now);
        std::cout << "\n";

        // 3. Asset Data Quality
        render_asset_quality(pal, now);
        std::cout << "\n";

        // 4. Cache Performance
        render_cache_performance(pal);
        std::cout << "\n";

        // 5. Summary Score & Alerts
        HealthSummary summary = evaluate_health_unlocked_(now);
        render_summary_score(pal, summary);
        std::cout << "\n";
        render_alerts(pal, summary);

        std::cout << pal.glow_rgb;
        std::cout << "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝\n";
        std::cout << pal.reset_code;
    }

    // Quick inline status for other modules
    static std::string get_inline_status(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = asset_quality_.find(symbol);
        if (it == asset_quality_.end()) {
            std::string norm = LiveMarketDataClient::normalize_symbol(symbol);
            it = asset_quality_.find(norm);
        }
        if (it != asset_quality_.end()) {
            const auto& aq = it->second;
            auto now = std::chrono::system_clock::now();
            auto age_sec = std::chrono::duration_cast<std::chrono::seconds>(now - aq.data_fetch_time).count();
            std::string freshness = (!aq.is_live) ? "🔴"
                : (age_sec < 60) ? "🟢"
                : (age_sec < 300) ? "🟡" : "🔴";
            return std::format("{} Live:{} Bars:{} Nulls:{} Splits:{} Age:{}s",
                               freshness, aq.is_live ? "Y" : "N", aq.valid_bars,
                               aq.null_bars_skipped, aq.split_adjustments, age_sec);
        }
        return I18n::tr("Veri yok", "No data", "Keine Daten");
    }

    // Reset all metrics (for testing)
    static void reset() {
        DataQualityTelemetry::reset();
    }

private:
    static std::string format_duration(std::chrono::seconds dur) {
        if (dur.count() < 0) return "0s";
        if (dur.count() < 60) return std::format("{}s", dur.count());
        if (dur.count() < 3600) return std::format("{}m{}s", dur.count() / 60, dur.count() % 60);
        return std::format("{}h{}m", dur.count() / 3600, (dur.count() % 3600) / 60);
    }

    static std::string latency_color(std::chrono::milliseconds ms, const CrtColorPalette& pal) {
        if (ms.count() < 300) return pal.green_rgb;
        if (ms.count() < 700) return pal.glow_rgb;
        if (ms.count() < 1500) return "\033[33m"; // yellow
        return pal.red_rgb;
    }

    static std::string health_badge(bool live, const CrtColorPalette& pal) {
        return live ? (pal.green_rgb + I18n::tr("● CANLI", "● LIVE", "● LIVE") + pal.reset_code)
                    : (pal.red_rgb + I18n::tr("● ÇEVRİMDIŞI", "● OFFLINE", "● OFFLINE") + pal.reset_code);
    }

    static std::string score_color(double val, const CrtColorPalette& pal) {
        if (val >= 0.80) return pal.green_rgb;
        if (val >= 0.50) return pal.glow_rgb;
        return pal.red_rgb;
    }

    static void render_api_health(const CrtColorPalette& pal) {
        auto now = std::chrono::system_clock::now();

        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("API UÇ NOKTASI SAĞLIĞI", "API ENDPOINT HEALTH", "API-ENDPOINT-GESUNDHEIT")
                  << " ───╢\n" << pal.reset_code;

        std::cout << "  "
                  << ThemeEngine::pad_right(I18n::tr("Kaynak", "Source", "Quelle"), 25) << " "
                  << ThemeEngine::pad_right(I18n::tr("Endpoint", "Endpoint", "Endpunkt"), 48) << " "
                  << ThemeEngine::pad_right(I18n::tr("Durum", "Status", "Status"), 20) << " "
                  << pad_left(I18n::tr("Gecikme", "Latency", "Latenz"), 8) << " "
                  << pad_left(I18n::tr("Başarılı/Toplam", "Success/Total", "Erfolg/Gesamt"), 15) << " "
                  << pad_left(I18n::tr("Cache Hit%", "Cache Hit%", "Cache Treffer%"), 10) << " "
                  << I18n::tr("Son Hata", "Last Error", "Letzter Fehler") << "\n";
        std::cout << "  " << std::string(148, '-') << "\n";

        for (const auto& [src, ep] : expected_endpoints()) {
            std::string ep_disp = utf8_truncate(ep, 48);
            auto it = metrics_.find(src);
            if (it == metrics_.end() || it->second.total_requests == 0) {
                std::cout << "  "
                          << ThemeEngine::pad_right(src, 25) << " "
                          << ThemeEngine::pad_right(ep_disp, 48) << " "
                          << ThemeEngine::pad_right(pal.red_rgb + "N/A" + pal.reset_code, 20) << " "
                          << pad_left("N/A", 8) << " "
                          << pad_left("0/0", 15) << " "
                          << pad_left("N/A", 10) << " "
                          << "—" << "\n";
                continue;
            }
            const auto& m = it->second;
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - m.last_fetch_time);
            std::string age_str = (m.last_fetch_time.time_since_epoch().count() == 0) ? "Never"
                : std::format("{} ago", format_duration(age));
            std::string latency_str = std::format("{}ms", m.last_latency.count());
            std::string success_str = std::format("{}/{}", m.successful_requests, m.total_requests);
            double hit_rate = (m.cache_hits + m.cache_misses > 0)
                ? (100.0 * static_cast<double>(m.cache_hits) / static_cast<double>(m.cache_hits + m.cache_misses))
                : 0.0;
            std::string cache_str = std::format("{:.0f}%", hit_rate);
            std::string err_raw = m.last_error.empty() ? "—" : m.last_error;
            std::string err_str = m.last_error.empty() ? "—" : utf8_truncate(err_raw, 24);

            std::string status_col = health_badge(m.is_live, pal) + " " + age_str;
            std::string lat_col = latency_color(m.last_latency, pal) + latency_str + pal.reset_code;

            std::cout << "  "
                      << ThemeEngine::pad_right(src, 25) << " "
                      << ThemeEngine::pad_right(ep_disp, 48) << " "
                      << ThemeEngine::pad_right(status_col, 20) << " "
                      << pad_left(lat_col, 8) << " "
                      << pad_left(success_str, 15) << " "
                      << pad_left(cache_str, 10) << " "
                      << (m.last_error.empty() ? err_str : (pal.red_rgb + err_str + pal.reset_code)) << "\n";
        }
    }

    static void render_fx_status(const CrtColorPalette& pal, std::chrono::system_clock::time_point now) {
        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("DÖVİZ KURLARI (FX RATES)", "FX RATES", "WECHSELKURSE")
                  << " ───╢\n" << pal.reset_code;

        std::cout << "  "
                  << ThemeEngine::pad_right(I18n::tr("Parite", "Pair", "Paar"), 10) << " "
                  << pad_left(I18n::tr("Kur", "Rate", "Kurs"), 11) << "  "
                  << ThemeEngine::pad_right(I18n::tr("Durum", "Status", "Status"), 14) << " "
                  << ThemeEngine::pad_right(I18n::tr("Yaş", "Age", "Alter"), 10) << " "
                  << ThemeEngine::pad_right(I18n::tr("Kaynak", "Source", "Quelle"), 14) << " "
                  << I18n::tr("Not", "Note", "Hinweis") << "\n";
        std::cout << "  " << std::string(79, '-') << "\n";

        for (const auto& pair : expected_fx_pairs()) {
            auto it = fx_rates_.find(pair);
            if (it == fx_rates_.end()) {
                std::cout << "  "
                          << ThemeEngine::pad_right(pair, 10) << " "
                          << pad_left("N/A", 11) << "  "
                          << ThemeEngine::pad_right(pal.red_rgb + "N/A" + pal.reset_code, 14) << " "
                          << ThemeEngine::pad_right("Never", 10) << " "
                          << ThemeEngine::pad_right("N/A", 14) << " "
                          << "—" << "\n";
                continue;
            }
            const auto& fx = it->second;
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - fx.fetch_time);
            std::string age_str = (fx.fetch_time.time_since_epoch().count() == 0) ? "Never"
                : std::format("{} ago", format_duration(age));
            std::string status = health_badge(fx.is_live, pal);
            std::string source_str = fx.is_live
                ? (pal.glow_rgb + (fx.source.empty() ? "Live API" : fx.source) + pal.reset_code)
                : (pal.red_rgb + "Fallback" + pal.reset_code);
            std::string note = "—";
            if (!fx.is_live) {
                if (pair == "USD/TRY") note = "=38.00 const";
                else if (pair == "EUR/TRY") note = "=41.00 const";
                else if (pair == "EUR/USD") note = "=1.080 const";
                else if (pair == "TRY/USD") note = "=1/38.0 const";
                else if (pair == "TRY/EUR") note = "=1/41.0 const";
                else note = "Fallback const";
            }

            std::cout << "  "
                      << ThemeEngine::pad_right(pair, 10) << " "
                      << pad_left(std::format("{:.4f}", fx.rate), 11) << "  "
                      << ThemeEngine::pad_right(status, 14) << " "
                      << ThemeEngine::pad_right(age_str, 10) << " "
                      << ThemeEngine::pad_right(source_str, 14) << " "
                      << (fx.is_live ? note : (pal.red_rgb + note + pal.reset_code)) << "\n";
        }
    }

    static void render_asset_quality(const CrtColorPalette& pal, std::chrono::system_clock::time_point now) {
        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("VARLIK VERİ KALİTESİ", "ASSET DATA QUALITY", "ANLAGENDATENQUALITÄT")
                  << " ───╢\n" << pal.reset_code;

        if (asset_quality_.empty()) {
            std::cout << "  " << pal.red_rgb
                      << I18n::tr("Henüz varlık veri kalitesi kaydı yok.", "No asset data quality records captured.", "Keine Anlagendaten erfasst.")
                      << pal.reset_code << "\n";
            return;
        }

        std::cout << "  "
                  << ThemeEngine::pad_right(I18n::tr("Sembol", "Symbol", "Symbol"), 15) << " "
                  << pad_left(I18n::tr("Canlı", "Live", "Live"), 6) << " "
                  << pad_left(I18n::tr("Barlar", "Bars", "Bars"), 7) << " "
                  << pad_left(I18n::tr("Geçerli", "Valid", "Gültig"), 7) << " "
                  << pad_left(I18n::tr("Null", "Null", "Null"), 6) << " "
                  << pad_left(I18n::tr("Bölünmeler", "Splits", "Splits"), 10) << " "
                  << pad_left(I18n::tr("Yaş", "Age", "Alter"), 8) << " "
                  << ThemeEngine::pad_right(I18n::tr("Fund.", "Fund.", "Fund."), 6) << " "
                  << ThemeEngine::pad_right(I18n::tr("Haber", "News", "News"), 6) << " "
                  << pad_left(I18n::tr("FX Kur", "FX Rate", "FX Kurs"), 8) << "\n";
        std::cout << "  " << std::string(96, '-') << "\n";

        for (const auto& [sym, aq] : asset_quality_) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - aq.data_fetch_time);
            std::string age_str = format_duration(age);
            std::string live_badge = aq.is_live ? (pal.green_rgb + "✓" + pal.reset_code)
                                                : (pal.red_rgb + "✗" + pal.reset_code);
            std::string fund_badge = aq.has_fundamentals ? (pal.green_rgb + "✓" + pal.reset_code)
                                                         : (pal.dim_rgb + "—" + pal.reset_code);
            std::string news_badge = aq.has_news ? (pal.green_rgb + "✓" + pal.reset_code)
                                                 : (pal.dim_rgb + "—" + pal.reset_code);
            std::string fx_str = aq.fx_rate_used > 1.01 ? std::format("{:.2f}", aq.fx_rate_used) : "1.00";

            std::cout << "  "
                      << ThemeEngine::pad_right(sym, 15) << " "
                      << pad_left(live_badge, 6) << " "
                      << pad_left(std::to_string(aq.total_bars), 7) << " "
                      << pad_left(std::to_string(aq.valid_bars), 7) << " "
                      << pad_left(std::to_string(aq.null_bars_skipped), 6) << " "
                      << pad_left(std::to_string(aq.split_adjustments), 10) << " "
                      << pad_left(age_str, 8) << " "
                      << ThemeEngine::pad_right(fund_badge, 6) << " "
                      << ThemeEngine::pad_right(news_badge, 6) << " "
                      << pad_left(fx_str, 8) << "\n";
        }
    }

    static void render_cache_performance(const CrtColorPalette& pal) {
        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("ÖNBELLEK PERFORMANSI", "CACHE PERFORMANCE", "CACHE-LEISTUNG")
                  << " ───╢\n" << pal.reset_code;

        size_t total_hits = 0, total_misses = 0, total_reqs = 0, total_ok = 0, total_fail = 0;
        for (const auto& [_, m] : metrics_) {
            total_hits += m.cache_hits;
            total_misses += m.cache_misses;
            total_reqs += m.total_requests;
            total_ok += m.successful_requests;
            total_fail += m.failed_requests;
        }

        double hit_pct = (total_hits + total_misses > 0)
            ? 100.0 * static_cast<double>(total_hits) / static_cast<double>(total_hits + total_misses)
            : 0.0;
        double success_pct = (total_reqs > 0)
            ? 100.0 * static_cast<double>(total_ok) / static_cast<double>(total_reqs)
            : 0.0;

        std::vector<std::string> lines = {
            std::format("  {}: {} / {}  ({:.1f}%)", I18n::tr("Cache Hit/Miss", "Cache Hit/Miss", "Cache Treffer/Fehl"),
                        total_hits, total_misses, hit_pct),
            std::format("  {}: {} / {}  ({:.1f}%)", I18n::tr("Başarı Oranı", "Success Rate", "Erfolgsrate"),
                        total_ok, total_reqs, success_pct),
            std::format("  {}: {}", I18n::tr("Toplam İstek", "Total Requests", "Gesamtanfragen"), total_reqs),
            std::format("  {}: {}", I18n::tr("Toplam Hata", "Total Errors", "Gesamtfehler"), total_fail)
        };

        for (const auto& line : lines) {
            std::cout << line << "\n";
        }
    }

    static void render_summary_score(const CrtColorPalette& pal, const HealthSummary& summary) {
        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("ÖZET SKOR", "SUMMARY SCORE", "ZUSAMMENFASSUNG")
                  << " ───╢\n" << pal.reset_code;

        std::string overall_color = score_color(summary.overall_score, pal);
        std::string overall_label;
        if (summary.has_critical()) {
            overall_label = I18n::tr("KRİTİK UYARI — DİKKAT GEREKTİRİYOR",
                                     "CRITICAL ALERT — NEEDS ATTENTION",
                                     "KRITISCHE WARNUNG — HANDLUNGSBEDARF");
        } else if (summary.overall_score >= 0.80) {
            overall_label = I18n::tr("MÜKEMMEL", "EXCELLENT", "AUSGEZEICHNET");
        } else if (summary.overall_score >= 0.50) {
            overall_label = I18n::tr("İYİ / KISMEN UYARI", "GOOD / MINOR WARNINGS", "GUT / LEICHTE WARNUNGEN");
        } else {
            overall_label = I18n::tr("DİKKAT GEREKTİRİYOR", "NEEDS ATTENTION", "BEDARF BEACHTUNG");
        }

        std::cout << std::format("  {}: {}{:.2f}/1.00{}  ({}{}{})\n",
            I18n::tr("Genel Veri Sağlık Skoru", "Overall Data Health Score", "Gesamtdaten-Gesundheitswert"),
            overall_color, summary.overall_score, pal.reset_code,
            overall_color, overall_label, pal.reset_code);

        std::cout << std::format("  • {}: {}{:.2f}/1.00{}  ({}/{} {})\n",
            I18n::tr("API Sağlığı", "API Health", "API-Gesundheit"),
            score_color(summary.api_score, pal), summary.api_score, pal.reset_code,
            summary.api_live_count, summary.api_expected_count,
            I18n::tr("canlı", "live", "live"));

        std::cout << std::format("  • {}: {}{:.2f}/1.00{}  ({}/{} {})\n",
            I18n::tr("FX Veri Kalitesi", "FX Data Quality", "FX-Datenqualität"),
            score_color(summary.fx_score, pal), summary.fx_score, pal.reset_code,
            summary.fx_live_count, summary.fx_expected_count,
            I18n::tr("canlı", "live", "live"));

        std::cout << std::format("  • {}: {}{:.2f}/1.00{}  ({}/{} {})\n",
            I18n::tr("Varlık Veri Bütünlüğü", "Asset Data Integrity", "Anlagendaten-Integrität"),
            score_color(summary.asset_score, pal), summary.asset_score, pal.reset_code,
            summary.asset_live_count, summary.asset_tracked_count,
            I18n::tr("canlı", "live", "live"));
    }

    static void render_alerts(const CrtColorPalette& pal, const HealthSummary& summary) {
        std::cout << pal.glow_rgb << "╟─── "
                  << I18n::tr("VERİ SAĞLIK ALARMLARI & UYARILAR", "DATA HEALTH ALERTS & WARNINGS", "DATENQUALITÄTS-ALARME & WARNUNGEN")
                  << " ───╢\n" << pal.reset_code;

        if (summary.alerts.empty()) {
            std::cout << "  " << pal.green_rgb
                      << I18n::tr("✓ [OK] Tüm API uç noktaları, FX kurları ve varlık akışları sağlıklı (0 uyarı).",
                                  "✓ [OK] All API endpoints, FX rates, and asset feeds are healthy (0 alerts).",
                                  "✓ [OK] Alle API-Endpunkte, FX-Kurse und Asset-Feeds sind gesund (0 Alarme).")
                      << pal.reset_code << "\n";
            return;
        }

        for (const auto& alert : summary.alerts) {
            if (alert.severity == HealthAlert::Severity::Critical) {
                std::cout << "  " << pal.red_rgb
                          << I18n::tr("🚨 [KRİTİK] ", "🚨 [CRITICAL] ", "🚨 [KRITISCH] ")
                          << alert.message << pal.reset_code << "\n";
            } else if (alert.severity == HealthAlert::Severity::Warning) {
                std::cout << "  \033[33m"
                          << I18n::tr("⚠️  [UYARI]  ", "⚠️  [WARNING]  ", "⚠️  [WARNUNG]  ")
                          << alert.message << pal.reset_code << "\n";
            } else {
                std::cout << "  " << pal.dim_rgb
                          << I18n::tr("ℹ  [BİLGİ]  ", "ℹ  [INFO]     ", "ℹ  [INFO]     ")
                          << alert.message << pal.reset_code << "\n";
            }
        }
    }
};

} // namespace quant
