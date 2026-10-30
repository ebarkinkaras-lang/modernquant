#pragma once

#include "theme_engine.hpp"
#include "live_market_data.hpp"
#include "news_sentiment.hpp"
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
    CrowdedTradeWarning   // ⚠️ SÜRÜ PSİKOLOJİSİ / GEÇ KALINMIŞ TİCARET (CROWDED BETA)
};

struct AlphaCandidate {
    std::string symbol;
    std::string company_name;
    std::string sector;
    std::string market; // "BIST", "XETRA", "US"
    std::string currency;

    double current_price{0.0};
    double vwap_30d{0.0};
    double vwap_dist_pct{0.0};
    double vwap_z_score{0.0};
    double rsi_14{50.0};
    double vol_squeeze_ratio{1.0}; // sigma_5 / sigma_20
    double return_30d_pct{0.0};
    double institutional_flow{0.0}; // -1.0 to +1.0 (Volume-Price Skew)
    double crowdedness_pct{0.0};   // 0% (Radar altı / sessiz) to 100% (Aşırı şişmiş / sürü)

    // Asymmetric Risk/Reward Profile
    double stop_loss{0.0};
    double target_price{0.0};
    double downside_risk_pct{0.0};
    double upside_potential_pct{0.0};
    double asymmetry_ratio{1.0}; // upside / downside

    AlphaCategory category{AlphaCategory::EarlyMomentum};
    std::string category_label;
    std::string crowdedness_bar;
    std::string quant_diagnosis;
    std::string tactical_action;
};

struct ScreeningFilterOptions {
    bool exclude_crowded{false};          // Sürü hisselerini ele
    double min_asymmetry_ratio{2.5};      // En az 1:2.5 Risk/Kazanç
    double max_crowdedness_pct{65.0};     // Azami %65 kalabalık skoru
};

class QuantOpportunityScanner {
public:
    // Core Screener: Scans the target region and ranks assets by mathematical asymmetry & hidden alpha
    static std::vector<AlphaCandidate> scan_market(const std::string& region,
                                                   LiveMarketDataClient& client,
                                                   const ScreeningFilterOptions& options = {}) {
        auto universe = get_universe_definitions_(region);
        std::vector<AlphaCandidate> candidates;
        candidates.reserve(universe.size());

        for (const auto& item : universe) {
            auto data = client.fetch_asset(item.symbol, "3mo");
            if (data.current_price <= 0.0 && !data.historical_prices.empty()) {
                data.current_price = data.historical_prices.back();
            }
            if (data.current_price <= 0.0) continue;

            auto candidate = evaluate_asset_(item, data);
            
            // Apply filtering options if requested
            if (options.exclude_crowded && candidate.category == AlphaCategory::CrowdedTradeWarning) {
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

        // Quant Ranking Order:
        // 1. Hidden Accumulation & Volatility Squeeze (Highest Asymmetry first)
        // 2. Early Momentum
        // 3. Crowded Trade Warnings placed at the bottom as explicit cautionary case studies
        std::sort(candidates.begin(), candidates.end(), [](const AlphaCandidate& a, const AlphaCandidate& b) {
            auto cat_rank = [](AlphaCategory c) {
                switch (c) {
                    case AlphaCategory::HiddenAccumulation: return 4;
                    case AlphaCategory::VolatilitySqueeze:  return 3;
                    case AlphaCategory::EarlyMomentum:       return 2;
                    case AlphaCategory::CrowdedTradeWarning: return 1;
                }
                return 0;
            };

            int rank_a = cat_rank(a.category);
            int rank_b = cat_rank(b.category);

            if (rank_a != rank_b) return rank_a > rank_b;
            return a.asymmetry_ratio > b.asymmetry_ratio;
        });

        return candidates;
    }

    // Beautiful 80s CRT Terminal Visualization
    static void render_screener_report(const std::vector<AlphaCandidate>& candidates,
                                       const std::string& region_name) {
        auto& te = ThemeEngine::instance();
        const auto& pal = te.palette();

        std::cout << "\n" << pal.glow_rgb;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
        std::string report_hdr = I18n::tr(
            std::format("║  🎯 ASİMETRİK ALFA & RADAR-ALTI FIRSAT TARAMA RAPORU: {:<20} (1980s QUANT)   ║", region_name),
            std::format("║  🎯 ASYMMETRIC ALPHA & OFF-RADAR OPPORTUNITY SCAN REPORT: {:<16} (1980s QUANT)   ║", region_name),
            std::format("║  🎯 ASYMMETRISCHER ALPHA & UNTER-DEM-RADAR SCAN-REPORT: {:<17} (1980s QUANT)   ║", region_name)
        );
        std::cout << report_hdr << "\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << pal.reset_code;
        std::cout << pal.dim_rgb
                  << I18n::tr(
                        "  FELSEFE: Çoğunluğun konuştuğu hisseler (Crowded Beta) çoktan fiyatlanmış olup kâr/zarar asimetrisi\n"
                        "  aleyhinizedir. Bu motor; düşük kalabalık skoru, hacim akümülasyonu ve volatilite sıkışması\n"
                        "  sayesinde 'manşetlere düşmeden önceki' asimetrik patlama adaylarını tespit eder.\n",
                        "  PHILOSOPHY: Widely discussed consensus stocks (Crowded Beta) are already priced in, leaving\n"
                        "  risk/reward skewed against you. This engine detects asymmetric breakout setups prior to\n"
                        "  mainstream headlines via low crowdedness, volume accumulation, and volatility squeezes.\n",
                        "  PHILOSOPHIE: Von der Masse diskutierte Konsensaktien (Crowded Beta) sind bereits eingepreist;\n"
                        "  das Chance/Risiko-Profil steht gegen Sie. Diese Engine erkennt asymmetrische Ausbruchskandidaten\n"
                        "  vor den Schlagzeilen durch geringe Überfüllung, Volumenakkumulation und Volatilitätssqueeze.\n"
                     )
                  << pal.reset_code;
        std::cout << pal.glow_rgb
                  << "╟──────────────────────────────────────────────────────────────────────────────────────────────────────╢\n"
                  << pal.reset_code;

        int idx = 1;
        int hidden_count = 0;
        int squeeze_count = 0;
        int crowded_count = 0;

        for (const auto& c : candidates) {
            if (c.category == AlphaCategory::HiddenAccumulation) ++hidden_count;
            else if (c.category == AlphaCategory::VolatilitySqueeze) ++squeeze_count;
            else if (c.category == AlphaCategory::CrowdedTradeWarning) ++crowded_count;
        }

        std::cout << I18n::tr(
            std::format("  📊 TARAMA ÖZETİ: Toplam {} Varlık | 💎 {} Gizli Akümülasyon | ⚡ {} Sıkışma | ⚠️ {} Aşırı Şişmiş Sürü Hissesi\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count),
            std::format("  📊 SCREENING SUMMARY: {} Total Assets | 💎 {} Hidden Accumulation | ⚡ {} Squeeze | ⚠️ {} Overcrowded Herd Stocks\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count),
            std::format("  📊 SCAN-ÜBERSICHT: {} Gesamtanlagen | 💎 {} Versteckte Akkumulation | ⚡ {} Squeeze | ⚠️ {} Überfüllte Herdenaktien\n\n",
                        candidates.size(), hidden_count, squeeze_count, crowded_count)
        );

        for (const auto& c : candidates) {
            std::string header_color = pal.glow_rgb;
            if (c.category == AlphaCategory::HiddenAccumulation) header_color = "\033[36m"; // Cyan
            else if (c.category == AlphaCategory::VolatilitySqueeze) header_color = "\033[33m"; // Amber/Yellow
            else if (c.category == AlphaCategory::CrowdedTradeWarning) header_color = "\033[31m"; // Red
            else header_color = pal.green_rgb;

            std::cout << header_color
                      << std::format("┌─[{:>2}] {:<10} ── {:<42} [{}] ─┐\n",
                                     idx++, c.symbol, c.company_name, c.sector)
                      << pal.reset_code;

            std::cout << std::format("│  • {:<18}: {}{}{}\n",
                                     I18n::tr("Quant Sınıfı", "Quant Class", "Quant-Klasse"),
                                     header_color, c.category_label, pal.reset_code);
            std::cout << std::format("│  • {:<18}: {:.2f} {} ({}: {:.2f} {}, {}: {:+.1f}%, Z: {:+.2f}σ)\n",
                                     I18n::tr("Fiyat & VWAP", "Price & VWAP", "Kurs & VWAP"),
                                     c.current_price, c.currency,
                                     I18n::tr("30G VWAP", "30D VWAP", "30T VWAP"),
                                     c.vwap_30d, c.currency,
                                     I18n::tr("Fark", "Diff", "Diff"),
                                     c.vwap_dist_pct, c.vwap_z_score);
            std::cout << std::format("│  • {:<18}: {:.2f} {}  |  14-{}: {:.1f}  |  30-{} {}: {:+.1f}%\n",
                                     I18n::tr("Sıkışma (σ5/σ20)", "Squeeze (σ5/σ20)", "Squeeze (σ5/σ20)"),
                                     c.vol_squeeze_ratio, (c.vol_squeeze_ratio < 0.70 ? I18n::tr("⚡ [YÜKSEK SIKIŞMA]", "⚡ [HIGH SQUEEZE]", "⚡ [HOHER SQUEEZE]") : "normal"),
                                     I18n::tr("G RSI", "D RSI", "T RSI"), c.rsi_14,
                                     I18n::tr("G", "D", "T"),
                                     I18n::tr("Getiri", "Return", "Rendite"), c.return_30d_pct);
            
            std::string crowd_status = (c.crowdedness_pct >= 65.0) ?
                I18n::tr("AŞIRI ŞİŞMİŞ / MANŞETLERDE", "OVERCROWDED / IN HEADLINES", "ÜBERFÜLLT / IN SCHLAGZEILEN") :
                ((c.crowdedness_pct <= 35.0) ?
                    I18n::tr("RADAR ALTI / SESSİZ", "OFF-RADAR / STEALTH", "UNTER DEM RADAR / RUHIG") :
                    I18n::tr("DENGELİ", "BALANCED", "AUSGEWOGEN"));

            std::cout << std::format("│  • {:<18}: {} %{:.1f} ({})\n",
                                     I18n::tr("Kalabalık Skoru", "Crowd Score", "Herden-Score"),
                                     c.crowdedness_bar, c.crowdedness_pct, crowd_status);

            std::string asym_badge = (c.asymmetry_ratio >= 3.0) ?
                I18n::tr(" [ÜSTÜN ASİMETRİ]", " [SUPERIOR ASYMMETRY]", " [HOHE ASYMMETRIE]") :
                ((c.asymmetry_ratio < 1.0) ?
                    I18n::tr(" [NEGATİF ASİMETRİ!]", " [NEGATIVE ASYMMETRY!]", " [NEGATIVE ASYMMETRIE!]") : "");

            std::cout << std::format("│  • {:<18}: {}{}1 : {:.1f}{}{} ({}: {:.2f} {}, {}: {:.2f} {})\n",
                                     I18n::tr("Asimetrik R:R", "Asymmetric R:R", "Asymmetrisches R:R"),
                                     (c.asymmetry_ratio >= 3.0 ? "\033[32m" : (c.asymmetry_ratio < 1.0 ? "\033[31m" : pal.glow_rgb)),
                                     (c.asymmetry_ratio >= 3.0 ? "💎 " : ""),
                                     c.asymmetry_ratio,
                                     asym_badge,
                                     pal.reset_code,
                                     I18n::tr("Zarar Kes", "Stop-Loss", "Stop-Loss"),
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

    static std::vector<UniverseAsset> get_universe_definitions_(const std::string& region) {
        if (region == "XETRA" || region == "DAX") {
            return {
                {"HNR.DE",  "Hensoldt AG",                 "Savunma & Radar Sensörleri",   "XETRA", "EUR"},
                {"AIXA.DE", "Aixtron SE",                  "Yarıiletken Ekipman & GaN",    "XETRA", "EUR"},
                {"NEM.DE",  "Nemetschek SE",               "Mimarlık / AEC Yazılımı",      "XETRA", "EUR"},
                {"BC8.DE",  "Bechtle AG",                  "Kurumsal IT Sistemleri",       "XETRA", "EUR"},
                {"CON.DE",  "Continental AG",              "Otomotiv & Lastik Sanayi",     "XETRA", "EUR"},
                {"MBG.DE",  "Mercedes-Benz Group",         "Otomotiv & Premium Lüks",      "XETRA", "EUR"},
                {"HEI.DE",  "Heidelberg Materials",        "Altyapı & Çimento Malzemeleri","XETRA", "EUR"},
                // Consensus Mega-caps to illustrate crowded trade detection
                {"RHM.DE",  "Rheinmetall AG",              "Savunma & Mühimmat Lideri",    "XETRA", "EUR"},
                {"SAP.DE",  "SAP SE",                      "Kurumsal Bulut & ERP Devi",    "XETRA", "EUR"},
                {"SIE.DE",  "Siemens AG",                  "Endüstriyel Otomasyon & Enerji","XETRA","EUR"}
            };
        } else if (region == "US" || region == "WALLSTREET") {
            return {
                {"PLTR", "Palantir Technologies",      "Savunma & Kurumsal AI Veri Platformu", "US", "USD"},
                {"CRWD", "CrowdStrike Holdings",        "Yeni Nesil Bulut Siber Güvenlik",      "US", "USD"},
                {"ANET", "Arista Networks",             "AI Veri Merkezi Yüksek Hızlı Ağ",     "US", "USD"},
                {"UBER", "Uber Technologies",           "Mobilite Platformu & Serbest Nakit",   "US", "USD"},
                {"ON",   "ON Semiconductor",            "Otomotiv & Güç Yarıiletkenleri",       "US", "USD"},
                {"CELH", "Celsius Holdings",            "Hızlı Büyüyen Enerji İçeceği",         "US", "USD"},
                {"ENPH", "Enphase Energy",              "Güneş Enerjisi Mikroinverter (Dip)",   "US", "USD"},
                {"PANW", "Palo Alto Networks",          "Siber Güvenlik Platform Lideri",       "US", "USD"},
                // Consensus Mega-caps
                {"NVDA", "NVIDIA Corporation",          "AI Hızlandırıcı & GPU Tekeli",         "US", "USD"},
                {"AAPL", "Apple Inc.",                  "Tüketici Ekosistemi & Hizmetler",      "US", "USD"},
                {"MSFT", "Microsoft Corporation",       "Azure Bulut & Kurumsal AI",            "US", "USD"},
                {"SPY",  "SPDR S&P 500 ETF Trust",      "Geniş ABD Piyasa Endeks Fonu",         "US", "USD"}
            };
        } else {
            // Default: BIST 100 & Mid-caps
            return {
                {"EREGL.IS", "Ereğli Demir ve Çelik",   "Demir-Çelik & Ağır Sanayi",            "BIST", "TL"},
                {"PGSUS.IS", "Pegasus Hava Taşımacılığı","Düşük Maliyetli Havacılık & Büyüme",   "BIST", "TL"},
                {"MGROS.IS", "Migros Ticaret A.Ş.",     "Defansif Gıda & Hızlı Tüketim",        "BIST", "TL"},
                {"ENKAI.IS", "Enka İnşaat ve Sanayi",   "Küresel Müteahhitlik & Döviz Rezervi", "BIST", "TL"},
                {"TAVHL.IS", "TAV Havalimanları",       "Havalimanı İşletmeciliği (EUR Gelir)", "BIST", "TL"},
                {"CCOLA.IS", "Coca-Cola İçecek",        "Orta Asya/Doğu Hızlı Tüketim",         "BIST", "TL"},
                {"KCHOL.IS", "Koç Holding A.Ş.",        "Çok Sektörlü Holding (Net İskonto)",   "BIST", "TL"},
                {"SAHOL.IS", "Sabancı Holding",         "Banka & Yeşil Enerji İskontosu",       "BIST", "TL"},
                {"SISE.IS",  "Şişecam (Şişe ve Cam)",   "Küresel Cam & Soda Sanayi",            "BIST", "TL"},
                {"ALARK.IS", "Alarko Holding",          "Enerji & Tarım Yatırımları",           "BIST", "TL"},
                {"ASTOR.IS", "Astor Enerji",            "Trafo & Elektrik Şebeke İhracatı",     "BIST", "TL"},
                {"FROTO.IS", "Ford Otosan",             "Ticari Araç İhracat Şampiyonu",        "BIST", "TL"},
                // Consensus Mega-caps
                {"THYAO.IS", "Türk Hava Yolları",       "Küresel Havacılık Lokomotifi",         "BIST", "TL"},
                {"ASELS.IS", "Aselsan Elektronik",      "Milli Savunma & İleri Elektronik",     "BIST", "TL"},
                {"TUPRS.IS", "Tüpraş Rafinerileri",     "Akdeniz Rafineri Marjı & Temettü",     "BIST", "TL"},
                {"BIMAS.IS", "BİM Birleşik Mağazalar",  "İndirim Perakendeciliği & Nakit Akışı","BIST", "TL"}
            };
        }
    }

    static AlphaCandidate evaluate_asset_(const UniverseAsset& asset, const RealTimeAssetData& data) {
        AlphaCandidate c;
        c.symbol = asset.symbol;
        c.company_name = asset.company_name;
        c.sector = asset.sector;
        c.market = asset.market;
        c.currency = asset.currency;
        c.current_price = data.current_price;

        const auto& p = data.historical_prices;
        const auto& v = data.historical_volumes;

        // 1. Calculate 30-Day Return
        if (p.size() >= 20) {
            double old_p = p[p.size() - 20];
            if (old_p > 0.0) {
                c.return_30d_pct = ((c.current_price - old_p) / old_p) * 100.0;
            }
        } else if (!p.empty() && p.front() > 0.0) {
            c.return_30d_pct = ((c.current_price - p.front()) / p.front()) * 100.0;
        }

        // 2. 14-Period RSI
        c.rsi_14 = compute_rsi_(p, 14);

        // 3. 30-Day VWAP and Z-Score
        c.vwap_30d = (data.vwap > 0.0) ? data.vwap : c.current_price;
        if (c.vwap_30d > 0.0) {
            c.vwap_dist_pct = ((c.current_price - c.vwap_30d) / c.vwap_30d) * 100.0;
        }

        // Price standard deviation for 30-day (22-trading-day) Z-score
        double p_mean = 0.0;
        if (!p.empty()) {
            size_t lookback = std::min(p.size(), size_t{22});
            auto p_start = p.cend() - static_cast<std::ptrdiff_t>(lookback);
            p_mean = std::accumulate(p_start, p.cend(), 0.0) / static_cast<double>(lookback);
            double p_var = 0.0;
            for (auto it = p_start; it != p.cend(); ++it) {
                p_var += (*it - p_mean) * (*it - p_mean);
            }
            double p_std = std::sqrt(p_var / static_cast<double>(lookback));
            if (p_std > 1e-4) {
                c.vwap_z_score = (c.current_price - c.vwap_30d) / p_std;
            }
        }

        // 4. Volatility Squeeze Ratio (sigma_5 / sigma_20)
        c.vol_squeeze_ratio = compute_squeeze_ratio_(p);

        // 5. Institutional Flow (On-Balance Volume Skew over last 15 bars)
        c.institutional_flow = compute_flow_score_(p, v);

        // 6. Crowdedness Score (0.0 to 100.0%)
        // How much of the move is already finished and crowded by retail FOMO?
        double f_ret = std::clamp((c.return_30d_pct - 3.0) / 32.0, 0.0, 1.0);
        double f_rsi = std::clamp((c.rsi_14 - 55.0) / 25.0, 0.0, 1.0);
        double f_vwap = std::clamp((c.vwap_dist_pct - 1.5) / 16.0, 0.0, 1.0);

        c.crowdedness_pct = (0.40 * f_ret + 0.35 * f_rsi + 0.25 * f_vwap) * 100.0;
        c.crowdedness_pct = std::clamp(c.crowdedness_pct, 0.0, 100.0);
        c.crowdedness_bar = build_gauge_bar_(c.crowdedness_pct);

        // 7. Average Daily Volatility and ATR Estimation
        double daily_vol = std::max(0.015, data.annualized_volatility / std::sqrt(252.0));
        double atr_est = c.current_price * daily_vol * 1.5;

        // 8. Categorization & Asymmetry Calculation
        bool is_crowded = (c.crowdedness_pct >= 62.0 || (c.rsi_14 >= 72.0 && c.return_30d_pct >= 28.0) || c.vwap_dist_pct >= 14.0);
        bool is_falling_knife = (c.institutional_flow < -0.12 && c.return_30d_pct < -3.0);

        if (is_crowded) {
            c.category = AlphaCategory::CrowdedTradeWarning;
            c.category_label = I18n::tr(
                "⚠️ SÜRÜ PSİKOLOJİSİ / GEÇ KALINMIŞ TİCARET (CROWDED BETA)",
                "⚠️ HERD BEHAVIOR / OVERCROWDED TRADE (CROWDED BETA)",
                "⚠️ HERDENTRIEB / ÜBERLAUFENE POSITION (CROWDED BETA)"
            );
            c.stop_loss = c.current_price * 0.86; // -14% stop loss
            c.target_price = c.current_price * 1.05; // +5% remaining target
            c.downside_risk_pct = 14.0;
            c.upside_potential_pct = 5.0;
            c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct;
            c.quant_diagnosis = I18n::tr(
                "Hisse zaten kamuoyunda manşetlere çıktı; kitleler akın etti. Zirvede kurumsal akıllı para kâr realize etmekte ve malı geç gelen bireysele devretmektedir.",
                "Asset has already hit mainstream headlines; retail crowd has piled in. Smart institutional money is taking profits and distributing into retail FOMO at local peaks.",
                "Aktie ist bereits in den Schlagzeilen; die breite Masse ist eingestiegen. Institutionelles Smart Money realisiert Gewinne und verteilt Positionen am Hoch an Kleinanleger."
            );
            c.tactical_action = I18n::tr(
                "ZİRVEDEN ALMA / DÜZELTME BEKLE (FOMO TUZAĞI). Kâr/Zarar asimetrisi aleyhinizedir (1 risk için sadece 0.35 getiri).",
                "AVOID BUYING AT PEAK / AWAIT PULLBACK (FOMO TRAP). Risk/reward asymmetry is unfavorable (only ~0.35 reward per 1 unit of risk).",
                "NICHT AM HOCH KAUFEN / KORREKTUR ABWARTEN (FOMO-FALLE). Asymmetrie ungünstig (nur ca. 0,35 Ertrag pro 1 Einheit Risiko)."
            );
        } else if (is_falling_knife) {
            c.category = AlphaCategory::CrowdedTradeWarning;
            c.category_label = I18n::tr(
                "⚠️ KURUMSAL ÇIKIŞ / DÜŞEN BIÇAK RİSKİ (NEGATİF HACİM AKIŞI)",
                "⚠️ INSTITUTIONAL EXODUS / FALLING KNIFE RISK (NEGATIVE VOLUME FLOW)",
                "⚠️ INSTITUTIONELLER ABFLUSS / FALLENDES MESSER (NEGATIVER VOLUMENFLUSS)"
            );
            c.stop_loss = c.current_price * 0.90;
            c.target_price = c.current_price * 1.07;
            c.downside_risk_pct = 10.0;
            c.upside_potential_pct = 7.0;
            c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct; // 0.70
            c.quant_diagnosis = I18n::tr(
                std::format("Fiyat ucuzlamış görünse de hacim akışı net kurumsal çıkışı gösteriyor ({:+.2f}). Satış baskısı henüz durmadı; erken giriş düşen bıçak riski taşır.", c.institutional_flow),
                std::format("Even though price seems discounted, volume flow reveals net institutional distribution ({:+.2f}). Selling pressure persists; early entry poses falling knife risk.", c.institutional_flow),
                std::format("Obwohl der Kurs optisch günstig wirkt, zeigt der Volumenfluss institutionelle Abflüsse ({:+.2f}). Verkaufsdruck hält an; verfrühtes Greifen ins fallende Messer.", c.institutional_flow)
            );
            c.tactical_action = I18n::tr(
                "İZLEMEDE KAL / TABAN TEYİDİ BEKLE. Kurumsal hacim akışı pozitife (+0.05 üstüne) dönmeden alım yapma.",
                "STAY ON SIDELINES / AWAIT BASE CONFIRMATION. Do not initiate longs until institutional volume flow turns positive (> +0.05).",
                "AN DER SEITENLINIE BLEIBEN / BODENBESTÄTIGUNG ABWARTEN. Keine Käufe, bevor der Volumenfluss nicht positiv wird (> +0,05)."
            );
        } else if (c.vol_squeeze_ratio <= 0.68 && c.crowdedness_pct < 55.0) {
            c.category = AlphaCategory::VolatilitySqueeze;
            c.category_label = I18n::tr(
                "⚡ VOLATİLİTE SIKIŞMASI (PATLAMA ÖNCESİ SESSİZLİK)",
                "⚡ VOLATILITY SQUEEZE (CALM BEFORE THE BREAKOUT)",
                "⚡ VOLATILITÄTSKOMPRESSION (RUHE VOR DEM AUSBRUCH)"
            );
            c.stop_loss = c.current_price - (atr_est * 1.1);
            c.target_price = c.current_price + (atr_est * 4.2);
            c.downside_risk_pct = std::max(2.5, ((c.current_price - c.stop_loss) / c.current_price) * 100.0);
            c.upside_potential_pct = ((c.target_price - c.current_price) / c.current_price) * 100.0;
            c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct; // ~3.8x
            c.quant_diagnosis = I18n::tr(
                std::format("5-günlük volatilite, 20-günlük bantın %{:.1f}'ine sıkıştı (Gerilmiş Yay, Hacim Akışı: {:+.2f}). Fiyat dar bir aralıkta kilitlendi; yönlü patlama an meselesi.", c.vol_squeeze_ratio * 100.0, c.institutional_flow),
                std::format("5-day volatility has compressed to {:.1f}% of 20-day band (Coiled Spring, Volume Flow: {:+.2f}). Price locked in narrow range; imminent explosive expansion.", c.vol_squeeze_ratio * 100.0, c.institutional_flow),
                std::format("5-Tage-Volatilität ist auf {:.1f}% des 20-Tage-Bands komprimiert (Gespannte Feder, Fluss: {:+.2f}). Kurs eng eingeklemmt; dynamischer Ausbruch steht bevor.", c.vol_squeeze_ratio * 100.0, c.institutional_flow)
            );
            c.tactical_action = I18n::tr(
                "SESSİZ AKÜMÜLASYON / PATLAMA ÖNCESİ GİRİŞ. Kalabalıklar hisseyi 'sıkıcı' bulurken dar stop ile yüksek asimetrili pozisyonlanma.",
                "PRE-BREAKOUT POSITIONING / STEALTH ENTRY. Position with tight stop and high asymmetry while the crowd still perceives the asset as 'boring'.",
                "POSITIONIERUNG VOR DEM AUSBRUCH / STILLER EINSTIEG. Hohe Asymmetrie mit engem Stop eingehen, solange die Masse die Aktie als 'langweilig' empfindet."
            );
        } else if (c.institutional_flow >= 0.08 && c.crowdedness_pct < 45.0) {
            c.category = AlphaCategory::HiddenAccumulation;
            c.category_label = I18n::tr(
                "💎 GİZLİ ALFA / RADAR ALTI DİP AKÜMÜLASYONU",
                "💎 HIDDEN ALPHA / OFF-RADAR BOTTOM ACCUMULATION",
                "💎 VERSTECKTER ALPHA / AKKUMULATION UNTER DEM RADAR"
            );
            c.stop_loss = c.current_price - (atr_est * 1.2);
            c.target_price = c.current_price + (atr_est * 4.5);
            c.downside_risk_pct = std::max(3.0, ((c.current_price - c.stop_loss) / c.current_price) * 100.0);
            c.upside_potential_pct = ((c.target_price - c.current_price) / c.current_price) * 100.0;
            c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct; // ~3.75x
            c.quant_diagnosis = I18n::tr(
                std::format("Hisse manşetlerden uzakta (%0-45 kalabalık) seyrederken hacim akışı net kurumsal alımı gösteriyor ({:+.2f}). Akıllı para sessizce pozisyon topluyor.", c.institutional_flow),
                std::format("While stock remains away from headlines (0-45% crowd score), volume flow confirms net institutional buying ({:+.2f}). Smart money stealthily building inventory.", c.institutional_flow),
                std::format("Während die Aktie fernab der Schlagzeilen läuft (0-45% Herden-Score), zeigt das Volumen institutionelle Zukäufe ({:+.2f}). Smart Money sammelt unbemerkt Stücke.", c.institutional_flow)
            );
            c.tactical_action = I18n::tr(
                "KURUMSAL İZİNDE DİPTEN TOPLAMA. Çoğunluk ilgilenmezken mükemmel 1:3.7 Risk/Kazanç asimetrisi.",
                "ACCUMULATE ON INSTITUTIONAL FOOTPRINTS. Superb 1:3.7+ Risk/Reward asymmetry while the crowd is not watching.",
                "INSTITUTIONELLEM SMART MONEY FOLGEN. Erstklassiges 1:3,7+ Chance/Risiko-Profil, bevor die Masse aufmerksam wird."
            );
        } else {
            c.category = AlphaCategory::EarlyMomentum;
            c.category_label = I18n::tr(
                "📈 ERKEN MOMENTUM / DENGELİ TREND",
                "📈 EARLY MOMENTUM / BALANCED TREND",
                "📈 FRÜHES MOMENTUM / AUSGEWOGENER TREND"
            );
            c.stop_loss = c.current_price - (atr_est * 1.4);
            c.target_price = c.current_price + (atr_est * 3.5);
            c.downside_risk_pct = std::max(3.5, ((c.current_price - c.stop_loss) / c.current_price) * 100.0);
            c.upside_potential_pct = ((c.target_price - c.current_price) / c.current_price) * 100.0;
            c.asymmetry_ratio = c.upside_potential_pct / c.downside_risk_pct; // ~2.5x
            c.quant_diagnosis = I18n::tr(
                std::format("Teknik göstergeler ve 30G VWAP dengeli seyrediyor (Hacim Akışı: {:+.2f}). Henüz sosyal medyada 'şişirilmiş' duruma gelmedi.", c.institutional_flow),
                std::format("Technical indicators & 30D VWAP remain constructive (Volume Flow: {:+.2f}). Has not yet reached viral retail hype territory.", c.institutional_flow),
                std::format("Technische Indikatoren & 30T VWAP solide (Volumenfluss: {:+.2f}). Noch nicht im überhitzten Social-Media-Hype-Bereich.", c.institutional_flow)
            );
            c.tactical_action = I18n::tr(
                "KADEMELİ TREND ALIMI. Kalabalıklaşmadan önceki dalgada kontrollü pozisyonlanma.",
                "CONTROLLED TREND ENTRY. Build positions smoothly during the early wave before overcrowding occurs.",
                "KONTROLLIERTER TREND-EINSTIEG. Positionieren in der Frühphase vor Überfüllung durch die Herde."
            );
        }

        return c;
    }

    static double compute_rsi_(const std::vector<double>& prices, int period = 14) {
        if (prices.size() <= static_cast<size_t>(period)) return 50.0;

        double gains = 0.0;
        double losses = 0.0;

        // First period SMA
        for (size_t i = 1; i <= static_cast<size_t>(period); ++i) {
            double change = prices[i] - prices[i - 1];
            if (change > 0.0) gains += change;
            else losses -= change;
        }

        double avg_gain = gains / period;
        double avg_loss = losses / period;

        // Wilder's smoothing for subsequent bars
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
            if (prices[i - 1] > 0.0) {
                ret5.push_back(std::log(prices[i] / prices[i - 1]));
            }
        }
        for (size_t i = n - 20; i < n; ++i) {
            if (prices[i - 1] > 0.0) {
                ret20.push_back(std::log(prices[i] / prices[i - 1]));
            }
        }

        auto calc_std = [](const std::vector<double>& v) {
            if (v.empty()) return 0.02;
            double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
            double var = 0.0;
            for (double x : v) var += (x - m) * (x - m);
            return std::sqrt(var / v.size());
        };

        double std5 = calc_std(ret5);
        double std20 = calc_std(ret20);

        if (std20 < 1e-5) return 1.0;
        return std5 / std20;
    }

    static double compute_flow_score_(const std::vector<double>& prices, const std::vector<double>& volumes) {
        if (prices.size() < 5 || volumes.size() < 5) return 0.0;

        size_t lookback = std::min({prices.size() - 1, volumes.size() - 1, size_t{15}});
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
