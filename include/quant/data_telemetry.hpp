#pragma once

#include "theme_engine.hpp"
#include "i18n.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace quant {

struct DataSourceMetrics {
    std::string source_name;
    std::string endpoint;
    bool is_live{false};
    std::chrono::system_clock::time_point last_fetch_time{};
    std::chrono::milliseconds last_latency{0};
    size_t total_requests{0};
    size_t successful_requests{0};
    size_t failed_requests{0};
    size_t cache_hits{0};
    size_t cache_misses{0};
    std::string last_error;
    std::string http_status;
};

struct FXRateInfo {
    std::string pair;
    double rate{0.0};
    std::chrono::system_clock::time_point fetch_time{};
    bool is_live{false};
    std::string source; // "live", "live (inv)", "live (cross)", or "fallback"
};

struct AssetDataQuality {
    std::string symbol;
    bool is_live{false};
    size_t total_bars{0};
    size_t valid_bars{0};
    size_t null_bars_skipped{0};
    size_t split_adjustments{0};
    double first_date{0.0};
    double last_date{0.0};
    bool has_fundamentals{false};
    bool has_news{false};
    std::string fundamentals_currency;
    double fx_rate_used{1.0};
    std::chrono::system_clock::time_point data_fetch_time{};
    std::chrono::system_clock::time_point fundamentals_fetch_time{};
};

struct HealthAlert {
    enum class Severity { Critical, Warning, Info };
    Severity severity{Severity::Warning};
    std::string code;
    std::string message;
};

struct HealthSummary {
    double overall_score{0.0};
    double api_score{0.0};
    double fx_score{0.0};
    double asset_score{0.0};
    size_t api_tracked_count{0};
    size_t api_expected_count{5};
    size_t api_live_count{0};
    size_t fx_tracked_count{0};
    size_t fx_expected_count{5};
    size_t fx_live_count{0};
    size_t asset_tracked_count{0};
    size_t asset_live_count{0};
    std::vector<HealthAlert> alerts;

    bool has_critical() const {
        for (const auto& a : alerts) {
            if (a.severity == HealthAlert::Severity::Critical) return true;
        }
        return false;
    }

    bool has_warning() const {
        for (const auto& a : alerts) {
            if (a.severity == HealthAlert::Severity::Warning) return true;
        }
        return false;
    }
};

class DataQualityTelemetry {
public:
    static const std::vector<std::pair<std::string, std::string>>& expected_endpoints() {
        static const std::vector<std::pair<std::string, std::string>> endpoints = {
            {"Yahoo v8 Chart",           "query1.finance.yahoo.com/v8/finance/chart"},
            {"Yahoo v10 QuoteSummary",   "query2.finance.yahoo.com/v10/finance/quoteSummary"},
            {"Yahoo v1 Fundamentals-TS", "query2.finance.yahoo.com/ws/fundamentals-timeseries/v1"},
            {"Yahoo v1 News Search",     "query2.finance.yahoo.com/v1/finance/search"},
            {"Yahoo Treasury Yields",    "query1.finance.yahoo.com/v8/finance/chart/^IRX,^FVX,^TNX,^TYX"}
        };
        return endpoints;
    }

    static const std::vector<std::string>& expected_fx_pairs() {
        static const std::vector<std::string> pairs = {
            "USD/TRY", "EUR/TRY", "EUR/USD", "TRY/USD", "TRY/EUR"
        };
        return pairs;
    }

    static std::string utf8_truncate(const std::string& text, size_t max_chars) {
        if (max_chars == 0) return {};
        size_t chars = 0;
        size_t i = 0;
        size_t prev_i = 0;
        while (i < text.size()) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            size_t seq_len = 1;
            if ((c & 0x80) == 0x00) seq_len = 1;
            else if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            if (i + seq_len > text.size()) break;
            if (chars == max_chars) {
                if (max_chars >= 2) {
                    return text.substr(0, prev_i) + "…";
                }
                return text.substr(0, i);
            }
            prev_i = i;
            i += seq_len;
            ++chars;
        }
        return text.substr(0, i);
    }

    static std::string pad_left(const std::string& text, int target_width) {
        int pad = std::max(0, target_width - ThemeEngine::utf8_visible_length(text));
        return std::string(static_cast<size_t>(pad), ' ') + text;
    }

    static void record_fetch(const std::string& source, const std::string& endpoint,
                             bool success, std::chrono::milliseconds latency,
                             bool from_cache, const std::string& error = "",
                             long http_code = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& m = metrics_[source];
        m.source_name = source;
        m.endpoint = endpoint;
        m.total_requests++;
        m.last_fetch_time = std::chrono::system_clock::now();
        // Preserve actual network latency when subsequent calls are served from cache
        if (!from_cache || m.last_latency.count() == 0) {
            m.last_latency = latency;
        }
        if (from_cache) {
            m.cache_hits++;
        } else {
            m.cache_misses++;
        }
        if (success) {
            m.successful_requests++;
            m.is_live = true;
            m.last_error.clear();
        } else {
            m.failed_requests++;
            m.is_live = false;
            if (!error.empty()) {
                m.last_error = error;
            } else if (http_code > 0) {
                m.last_error = std::format("HTTP {}", http_code);
            } else {
                m.last_error = "Request failed";
            }
        }
        if (http_code > 0) m.http_status = std::to_string(http_code);
    }

    static void record_fx_rate(const std::string& pair, double rate,
                               bool is_live, const std::string& source) {
        std::lock_guard<std::mutex> lock(mutex_);
        FXRateInfo info;
        info.pair = pair;
        info.rate = rate;
        info.fetch_time = std::chrono::system_clock::now();
        info.is_live = is_live;
        info.source = source;
        fx_rates_[pair] = std::move(info);
    }

    static void record_asset_quality_entry(AssetDataQuality aq) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = asset_quality_.find(aq.symbol); it != asset_quality_.end()) {
            if (!aq.has_news && it->second.has_news) {
                aq.has_news = true;
            }
        }
        asset_quality_[aq.symbol] = std::move(aq);
    }

    static void record_news_fetch(const std::string& symbol, bool has_news, size_t /*count*/) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = asset_quality_.find(symbol); it != asset_quality_.end()) {
            it->second.has_news = has_news;
        }
    }

    static HealthSummary evaluate_health() {
        std::lock_guard<std::mutex> lock(mutex_);
        return evaluate_health_unlocked_(std::chrono::system_clock::now());
    }

    static void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.clear();
        fx_rates_.clear();
        asset_quality_.clear();
    }

protected:
    inline static std::mutex mutex_;
    inline static std::unordered_map<std::string, DataSourceMetrics> metrics_;
    inline static std::unordered_map<std::string, FXRateInfo> fx_rates_;
    inline static std::unordered_map<std::string, AssetDataQuality> asset_quality_;

    static HealthSummary evaluate_health_unlocked_(std::chrono::system_clock::time_point now) {
        HealthSummary s;
        const auto& exp_eps = expected_endpoints();
        const auto& exp_fx = expected_fx_pairs();
        s.api_expected_count = exp_eps.size();
        s.fx_expected_count = exp_fx.size();

        // 1. API Endpoint Health
        double api_sum = 0.0;
        size_t api_count = 0;
        size_t exp_ep_tracked = 0;
        std::vector<std::string> unprobed_eps;

        for (const auto& [src, _] : exp_eps) {
            auto it = metrics_.find(src);
            if (it == metrics_.end() || it->second.total_requests == 0) {
                unprobed_eps.push_back(src);
            } else {
                ++exp_ep_tracked;
            }
        }

        for (const auto& [src, m] : metrics_) {
            if (m.total_requests == 0) continue;
            ++api_count;
            if (m.is_live) ++s.api_live_count;

            double success_rate = static_cast<double>(m.successful_requests) /
                                  static_cast<double>(m.total_requests);
            double lat_ms = static_cast<double>(m.last_latency.count());
            double latency_factor = 1.0;
            if (lat_ms > 600.0) {
                latency_factor = 1.0 - 0.40 * std::min(1.0, (lat_ms - 600.0) / 2400.0);
            }

            if (!m.is_live) {
                std::string err_detail = m.last_error.empty() ? "OFFLINE" : m.last_error;
                if (!m.http_status.empty() && err_detail.find("HTTP") == std::string::npos) {
                    err_detail = std::format("HTTP {} — {}", m.http_status, err_detail);
                }
                s.alerts.push_back({
                    HealthAlert::Severity::Critical,
                    "API_ENDPOINT_OFFLINE",
                    I18n::tr(
                        std::format("{} ÇEVRİMDIŞI ({}/{}, son hata: {}).", src, m.successful_requests, m.total_requests, err_detail),
                        std::format("{} is OFFLINE ({}/{}, last error: {}).", src, m.successful_requests, m.total_requests, err_detail),
                        std::format("{} ist OFFLINE ({}/{}, letzter Fehler: {}).", src, m.successful_requests, m.total_requests, err_detail)
                    )
                });
            } else {
                api_sum += success_rate * latency_factor;
                if (m.failed_requests > 0) {
                    s.alerts.push_back({
                        HealthAlert::Severity::Warning,
                        "API_ENDPOINT_DEGRADED",
                        I18n::tr(
                            std::format("{} kısmi hata verdi ({}/{} başarılı).", src, m.successful_requests, m.total_requests),
                            std::format("{} experienced partial failures ({}/{} succeeded).", src, m.successful_requests, m.total_requests),
                            std::format("{} hatte Teilausfälle ({}/{} erfolgreich).", src, m.successful_requests, m.total_requests)
                        )
                    });
                }
            }
        }

        s.api_tracked_count = api_count;
        if (api_count == 0) {
            s.api_score = 0.0;
            s.alerts.push_back({
                HealthAlert::Severity::Critical,
                "API_TELEMETRY_EMPTY",
                I18n::tr(
                    std::format("API Sağlığı 0.00/1.00 — Hiçbir API isteği kaydedilmedi (0/{} uç nokta).", s.api_expected_count),
                    std::format("API Health is 0.00/1.00 — No API requests were recorded (0/{} endpoints).", s.api_expected_count),
                    std::format("API-Gesundheit ist 0.00/1.00 — Keine API-Anfragen erfasst (0/{} Endpunkte).", s.api_expected_count)
                )
            });
        } else {
            for (const auto& src : unprobed_eps) {
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "API_ENDPOINT_UNPROBED",
                    I18n::tr(
                        std::format("{} uç noktası için telemetri yok (0/0 istek).", src),
                        std::format("No telemetry recorded for endpoint {} (0/0 requests).", src),
                        std::format("Keine Telemetrie für Endpunkt {} erfasst (0/0 Anfragen).", src)
                    )
                });
            }
            double raw_api = api_sum / static_cast<double>(api_count);
            double cov = static_cast<double>(exp_ep_tracked) /
                         static_cast<double>(std::max<size_t>(1, exp_eps.size()));
            s.api_score = raw_api * (0.50 + 0.50 * std::min(1.0, cov));
        }

        // 2. FX Rate Health
        double fx_sum = 0.0;
        size_t fx_count = 0;
        size_t exp_fx_tracked = 0;
        std::vector<std::string> unprobed_fx;

        for (const auto& pair : exp_fx) {
            auto it = fx_rates_.find(pair);
            if (it == fx_rates_.end()) {
                unprobed_fx.push_back(pair);
            } else {
                ++exp_fx_tracked;
            }
        }

        for (const auto& [pair, fx] : fx_rates_) {
            ++fx_count;
            auto age_sec = std::chrono::duration_cast<std::chrono::seconds>(now - fx.fetch_time).count();
            double freshness = 1.0;
            if (fx.fetch_time.time_since_epoch().count() == 0 || age_sec > 3600) {
                freshness = 0.40;
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "FX_STALE",
                    I18n::tr(
                        std::format("{} FX kuru bayat ({} sn önce güncellendi).", pair, std::max<long long>(0, age_sec)),
                        std::format("FX rate {} is stale (updated {}s ago).", pair, std::max<long long>(0, age_sec)),
                        std::format("FX-Kurs {} ist veraltet (vor {}s aktualisiert).", pair, std::max<long long>(0, age_sec))
                    )
                });
            } else if (age_sec > 300) {
                freshness = 0.75;
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "FX_STALE",
                    I18n::tr(
                        std::format("{} FX kuru 5 dakikadan eski ({} sn).", pair, age_sec),
                        std::format("FX rate {} is older than 5 minutes ({}s).", pair, age_sec),
                        std::format("FX-Kurs {} ist älter als 5 Minuten ({}s).", pair, age_sec)
                    )
                });
            }

            if (!std::isfinite(fx.rate) || fx.rate <= 0.0) {
                s.alerts.push_back({
                    HealthAlert::Severity::Critical,
                    "FX_INVALID_RATE",
                    I18n::tr(
                        std::format("{} için geçersiz kur değeri ({}).", pair, fx.rate),
                        std::format("Invalid FX rate value ({}) for {}.", fx.rate, pair),
                        std::format("Ungültiger FX-Kurs ({}) für {}.", fx.rate, pair)
                    )
                });
            } else if (!fx.is_live) {
                fx_sum += 0.20 * freshness;
                s.alerts.push_back({
                    HealthAlert::Severity::Critical,
                    "FX_FALLBACK_ACTIVE",
                    I18n::tr(
                        std::format("{} canlı kur alınamadı — sabit fallback ({:.4f}) kullanılıyor!", pair, fx.rate),
                        std::format("Live FX feed failed for {} — using static fallback ({:.4f})!", pair, fx.rate),
                        std::format("Live-FX-Feed für {} ausgefallen — statischer Fallback ({:.4f}) aktiv!", pair, fx.rate)
                    )
                });
            } else {
                ++s.fx_live_count;
                fx_sum += 1.0 * freshness;
            }
        }

        s.fx_tracked_count = fx_count;
        if (fx_count == 0) {
            s.fx_score = 0.0;
            s.alerts.push_back({
                HealthAlert::Severity::Critical,
                "FX_TELEMETRY_EMPTY",
                I18n::tr(
                    std::format("FX Veri Kalitesi 0.00/1.00 — Hiçbir döviz kuru kaydedilmedi (0/{} parite).", s.fx_expected_count),
                    std::format("FX Data Quality is 0.00/1.00 — No FX rates were recorded (0/{} pairs).", s.fx_expected_count),
                    std::format("FX-Datenqualität ist 0.00/1.00 — Keine Wechselkurse erfasst (0/{} Paare).", s.fx_expected_count)
                )
            });
        } else {
            for (const auto& pair : unprobed_fx) {
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "FX_PAIR_UNPROBED",
                    I18n::tr(
                        std::format("{} döviz kuru henüz sorgulanmadı (N/A).", pair),
                        std::format("FX pair {} has not been queried yet (N/A).", pair),
                        std::format("Wechselkurs {} wurde noch nicht abgefragt (N/A).", pair)
                    )
                });
            }
            double raw_fx = fx_sum / static_cast<double>(fx_count);
            double cov = static_cast<double>(exp_fx_tracked) /
                         static_cast<double>(std::max<size_t>(1, exp_fx.size()));
            s.fx_score = raw_fx * (0.50 + 0.50 * std::min(1.0, cov));
        }

        // 3. Asset Data Quality
        double data_sum = 0.0;
        size_t data_count = 0;

        for (const auto& [sym, aq] : asset_quality_) {
            ++data_count;
            double completeness = (aq.total_bars > 0)
                ? std::min(1.0, static_cast<double>(aq.valid_bars) / static_cast<double>(aq.total_bars))
                : (aq.valid_bars > 0 ? 1.0 : 0.0);

            auto age_sec = std::chrono::duration_cast<std::chrono::seconds>(now - aq.data_fetch_time).count();
            double freshness = 1.0;
            if (aq.data_fetch_time.time_since_epoch().count() == 0 || age_sec > 3600) {
                freshness = 0.40;
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "ASSET_STALE",
                    I18n::tr(
                        std::format("{} varlık verisi bayat ({} sn).", sym, std::max<long long>(0, age_sec)),
                        std::format("Asset data for {} is stale ({}s old).", sym, std::max<long long>(0, age_sec)),
                        std::format("Anlagendaten für {} sind veraltet ({}s).", sym, std::max<long long>(0, age_sec))
                    )
                });
            } else if (age_sec > 300) {
                freshness = 0.75;
                s.alerts.push_back({
                    HealthAlert::Severity::Warning,
                    "ASSET_STALE",
                    I18n::tr(
                        std::format("{} varlık verisi 5 dakikadan eski ({} sn).", sym, age_sec),
                        std::format("Asset data for {} is older than 5 minutes ({}s).", sym, age_sec),
                        std::format("Anlagendaten für {} sind älter als 5 Minuten ({}s).", sym, age_sec)
                    )
                });
            }

            if (!aq.is_live) {
                s.alerts.push_back({
                    HealthAlert::Severity::Critical,
                    "ASSET_OFFLINE",
                    I18n::tr(
                        std::format("{} canlı piyasa verisi ÇEVRİMDIŞI (fallback/simülasyon verisi kullanılıyor)!", sym),
                        std::format("Live market data for {} is OFFLINE (fallback/simulated data active)!", sym),
                        std::format("Live-Marktdaten für {} sind OFFLINE (Fallback-Daten aktiv)!", sym)
                    )
                });
            } else if (aq.valid_bars == 0) {
                s.alerts.push_back({
                    HealthAlert::Severity::Critical,
                    "ASSET_EMPTY",
                    I18n::tr(
                        std::format("{} için geçerli fiyat barı bulunamadı (0 bar)!", sym),
                        std::format("No valid price bars found for {} (0 bars)!", sym),
                        std::format("Keine gültigen Kurs-Bars für {} gefunden (0 Bars)!", sym)
                    )
                });
            } else {
                ++s.asset_live_count;
                data_sum += completeness * freshness;
                if (completeness < 0.85 || aq.null_bars_skipped > 3) {
                    s.alerts.push_back({
                        HealthAlert::Severity::Warning,
                        "ASSET_NULL_BARS",
                        I18n::tr(
                            std::format("{} serisinde {} null/eksik bar atlandı (tamlık: %{:.0f}).",
                                        sym, aq.null_bars_skipped, completeness * 100.0),
                            std::format("{} skipped {} null/missing bars (completeness: {:.0f}%).",
                                        sym, aq.null_bars_skipped, completeness * 100.0),
                            std::format("{} übersprang {} Null-Bars (Vollständigkeit: {:.0f}%).",
                                        sym, aq.null_bars_skipped, completeness * 100.0)
                        )
                    });
                }
                if (aq.split_adjustments > 0) {
                    s.alerts.push_back({
                        HealthAlert::Severity::Info,
                        "ASSET_SPLIT_ADJUSTED",
                        I18n::tr(
                            std::format("{} serisinde {} bölünme/bedelsiz fiyat düzeltmesi tespit edilip uygulandı.",
                                        sym, aq.split_adjustments),
                            std::format("Detected & adjusted {} unadjusted split event(s) in {}.",
                                        aq.split_adjustments, sym),
                            std::format("{} unbereinigte Split-Ereignisse bei {} korrigiert.",
                                        aq.split_adjustments, sym)
                        )
                    });
                }
            }
        }

        s.asset_tracked_count = data_count;
        if (data_count == 0) {
            s.asset_score = 0.0;
            s.alerts.push_back({
                HealthAlert::Severity::Critical,
                "ASSET_TELEMETRY_EMPTY",
                I18n::tr(
                    "Varlık Veri Bütünlüğü 0.00/1.00 — Hiçbir varlık verisi doğrulanmadı.",
                    "Asset Data Integrity is 0.00/1.00 — No asset quality records captured.",
                    "Anlagendaten-Integrität ist 0.00/1.00 — Keine Anlagendaten erfasst."
                )
            });
        } else {
            s.asset_score = data_sum / static_cast<double>(data_count);
        }

        // 4. Overall Health Score: ALWAYS combine all 3 pillars & cap if critical/warning alerts exist
        double raw_overall = (s.api_score + s.fx_score + s.asset_score) / 3.0;

        bool any_pillar_zero = (s.api_score <= 0.0 || s.fx_score <= 0.0 || s.asset_score <= 0.0);
        if (any_pillar_zero) {
            s.overall_score = std::min(raw_overall, 0.45);
            s.alerts.push_back({
                HealthAlert::Severity::Critical,
                "PILLAR_ZERO",
                I18n::tr(
                    std::format("Kritik alt skor sıfır (API:{:.2f}, FX:{:.2f}, Varlık:{:.2f}) — Genel skor sınırlandı!",
                                s.api_score, s.fx_score, s.asset_score),
                    std::format("Critical pillar score is zero (API:{:.2f}, FX:{:.2f}, Asset:{:.2f}) — Overall score capped!",
                                s.api_score, s.fx_score, s.asset_score),
                    std::format("Kritischer Teilwert ist null (API:{:.2f}, FX:{:.2f}, Asset:{:.2f}) — Gesamtwert begrenzt!",
                                s.api_score, s.fx_score, s.asset_score)
                )
            });
        } else if (s.has_critical()) {
            s.overall_score = std::min(raw_overall, 0.49);
        } else if (s.has_warning()) {
            s.overall_score = std::min(raw_overall, 0.79);
        } else {
            s.overall_score = raw_overall;
        }

        return s;
    }
};

} // namespace quant
