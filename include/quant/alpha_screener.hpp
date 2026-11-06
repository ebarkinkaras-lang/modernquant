#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
#include "fundamentals.hpp"
#include "i18n.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <format>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace quant {

enum class AlphaCategory {
    HiddenAccumulation,   // 💎 GİZLİ ALFA / RADAR ALTI DİP AKÜMÜLASYONU
    VolatilitySqueeze,    // ⚡ VOLATİLİTE SIKIŞMASI (PATLAMA ÖNCESİ SESSİZLİK)
    EarlyMomentum,        // 📈 ERKEN MOMENTUM (HENÜZ MANŞETLERE DÜŞMEMİŞ)
    CrowdedTradeWarning   // ⚠️ SÜRÜ PSİKOLOJİSİ / DAĞITIM UYARISI (CROWDED / RISK)
};

struct AlphaCandidate {
    std::string symbol;
    std::string company_name;
    std::string sector;
    std::string market; // "BIST", "XETRA", "US"
    std::string currency;
    bool is_live{false};

    double current_price{0.0};
    double vwap_30d{0.0};
    double vwap_dist_pct{0.0};
    double vwap_z_score{0.0};
    double atr_14{0.0};
    double rsi_14{50.0};
    double vol_squeeze_ratio{1.0};   // sigma_5 / sigma_20
    double return_30d_pct{0.0};
    double rel_strength_20d_pct{0.0}; // Excess return vs regional index
    double institutional_flow{0.0};  // -1.0 to +1.0 (Volume-Price Skew)
    double crowdedness_pct{0.0};     // 0% to 100%

    // Data-Driven Asymmetric Risk/Reward Profile
    double stop_loss{0.0};
    double target_price{0.0};
    double downside_risk_pct{0.0};
    double upside_potential_pct{0.0};
    double asymmetry_ratio{1.0};     // upside / downside
    double composite_alpha_score{0.0};

    // Integrated Institutional Fundamental & FAVÖK Profile
    FundamentalProfile fundamentals;

    AlphaCategory category{AlphaCategory::EarlyMomentum};
    std::string category_label;
    std::string crowdedness_bar;
    std::string quant_diagnosis;
    std::string tactical_action;
};

struct ScreeningFilterOptions {
    bool exclude_crowded{false};          // Sürü / zayıf bilanço hisselerini ele
    double min_asymmetry_ratio{1.35};     // Asgari Risk/Kazanç eşiği
    double max_crowdedness_pct{65.0};     // Azami kalabalık skoru
};

class QuantOpportunityScanner {
public:
    // Scans the target region using 6-month synchronized OHLCV + Benchmark Relative Strength + Live FAVÖK/Fundamentals
    static std::vector<AlphaCandidate> scan_market(const std::string& region,
                                                   LiveMarketDataClient& client,
                                                   const ScreeningFilterOptions& options = {}) {
        auto universe = get_universe_definitions_(region);

        // Fetch Regional Benchmark for Relative Strength calibration
        std::string bench_sym = "XU100.IS";
        if (region == "XETRA" || region == "DAX") bench_sym = "^GDAXI";
        else if (region == "US" || region == "WALLSTREET") bench_sym = "^GSPC";

        double bench_ret_20d = 0.0;
        auto bench = client.fetch_asset(bench_sym, "6mo");
        if (bench.is_live && bench.historical_prices.size() >= 21) {
            double old_b = bench.historical_prices[bench.historical_prices.size() - 21];
            if (old_b > 0.0) {
                bench_ret_20d = ((bench.current_price - old_b) / old_b) * 100.0;
            }
        }

        std::vector<std::pair<UniverseAsset, RealTimeAssetData>> raw_fetched;
        raw_fetched.reserve(universe.size());
        bool any_live = false;

        for (const auto& item : universe) {
            auto data = client.fetch_asset(item.symbol, "6mo");
            if (data.current_price <= 0.0 && !data.historical_prices.empty()) {
                data.current_price = data.historical_prices.back();
            }
            if (data.current_price <= 0.0) continue;
            if (data.is_live) any_live = true;
            raw_fetched.emplace_back(item, std::move(data));
        }

        std::vector<AlphaCandidate> candidates;
        candidates.reserve(raw_fetched.size());

        for (const auto& [item, data] : raw_fetched) {
            // Never mix simulated offline quotes into a live scan!
            if (any_live && !data.is_live) continue;

            FundamentalProfile fund = FundamentalEngine::fetch_fundamentals(client, data);
            auto candidate = evaluate_asset_(item, data, fund, bench_ret_20d);

            if (options.exclude_crowded && candidate.category == AlphaCategory::CrowdedTradeWarning) {
                continue;
            }
            if (options.exclude_crowded && candidate.fundamentals.is_available &&
                candidate.fundamentals.fundamental_score < -0.25) {
                continue;
            }
            if (candidate.asymmetry_ratio < options.min_asymmetry_ratio &&
                candidate.category != AlphaCategory::CrowdedTradeWarning) {
                continue;
            }
            if (candidate.crowdedness_pct > options.max_crowdedness_pct && options.exclude_crowded) {
                continue;
            }

            candidates.push_back(std::move(candidate));
        }

        // Rank by Category Tier then Composite Alpha (Asymmetry + Fundamental FAVÖK Quality)
        std::sort(candidates.begin(), candidates.end(), [](const AlphaCandidate& a, const AlphaCandidate& b) {
            auto cat_rank = [](AlphaCategory c) {
                switch (c) {
                    case AlphaCategory::HiddenAccumulation:  return 4;
                    case AlphaCategory::VolatilitySqueeze:   return 3;
                    case AlphaCategory::EarlyMomentum:       return 2;
                    case AlphaCategory::CrowdedTradeWarning: return 1;
                }
                return 0;
            };

            int rank_a = cat_rank(a.category);
            int rank_b = cat_rank(b.category);
            if (rank_a != rank_b) return rank_a > rank_b;
            return a.composite_alpha_score > b.composite_alpha_score;
        });

        return candidates;
    }

    static void render_screener_report(const std::vector<AlphaCandidate>& candidates,
                                       const std::string& region_name) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
        std::string report_hdr = I18n::tr(
            std::format("║  🎯 ASİMETRİK ALFA, HACİM SIKIŞMASI & FAVÖK BİLANÇO TARAMASI: {}", region_name),
            std::format("║  🎯 ASYMMETRIC ALPHA, SQUEEZE & EBITDA FUNDAMENTAL SCANNER: {}", region_name),
            std::format("║  🎯 ASYMMETRISCHER ALPHA-, SQUEEZE- & EBITDA-SCANNER: {}", region_name)
        );
        int hpad = std::max(0, 103 - ThemeEngine::utf8_visible_length(report_hdr));
        std::cout << report_hdr << std::string(static_cast<size_t>(hpad), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << pal.dim_rgb
                  << I18n::tr(
                        "  METODOLOJİ: 6-aylık senkronize OHLCV kanal destek/dirençleri, gerçek 14-günlük ATR, kurumsal hacim\n"
                        "  akümülasyonu ve kur-uyumlu TTM FAVÖK (FD/FAVÖK, Net Borç/FAVÖK, ROE) verileri birleştirilerek\n"
                        "  her varlığa özgü veri-tabanlı (sabit katsayısız) Risk/Kazanç asimetrisi hesaplanır.\n",
                        "  METHODOLOGY: Combines 6-month OHLCV structural channels, true 14-day ATR, institutional volume flow,\n"
                        "  and FX-aligned TTM EBITDA (EV/EBITDA, Net Debt/EBITDA, ROE) to compute asset-specific asymmetry.\n",
                        "  METHODIK: Kombiniert 6-Monats-OHLCV-Kanäle, echten 14-Tage-ATR, institutionellen Volumenfluss\n"
                        "  und währungsbereinigtes TTM-EBITDA (EV/EBITDA, Nettoschuld/EBITDA, ROE).\n"
                     )
                  << pal.reset_code;
        std::cout << pal.glow_rgb
                  << "╟──────────────────────────────────────────────────────────────────────────────────────────────────────╢\n"
                  << pal.reset_code;

        int idx = 1;
        int hidden_count = 0;
        int squeeze_count = 0;
        int crowded_count = 0;
        bool all_offline = !candidates.empty();

        for (const auto& c : candidates) {
            if (c.is_live) all_offline = false;
            if (c.category == AlphaCategory::HiddenAccumulation) ++hidden_count;
            else if (c.category == AlphaCategory::VolatilitySqueeze) ++squeeze_count;
            else if (c.category == AlphaCategory::CrowdedTradeWarning) ++crowded_count;
        }

        if (all_offline) {
            std::cout << pal.red_rgb
                      << "  ⚠️ UYARI: CANLI VERİ AKIŞINA ULAŞILAMADI — LÜTFEN İNTERNET BAĞLANTINIZI KONTROL EDİN.\n"
                      << pal.reset_code;
        }

        std::cout << I18n::tr(
            std::format("  📊 TARAMA ÖZETİ: Toplam {} Varlık | 💎 {} Gizli Akümülasyon | ⚡ {} Sıkışma | ⚠️ {} Risk / Sürü Uyarısı\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count),
            std::format("  📊 SCREENING SUMMARY: {} Total Assets | 💎 {} Hidden Accumulation | ⚡ {} Squeeze | ⚠️ {} Risk / Herd Warnings\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count),
            std::format("  📊 SCAN-ÜBERSICHT: {} Gesamtanlagen | 💎 {} Versteckte Akkumulation | ⚡ {} Squeeze | ⚠️ {} Risiko-/Herdenwarnungen\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count)
        );

        if (candidates.empty()) {
            std::cout << "  " << pal.dim_rgb << I18n::tr(
                "ℹ Seçilen katı filtre eşiklerini (örn. R:R ≥ 2.5 ve düşük kalabalık) anlık piyasa fiyatlamasında karşılayan hisse bulunamadı.\n"
                "    Hisselerin 60-günlük zirvelere yakın işlem gördüğü dönemlerde [1] Tüm Evreni Tara seçeneğiyle mevcut R:R sıralamasını inceleyebilirsiniz.\n\n",
                "ℹ No assets currently meet the strict filter thresholds (e.g. R:R >= 2.5 & low crowd). Use [1] Scan Full Universe to view all ranked setups.\n\n",
                "ℹ Aktuell erfüllt kein Wert die strengen Filterkriterien (z.B. R:R >= 2.5). Wählen Sie [1] Gesamtes Universum scannen.\n\n"
            ) << pal.reset_code;
            return;
        }

        for (const auto& c : candidates) {
            std::string header_color = pal.glow_rgb;
            if (c.category == AlphaCategory::HiddenAccumulation) header_color = "\033[36m";
            else if (c.category == AlphaCategory::VolatilitySqueeze) header_color = "\033[33m";
            else if (c.category == AlphaCategory::CrowdedTradeWarning) header_color = "\033[31m";
            else header_color = pal.green_rgb;

            std::cout << header_color
                      << std::format("┌─[{:>2}] {:<10} ── {} [{}] ─┐\n",
                                     idx++, c.symbol, ThemeEngine::pad_right(c.company_name, 38), c.sector)
                      << pal.reset_code;

            std::cout << std::format("│  • {:<18}: {}{}{}\n",
                                     I18n::tr("Quant Sınıfı", "Quant Class", "Quant-Klasse"),
                                     header_color, c.category_label, pal.reset_code);
            std::cout << std::format("│  • {:<18}: {:.2f} {} ({}: {:.2f} {}, {}: {:+.1f}%, Z: {:+.2f}σ, ATR14: {:.2f})\n",
                                     I18n::tr("Fiyat & VWAP", "Price & VWAP", "Kurs & VWAP"),
                                     c.current_price, c.currency,
                                     I18n::tr("22G VWAP", "22D VWAP", "22T VWAP"),
                                     c.vwap_30d, c.currency,
                                     I18n::tr("Fark", "Diff", "Diff"),
                                     c.vwap_dist_pct, c.vwap_z_score, c.atr_14);
            std::cout << std::format("│  • {:<18}: {:.2f} {}  │  RSI(14): {:.1f}  │  20G: {:+.1f}% (Endekse Göre: {:+.1f}%)\n",
                                     I18n::tr("Sıkışma (σ5/σ20)", "Squeeze (σ5/σ20)", "Squeeze (σ5/σ20)"),
                                     c.vol_squeeze_ratio,
                                     (c.vol_squeeze_ratio < 0.72 ? I18n::tr("⚡[SIKIŞMA]", "⚡[SQUEEZE]", "⚡[SQUEEZE]") : "normal"),
                                     c.rsi_14, c.return_30d_pct, c.rel_strength_20d_pct);

            // FAVÖK & Fundamental Valuation Additional Line
            std::cout << std::format("│  • {:<18}: {}\n",
                                     I18n::tr("Temel & FAVÖK", "EBITDA & Multiples", "EBITDA & Bewertung"),
                                     format_fundamental_screener_line_(c.fundamentals, pal));

            std::string crowd_status = (c.crowdedness_pct >= 65.0)
                ? I18n::tr("AŞIRI ŞİŞMİŞ / MANŞETLERDE", "OVERCROWDED / IN HEADLINES", "ÜBERFÜLLT")
                : ((c.crowdedness_pct <= 35.0)
                    ? I18n::tr("RADAR ALTI / SESSİZ", "OFF-RADAR / STEALTH", "UNTER DEM RADAR")
                    : I18n::tr("DENGELİ", "BALANCED", "AUSGEWOGEN"));

            std::cout << std::format("│  • {:<18}: {} %{:.1f} ({})\n",
                                     I18n::tr("Kalabalık Skoru", "Crowd Score", "Herden-Score"),
                                     c.crowdedness_bar, c.crowdedness_pct, crowd_status);

            std::string asym_badge = (c.asymmetry_ratio >= 2.8)
                ? I18n::tr(" [ÜSTÜN ASİMETRİ]", " [SUPERIOR ASYMMETRY]", " [HOHE ASYMMETRIE]")
                : ((c.asymmetry_ratio < 1.0)
                    ? I18n::tr(" [NEGATİF ASİMETRİ!]", " [NEGATIVE ASYMMETRY!]", " [NEGATIVE ASYMMETRIE!]") : "");

            std::cout << std::format("│  • {:<18}: {}{}1 : {:.2f}{}{} ({}: {:.2f} {}, {}: {:.2f} {})\n",
                                     I18n::tr("Asimetrik R:R", "Asymmetric R:R", "Asymmetrisches R:R"),
                                     (c.asymmetry_ratio >= 2.5 ? "\033[32m" : (c.asymmetry_ratio < 1.0 ? "\033[31m" : pal.glow_rgb)),
                                     (c.asymmetry_ratio >= 2.8 ? "💎 " : ""),
                                     c.asymmetry_ratio,
                                     asym_badge,
                                     pal.reset_code,
                                     I18n::tr("Yapısal Stop", "Struct. Stop", "Stop-Loss"),
                                     c.stop_loss, c.currency,
                                     I18n::tr("Hedef", "Target", "Ziel"),
                                     c.target_price, c.currency);

            std::cout << std::format("│  • {:<18}: {}: -%{:.1f}  ──>  {}: +%{:.1f}\n",
                                     I18n::tr("Potansiyel Oran", "Potential Ratio", "Potenzial-Verhältnis"),
                                     I18n::tr("Zarar Riski", "Downside Risk", "Verlustrisiko"),
                                     c.downside_risk_pct,
                                     I18n::tr("Kâr Potansiyeli", "Upside Potential", "Gewinnpotenzial"),
                                     c.upside_potential_pct);

            std::cout << std::format("│  • {:<18}: {}\n",
                                     I18n::tr("Akıllı Para İzi", "Smart Money Footprint", "Smart-Money-Spur"),
                                     c.quant_diagnosis);

            std::cout << std::format("│  • {:<18}: {}{}{}\n",
                                     I18n::tr("Taktiksel Eylem", "Tactical Action", "Taktische Aktion"),
                                     header_color, c.tactical_action, pal.reset_code);

            std::cout << header_color
                      << "└──────────────────────────────────────────────────────────────────────────────────────────────────────┘\n"
                      << pal.reset_code << "\n";
        }
    }

private:
    struct UniverseAsset {
        std::string symbol;
        std::string company_name;
        std::string sector;
        std::string market;
        std::string currency;
    };

    static std::string format_fundamental_screener_line_(const FundamentalProfile& f, const CrtColorPalette& pal) {
        if (f.asset_class == AssetClassType::Equity && f.is_available && f.has_ebitda) {
            std::string eb = FundamentalEngine::format_compact_money(f.ebitda_ttm_fin, f.financial_currency);
            std::string mrg = std::isfinite(f.ebitda_margin_pct) ? std::format("%{:.1f}", f.ebitda_margin_pct) : "N/A";
            std::string eveb = std::isfinite(f.ev_to_ebitda) ? std::format("{:.2f}x", f.ev_to_ebitda) : "Neg";
            std::string ndeb = "N/A";
            if (std::isfinite(f.net_debt_to_ebitda)) {
                ndeb = (f.net_debt_to_ebitda < 0.0)
                    ? std::format("{}{:.2f}x [NetNakit]{}", pal.green_rgb, f.net_debt_to_ebitda, pal.reset_code)
                    : std::format("{:.2f}x", f.net_debt_to_ebitda);
            }
            std::string pb = std::isfinite(f.pb_ratio) ? std::format("{:.2f}x", f.pb_ratio) : "N/A";
            return std::format("FAVÖK: {} (Marj {}) │ FD/FAVÖK: {}{}{} │ NetBorç/FAVÖK: {} │ PD/DD: {} │ Not: {}{}{}",
                               eb, mrg,
                               pal.glow_rgb, eveb, pal.reset_code,
                               ndeb, pb,
                               pal.glow_rgb, f.quality_grade, pal.reset_code);
        }
        return f.summary_line;
    }

    static std::vector<UniverseAsset> get_universe_definitions_(const std::string& region) {
        if (region == "XETRA" || region == "DAX") {
            return {
                {"HNR1.DE", "Hannover Rück SE",             "Küresel Reasürans & Nakit Verimi", "XETRA", "EUR"},
                {"AIXA.DE", "Aixtron SE",                   "Yarıiletken Ekipman & GaN",        "XETRA", "EUR"},
                {"NEM.DE",  "Nemetschek SE",                "Mimarlık / AEC Yazılımı",          "XETRA", "EUR"},
                {"BC8.DE",  "Bechtle AG",                   "Kurumsal IT Sistemleri",           "XETRA", "EUR"},
                {"CON.DE",  "Continental AG",               "Otomotiv & Lastik Sanayi",         "XETRA", "EUR"},
                {"MBG.DE",  "Mercedes-Benz Group",          "Otomotiv & Premium Lüks",          "XETRA", "EUR"},
                {"HEI.DE",  "Heidelberg Materials",         "Altyapı & Çimento Malzemeleri",    "XETRA", "EUR"},
                {"RHM.DE",  "Rheinmetall AG",               "Savunma & Mühimmat Lideri",        "XETRA", "EUR"},
                {"SAP.DE",  "SAP SE",                       "Kurumsal Bulut & ERP Devi",        "XETRA", "EUR"},
                {"SIE.DE",  "Siemens AG",                   "Endüstriyel Otomasyon & Enerji",   "XETRA", "EUR"}
            };
        } else if (region == "US" || region == "WALLSTREET") {
            return {
                {"PLTR", "Palantir Technologies",      "Savunma & Kurumsal AI Veri Platformu", "US", "$"},
                {"CRWD", "CrowdStrike Holdings",       "Yeni Nesil Bulut Siber Güvenlik",      "US", "$"},
                {"ANET", "Arista Networks",            "AI Veri Merkezi Yüksek Hızlı Ağ",      "US", "$"},
                {"UBER", "Uber Technologies",          "Mobilite Platformu & Serbest Nakit",   "US", "$"},
                {"ON",   "ON Semiconductor",           "Otomotiv & Güç Yarıiletkenleri",       "US", "$"},
                {"AMAT", "Applied Materials",          "Yarıiletken Üretim Ekipmanları",       "US", "$"},
                {"PANW", "Palo Alto Networks",         "Siber Güvenlik Platform Lideri",       "US", "$"},
                {"CAT",  "Caterpillar Inc.",           "Küresel Altyapı & İş Makineleri",      "US", "$"},
                {"NVDA", "NVIDIA Corporation",         "AI Hızlandırıcı & GPU Lideri",         "US", "$"},
                {"AAPL", "Apple Inc.",                 "Tüketici Ekosistemi & Hizmetler",      "US", "$"},
                {"MSFT", "Microsoft Corporation",      "Azure Bulut & Kurumsal AI",            "US", "$"},
                {"SPY",  "SPDR S&P 500 ETF Trust",     "Geniş ABD Piyasa Endeks Fonu",         "US", "$"}
            };
        } else {
            return {
                {"EREGL.IS", "Ereğli Demir ve Çelik",    "Demir-Çelik & Ağır Sanayi",             "BIST", "TL"},
                {"PGSUS.IS", "Pegasus Hava Taşımacılığı","Düşük Maliyetli Havacılık & Büyüme",    "BIST", "TL"},
                {"MGROS.IS", "Migros Ticaret A.Ş.",      "Defansif Gıda & Perakende Nakit Akışı", "BIST", "TL"},
                {"ENKAI.IS", "Enka İnşaat ve Sanayi",    "Küresel Müteahhitlik & Net Döviz Nakit","BIST", "TL"},
                {"TAVHL.IS", "TAV Havalimanları",        "Havalimanı İşletmeciliği (EUR Gelir)",  "BIST", "TL"},
                {"CCOLA.IS", "Coca-Cola İçecek",         "Bölgesel İçecek & Tüketim",             "BIST", "TL"},
                {"KCHOL.IS", "Koç Holding A.Ş.",         "Çok Sektörlü Holding",                  "BIST", "TL"},
                {"SAHOL.IS", "Sabancı Holding",          "Banka, Enerji & Sanayi Holding",        "BIST", "TL"},
                {"SISE.IS",  "Şişecam",                  "Küresel Cam & Kimya Sanayi",            "BIST", "TL"},
                {"ASTOR.IS", "Astor Enerji",             "Trafo & Yüksek Gerilim İhracatı",       "BIST", "TL"},
                {"FROTO.IS", "Ford Otosan",              "Ticari Araç & Avrupa İhracat",          "BIST", "TL"},
                {"THYAO.IS", "Türk Hava Yolları",        "Küresel Havacılık Lokomotifi",          "BIST", "TL"},
                {"ASELS.IS", "Aselsan Elektronik",       "Savunma Sanayii & İleri Elektronik",    "BIST", "TL"},
                {"TUPRS.IS", "Tüpraş Rafinerileri",      "Enerji Rafineri & Nakit Akışı",         "BIST", "TL"},
                {"BIMAS.IS", "BİM Birleşik Mağazalar",   "İndirim Perakendeciliği Lideri",        "BIST", "TL"}
            };
        }
    }

    static AlphaCandidate evaluate_asset_(const UniverseAsset& asset,
                                          const RealTimeAssetData& data,
                                          const FundamentalProfile& fund,
                                          double bench_ret_20d) {
        AlphaCandidate c;
        c.symbol = asset.symbol;
        c.company_name = asset.company_name;
        c.sector = asset.sector;
        c.market = asset.market;
        c.currency = asset.currency;
        c.is_live = data.is_live;
        c.current_price = data.current_price;
        c.fundamentals = fund;

        const auto& p = data.historical_prices;
        const auto& v = data.historical_volumes;

        // 1. 20-Trading-Day (~1 Month) Return & Benchmark Relative Strength
        if (p.size() >= 21) {
            double old_p = p[p.size() - 21];
            if (old_p > 0.0) {
                c.return_30d_pct = ((c.current_price - old_p) / old_p) * 100.0;
            }
        } else if (!p.empty() && p.front() > 0.0) {
            c.return_30d_pct = ((c.current_price - p.front()) / p.front()) * 100.0;
        }
        c.rel_strength_20d_pct = c.return_30d_pct - bench_ret_20d;

        // 2. 14-Period Wilder RSI
        c.rsi_14 = compute_rsi_(p, 14);

        // 3. Synchronized 22-Day Institutional VWAP & Z-Score
        c.vwap_30d = (data.vwap > 0.0) ? data.vwap : c.current_price;
        if (c.vwap_30d > 0.0) {
            c.vwap_dist_pct = ((c.current_price - c.vwap_30d) / c.vwap_30d) * 100.0;
        }

        if (p.size() >= 5) {
            size_t lookback = std::min(p.size(), size_t{22});
            auto p_start = p.cend() - static_cast<std::ptrdiff_t>(lookback);
            double p_mean = std::accumulate(p_start, p.cend(), 0.0) / static_cast<double>(lookback);
            double p_var = 0.0;
            for (auto it = p_start; it != p.cend(); ++it) {
                p_var += (*it - p_mean) * (*it - p_mean);
            }
            double p_std = std::sqrt(p_var / static_cast<double>(lookback));
            if (p_std > 1e-4) {
                c.vwap_z_score = (c.current_price - c.vwap_30d) / p_std;
            }
        }

        // 4. Volatility Squeeze Ratio (sigma_5 / sigma_20) & Institutional OBV Flow
        c.vol_squeeze_ratio = compute_squeeze_ratio_(p);
        c.institutional_flow = compute_flow_score_(p, v);

        // 5. Relative Crowdedness Score (calibrated against regional benchmark excess return!)
        double f_ret  = std::clamp((c.rel_strength_20d_pct - 2.0) / 24.0, 0.0, 1.0);
        double f_rsi  = std::clamp((c.rsi_14 - 55.0) / 25.0, 0.0, 1.0);
        double f_vwap = std::clamp((c.vwap_dist_pct - 1.5) / 14.0, 0.0, 1.0);
        c.crowdedness_pct = std::clamp((0.40 * f_ret + 0.35 * f_rsi + 0.25 * f_vwap) * 100.0, 0.0, 100.0);
        c.crowdedness_bar = build_gauge_bar_(c.crowdedness_pct);

        // 6. True OHLC 14-Day ATR & Structural 20d Support / 60d Channel Resistance
        double daily_vol = std::max(0.012, data.annualized_volatility / std::sqrt(252.0));
        c.atr_14 = (data.atr_14 > 0.0) ? data.atr_14 : (c.current_price * daily_vol * 1.35);

        double swing_low_20 = c.current_price - 1.5 * c.atr_14;
        if (!p.empty()) {
            size_t lb20 = std::min<size_t>(20, p.size());
            swing_low_20 = *std::min_element(p.end() - static_cast<std::ptrdiff_t>(lb20), p.end());
            if (data.historical_lows.size() == p.size()) {
                double raw_low = *std::min_element(data.historical_lows.end() - static_cast<std::ptrdiff_t>(lb20),
                                                   data.historical_lows.end());
                // Use raw low only if unsplit (close to adjusted scale)
                if (raw_low > 0.5 * swing_low_20 && raw_low <= c.current_price) {
                    swing_low_20 = std::min(swing_low_20, raw_low);
                }
            }
        }

        double swing_high_60 = c.current_price + 3.0 * c.atr_14;
        if (!p.empty()) {
            size_t lb60 = std::min<size_t>(60, p.size());
            swing_high_60 = *std::max_element(p.end() - static_cast<std::ptrdiff_t>(lb60), p.end());
        }

        // Fundamental Quality & Balance-Sheet Multiplier (FAVÖK, EV/FAVÖK, Net Debt/FAVÖK)
        double fund_score = fund.is_available ? fund.fundamental_score : 0.0;
        bool heavy_debt_warning = (fund.has_ebitda && std::isfinite(fund.net_debt_to_ebitda) && fund.net_debt_to_ebitda > 4.2) ||
                                  (fund.asset_class == AssetClassType::Equity && fund.is_available && fund.has_ebitda && f_negative_ebitda_(fund));

        double fund_headroom_mult = std::clamp(1.0 + 0.32 * fund_score - (heavy_debt_warning ? 0.25 : 0.0), 0.58, 1.38);

        // Data-Driven Stop Distance: structural 20d support buffered by 0.35 ATR, bounded within [1.05 ATR, 2.85 ATR]
        double struct_risk = (c.current_price - swing_low_20) + 0.35 * c.atr_14;
        double risk_dist = std::clamp(struct_risk,
                                      std::max(1.05 * c.atr_14, 0.022 * c.current_price),
                                      std::max(2.85 * c.atr_14, 0.115 * c.current_price));

        // Data-Driven Upside Target Distance: combines 60d channel recovery + volatility/flow expansion + FAVÖK valuation headroom
        double channel_room = std::max(0.0, swing_high_60 - c.current_price);
        double squeeze_bonus = std::clamp(1.0 - c.vol_squeeze_ratio, -0.25, 0.55);
        double flow_bonus = std::clamp(c.institutional_flow, -0.55, 0.65);
        double crowd_penalty = (c.crowdedness_pct / 100.0) * 1.45;

        double vol_projection = c.atr_14 * std::max(0.75, 2.85 + 1.55 * flow_bonus + 1.35 * squeeze_bonus - crowd_penalty);
        double raw_upside_dist = (0.40 * channel_room + 0.60 * vol_projection) * fund_headroom_mult;

        // 7. Categorization & Genuine Asset-Specific R:R
        bool is_crowded = (c.crowdedness_pct >= 64.0 || (c.rsi_14 >= 73.0 && c.rel_strength_20d_pct >= 18.0) || c.vwap_dist_pct >= 13.5);
        bool is_falling_knife = (c.institutional_flow < -0.18 && c.rel_strength_20d_pct < -5.0) ||
                                (heavy_debt_warning && c.institutional_flow < -0.05);

        if (is_crowded) {
            c.category = AlphaCategory::CrowdedTradeWarning;
            c.category_label = I18n::tr(
                "⚠️ SÜRÜ PSİKOLOJİSİ / AŞIRI UZAMIŞ FİYAT (CROWDED BETA)",
                "⚠️ HERD BEHAVIOR / OVEREXTENDED PRICE (CROWDED BETA)",
                "⚠️ HERDENTRIEB / ÜBERDEHNTER KURS (CROWDED BETA)"
            );
            // Overextended above VWAP: wide reversion risk back below VWAP, compressed remaining upside
            double rev_risk = std::max(risk_dist * 1.25, std::max(0.0, c.current_price - c.vwap_30d) + 1.1 * c.atr_14);
            double capped_up = std::max(0.65 * c.atr_14, raw_upside_dist * 0.45);
            c.stop_loss = c.current_price - rev_risk;
            c.target_price = c.current_price + capped_up;
        } else if (is_falling_knife) {
            c.category = AlphaCategory::CrowdedTradeWarning;
            c.category_label = heavy_debt_warning
                ? I18n::tr("⚠️ ZAYIF BİLANÇO / YÜKSEK BORÇ & KURUMSAL ÇIKIŞ UYARISI",
                           "⚠️ WEAK BALANCE SHEET / HIGH DEBT & DISTRIBUTION WARNING",
                           "⚠️ SCHWACHE BILANZ / HOHE VERSCHULDUNG & ABFLUSS")
                : I18n::tr("⚠️ KURUMSAL DAĞITIM / DÜŞEN BIÇAK RİSKİ (NEGATİF HACİM)",
                           "⚠️ INSTITUTIONAL DISTRIBUTION / FALLING KNIFE RISK",
                           "⚠️ INSTITUTIONELLE DISTRIBUTION / FALLENDES MESSER");
            double knife_risk = std::max(risk_dist * 1.15, 2.0 * c.atr_14);
            double muted_up = std::max(0.80 * c.atr_14, raw_upside_dist * 0.55);
            c.stop_loss = c.current_price - knife_risk;
            c.target_price = c.current_price + muted_up;
        } else if (c.vol_squeeze_ratio <= 0.72 && c.crowdedness_pct < 55.0) {
            c.category = AlphaCategory::VolatilitySqueeze;
            c.category_label = I18n::tr(
                "⚡ VOLATİLİTE SIKIŞMASI (PATLAMA ÖNCESİ SESSİZLİK)",
                "⚡ VOLATILITY SQUEEZE (CALM BEFORE THE BREAKOUT)",
                "⚡ VOLATILITÄTSKOMPRESSION (RUHE VOR DEM AUSBRUCH)"
            );
            // Tight structural coil stop, expanded breakout target
            double tight_stop = std::clamp(struct_risk, std::max(0.95 * c.atr_14, 0.020 * c.current_price), 1.65 * c.atr_14);
            double squeeze_up = std::max(tight_stop * 2.15, raw_upside_dist * 1.18);
            c.stop_loss = c.current_price - tight_stop;
            c.target_price = c.current_price + squeeze_up;
        } else if (c.institutional_flow >= 0.06 && c.crowdedness_pct < 46.0 && fund_score >= -0.20) {
            c.category = AlphaCategory::HiddenAccumulation;
            c.category_label = I18n::tr(
                "💎 GİZLİ ALFA / RADAR ALTI KURUMSAL AKÜMÜLASYON",
                "💎 HIDDEN ALPHA / OFF-RADAR INSTITUTIONAL ACCUMULATION",
                "💎 VERSTECKTER ALPHA / INSTITUTIONELLE AKKUMULATION"
            );
            double accum_stop = std::clamp(struct_risk, std::max(1.00 * c.atr_14, 0.022 * c.current_price), 1.85 * c.atr_14);
            double accum_up = std::max(accum_stop * 2.25, raw_upside_dist * 1.22);
            c.stop_loss = c.current_price - accum_stop;
            c.target_price = c.current_price + accum_up;
        } else {
            c.category = AlphaCategory::EarlyMomentum;
            c.category_label = I18n::tr(
                "📈 ERKEN MOMENTUM / DENGELİ TREND",
                "📈 EARLY MOMENTUM / BALANCED TREND",
                "📈 FRÜHES MOMENTUM / AUSGEWOGENER TREND"
            );
            c.stop_loss = c.current_price - risk_dist;
            c.target_price = c.current_price + std::max(1.35 * c.atr_14, raw_upside_dist);
        }

        c.downside_risk_pct = std::max(1.5, ((c.current_price - c.stop_loss) / c.current_price) * 100.0);
        c.upside_potential_pct = std::max(1.0, ((c.target_price - c.current_price) / c.current_price) * 100.0);
        c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct;
        c.composite_alpha_score = c.asymmetry_ratio + (0.85 * fund_score) + (0.45 * c.institutional_flow);

        // Build Rich Diagnosis & Tactical Action referencing actual flow + FAVÖK quality
        std::string fund_tag = fund.is_available
            ? std::format(" [Temel Not: {} ({:+.2f})]", fund.quality_grade, fund.fundamental_score)
            : "";

        if (c.category == AlphaCategory::CrowdedTradeWarning) {
            if (is_crowded) {
                c.quant_diagnosis = I18n::tr(
                    std::format("22G VWAP'ın %{:+.1f} üzerinde ve endekse göre %{:+.1f} ayrışmış durumda; kısa vadeli kâr realizasyonu riski yüksek{}.",
                                c.vwap_dist_pct, c.rel_strength_20d_pct, fund_tag),
                    std::format("Trading {:+.1f}% above 22D VWAP with {:+.1f}% excess run; elevated profit-taking risk{}.",
                                c.vwap_dist_pct, c.rel_strength_20d_pct, fund_tag),
                    std::format("Notiert {:+.1f}% über 22T-VWAP; erhöhtes Gewinnmitnahmerisiko{}.",
                                c.vwap_dist_pct, fund_tag)
                );
                c.tactical_action = I18n::tr(
                    std::format("ZİRVEDEN KOVALAMA — {:.2f} {} VWAP bölgesine geri çekilme bekle (R:R 1:{:.2f}).",
                                c.vwap_30d, c.currency, c.asymmetry_ratio),
                    std::format("DO NOT CHASE — await pullback toward {:.2f} {} VWAP (R:R 1:{:.2f}).",
                                c.vwap_30d, c.currency, c.asymmetry_ratio),
                    std::format("NICHT HINTERHERLAUFEN — Rücksetzer Richtung {:.2f} {} abwarten.",
                                c.vwap_30d, c.currency)
                );
            } else {
                c.quant_diagnosis = I18n::tr(
                    std::format("Hacim akışı negatif ({:+.2f}){} — satış baskısı durulmadan dip yakalamak risklidir.",
                                c.institutional_flow, fund_tag),
                    std::format("Negative institutional volume flow ({:+.2f}){} — catching unconfirmed bottoms is risky.",
                                c.institutional_flow, fund_tag),
                    std::format("Negativer Volumenfluss ({:+.2f}){} — Bodenbildung abwarten.",
                                c.institutional_flow, fund_tag)
                );
                c.tactical_action = I18n::tr(
                    "İZLEMEDE KAL — Hacim akışı pozitife dönene ve 22G VWAP geri alınana kadar bekle.",
                    "STAY ON SIDELINES — await positive volume flow and 22D VWAP reclaim.",
                    "ABWARTEN — positiven Volumenfluss und 22T-VWAP-Rückeroberung abwarten."
                );
            }
        } else if (c.category == AlphaCategory::VolatilitySqueeze) {
            c.quant_diagnosis = I18n::tr(
                std::format("5G/20G volatilite oranı %{:.0f} seviyesine sıkıştı (Hacim Akışı: {:+.2f}){}. Dar yapısal stop ile patlama potansiyeli taşıyor.",
                            c.vol_squeeze_ratio * 100.0, c.institutional_flow, fund_tag),
                std::format("5D/20D volatility compressed to {:.0f}% (Flow: {:+.2f}){}. Coiled range with tight structural stop.",
                            c.vol_squeeze_ratio * 100.0, c.institutional_flow, fund_tag),
                std::format("5T/20T-Volatilität auf {:.0f}% komprimiert (Fluss: {:+.2f}){}.",
                            c.vol_squeeze_ratio * 100.0, c.institutional_flow, fund_tag)
            );
            c.tactical_action = I18n::tr(
                std::format("SESSİZ SIKIŞMA BİRİKİMİ — {:.2f} {} yapısal stop ile 1:{:.2f} asimetrili pozisyonlanma.",
                            c.stop_loss, c.currency, c.asymmetry_ratio),
                std::format("PRE-BREAKOUT ENTRY — {:.2f} {} structural stop for 1:{:.2f} asymmetry.",
                            c.stop_loss, c.currency, c.asymmetry_ratio),
                std::format("VOR-AUSBRUCH-EINSTIEG — Stop bei {:.2f} {} (1:{:.2f}).",
                            c.stop_loss, c.currency, c.asymmetry_ratio)
            );
        } else if (c.category == AlphaCategory::HiddenAccumulation) {
            c.quant_diagnosis = I18n::tr(
                std::format("Düşük kalabalık (%{:.0f}) altında pozitif kurumsal hacim toplama ({:+.2f}) ve sağlam bilanço yapısı birleşiyor{}.",
                            c.crowdedness_pct, c.institutional_flow, fund_tag),
                std::format("Low crowd score ({:.0f}%) paired with net institutional accumulation ({:+.2f}) and solid fundamentals{}.",
                            c.crowdedness_pct, c.institutional_flow, fund_tag),
                std::format("Geringer Herden-Score ({:.0f}%) mit institutioneller Akkumulation ({:+.2f}){}.",
                            c.crowdedness_pct, c.institutional_flow, fund_tag)
            );
            c.tactical_action = I18n::tr(
                std::format("RADAR ALTI TOPLAMA — {:.2f} {} yapısal destek stopuyla {:.2f} {} hedefi (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio),
                std::format("STEALTH ACCUMULATION — {:.2f} {} structural stop targeting {:.2f} {} (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio),
                std::format("AKKUMULATION — Stop {:.2f} {}, Ziel {:.2f} {} (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio)
            );
        } else {
            c.quant_diagnosis = I18n::tr(
                std::format("Fiyat-VWAP dengesi (%{:+.1f}) ve kurumsal akış ({:+.2f}) yapıcı seyrediyor{}.",
                            c.vwap_dist_pct, c.institutional_flow, fund_tag),
                std::format("Constructive price-VWAP balance ({:+.1f}%) and volume flow ({:+.2f}){}.",
                            c.vwap_dist_pct, c.institutional_flow, fund_tag),
                std::format("Konstruktive Kurs-VWAP-Balance ({:+.1f}%) und Volumenfluss ({:+.2f}){}.",
                            c.vwap_dist_pct, c.institutional_flow, fund_tag)
            );
            c.tactical_action = I18n::tr(
                std::format("KADEMELİ TREND TAKİBİ — {:.2f} {} stop, {:.2f} {} hedef (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio),
                std::format("GRADUAL TREND ENTRY — {:.2f} {} stop, {:.2f} {} target (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio),
                std::format("TREND-EINSTIEG — Stop {:.2f} {}, Ziel {:.2f} {} (1:{:.2f} R:R).",
                            c.stop_loss, c.currency, c.target_price, c.currency, c.asymmetry_ratio)
            );
        }

        return c;
    }

    static bool f_negative_ebitda_(const FundamentalProfile& f) {
        return f.has_ebitda && f.ebitda_ttm_fin <= 0.0;
    }

    static double compute_rsi_(const std::vector<double>& prices, int period = 14) {
        if (prices.size() <= static_cast<size_t>(period)) return 50.0;

        double gains = 0.0;
        double losses = 0.0;
        for (size_t i = 1; i <= static_cast<size_t>(period); ++i) {
            double change = prices[i] - prices[i - 1];
            if (change > 0.0) gains += change;
            else losses -= change;
        }

        double avg_gain = gains / period;
        double avg_loss = losses / period;
        for (size_t i = period + 1; i < prices.size(); ++i) {
            double change = prices[i] - prices[i - 1];
            double g = (change > 0.0) ? change : 0.0;
            double l = (change < 0.0) ? -change : 0.0;
            avg_gain = (avg_gain * (period - 1) + g) / period;
            avg_loss = (avg_loss * (period - 1) + l) / period;
        }

        if (avg_loss < 1e-6) return 100.0;
        double rs = avg_gain / avg_loss;
        return 100.0 - (100.0 / (1.0 + rs));
    }

    static double compute_squeeze_ratio_(const std::vector<double>& prices) {
        if (prices.size() < 22) return 1.0;

        std::vector<double> ret5;
        std::vector<double> ret20;
        size_t n = prices.size();
        for (size_t i = n - 5; i < n; ++i) {
            if (prices[i - 1] > 0.0) ret5.push_back(std::log(prices[i] / prices[i - 1]));
        }
        for (size_t i = n - 20; i < n; ++i) {
            if (prices[i - 1] > 0.0) ret20.push_back(std::log(prices[i] / prices[i - 1]));
        }

        auto calc_std = [](const std::vector<double>& v) {
            if (v.size() < 2) return 0.02;
            double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
            double var = 0.0;
            for (double x : v) var += (x - m) * (x - m);
            return std::sqrt(var / (v.size() - 1));
        };

        double std5 = calc_std(ret5);
        double std20 = calc_std(ret20);
        if (std20 < 1e-5) return 1.0;
        return std5 / std20;
    }

    static double compute_flow_score_(const std::vector<double>& prices, const std::vector<double>& volumes) {
        if (prices.size() < 5 || volumes.size() < 5) return 0.0;

        size_t lookback = std::min({prices.size() - 1, volumes.size() - 1, size_t{20}});
        size_t n = prices.size();
        double up_vol = 0.0;
        double down_vol = 0.0;

        for (size_t i = n - lookback; i < n; ++i) {
            double diff = prices[i] - prices[i - 1];
            double v = (i < volumes.size() && volumes[i] > 0.0) ? volumes[i] : 1.0;
            if (diff > 0.0) up_vol += v;
            else if (diff < 0.0) down_vol += v;
        }

        double total = up_vol + down_vol;
        if (total < 1e-4) return 0.0;
        return (up_vol - down_vol) / total;
    }

    static std::string build_gauge_bar_(double pct) {
        int filled = static_cast<int>(std::round((pct / 100.0) * 10.0));
        filled = std::clamp(filled, 0, 10);
        std::string bar = "[";
        for (int i = 0; i < filled; ++i) bar += "█";
        for (int i = filled; i < 10; ++i) bar += "─";
        bar += "]";
        return bar;
    }
};

} // namespace quant
