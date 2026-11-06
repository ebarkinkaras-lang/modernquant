#pragma once

#include "live_market_data.hpp"
#include "i18n.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <format>

namespace quant {

struct NewsHeadlineItem {
    std::string title;
    std::string publisher;
    double polarity_score{0.0}; // -1.0 to +1.0
    int64_t publish_time{0};
    double recency_weight{1.0};
};

struct MacroEconomicNews {
    std::string title;
    std::string source;
    std::string market_impact;
    double polarity_score{0.0};
};

struct AssetSentimentProfile {
    std::string ticker;
    std::string company_name;
    double sentiment_score{0.0}; // -1.00 to +1.00 (0.00 when no live news)
    bool has_live_news{false};
    int live_headline_count{0};
    std::string sentiment_label;
    std::string sentiment_bar;
    std::string macro_catalyst;
    std::string macro_risk;
    std::vector<NewsHeadlineItem> headlines;
};

struct RegionalMacroSentiment {
    std::string region_name;
    double overall_sentiment_score{0.0};
    std::string overall_sentiment_label;
    std::string overall_gauge_bar;
    std::vector<MacroEconomicNews> macro_developments;
    std::string macro_synthesis;

    // Live quantitative regime telemetry
    bool is_crisis_regime{false};
    double benchmark_spot{0.0};
    double benchmark_return_20d_pct{0.0};
    double benchmark_volatility_pct{0.0};
};

class NewsSentimentEngine {
public:
    // UTF-8 aware lowercasing for Turkish, German, and English financial text
    static std::string utf8_to_lower(std::string_view text) {
        std::string out;
        out.reserve(text.size() + 8);
        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c >= 'A' && c <= 'Z') {
                out.push_back(static_cast<char>(c - 'A' + 'a'));
                continue;
            }
            if (i + 1 < text.size()) {
                unsigned char n = static_cast<unsigned char>(text[i + 1]);
                if (c == 0xC4 && n == 0xB0) { out += "i"; ++i; continue; }          // İ -> i
                if (c == 0xC4 && n == 0x9E) { out += "\xC4\x9F"; ++i; continue; }   // Ğ -> ğ
                if (c == 0xC5 && n == 0x9E) { out += "\xC5\x9F"; ++i; continue; }   // Ş -> ş
                if (c == 0xC3 && n == 0x87) { out += "\xC3\xA7"; ++i; continue; }   // Ç -> ç
                if (c == 0xC3 && n == 0x96) { out += "\xC3\xB6"; ++i; continue; }   // Ö -> ö
                if (c == 0xC3 && n == 0x9C) { out += "\xC3\xBC"; ++i; continue; }   // Ü -> ü
                if (c == 0xC3 && n == 0x84) { out += "\xC3\xA4"; ++i; continue; }   // Ä -> ä
                if (c == 0xC3 && n == 0x82) { out += "\xC3\xA2"; ++i; continue; }   // Â -> â
            }
            // Split UTF-8 general punctuation sequences (e.g. ’, “, ”, –, —) into spaces
            if (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80) {
                out.push_back(' ');
                i += 2;
                continue;
            }
            out.push_back(static_cast<char>(c));
        }
        return out;
    }

    // Word-boundary tokenizer preventing substring false positives ("rise" in "enterprise", "gain" in "against")
    static std::vector<std::string> tokenize_words(std::string_view lower_text) {
        std::vector<std::string> tokens;
        std::string cur;
        for (size_t i = 0; i < lower_text.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(lower_text[i]);
            bool is_word_byte = (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') ||
                                (c >= 0x80);
            if (is_word_byte) {
                cur.push_back(static_cast<char>(c));
            } else {
                if (!cur.empty()) {
                    tokens.push_back(std::move(cur));
                    cur.clear();
                }
            }
        }
        if (!cur.empty()) {
            tokens.push_back(std::move(cur));
        }
        return tokens;
    }

    // Word-boundary & negation-aware financial NLP sentiment scoring (Turkish, English & German)
    static double score_headline(const std::string& text) {
        if (text.empty()) return 0.0;
        std::string lower = utf8_to_lower(text);
        std::vector<std::string> tokens = tokenize_words(lower);
        if (tokens.empty()) return 0.0;

        static const std::unordered_set<std::string> severe_negative_words = {
            // Turkish severe market / corporate stress
            "çakılma", "çakıldı", "çöküş", "çöktü", "panik", "iflas", "konkordato",
            "temerrüt", "tasfiye", "soruşturma",
            // English severe shock
            "crash", "crashes", "crashed", "plunge", "plunges", "plunged",
            "selloff", "bankruptcy", "bankrupt", "default", "defaults", "liquidation",
            "collapse", "collapses", "collapsed", "fraud",
            // German severe shock
            "einbruch", "absturz", "insolvenz", "panikverkäufe"
        };

        static const std::unordered_set<std::string> severe_negative_bigrams = {
            "statü uyarısı", "küme düşme", "panik satışı", "devre kesici", "devre kesti",
            "sermaye kaçışı", "likidite krizi", "not indirimi", "taban oldu",
            "capital flight", "margin call", "circuit breaker", "credit downgrade", "profit warning"
        };

        static const std::unordered_set<std::string> positive_words = {
            // Turkish bullish / strong fundamentals
            "rekor", "büyüme", "büyüdü", "kâr", "sipariş", "ihracat", "artış", "artiş", "arttı",
            "yükseliş", "yükseldi", "ralli", "temettü", "yatırım", "zirve", "kazanç",
            "başarı", "ihale", "onay", "güçlü", "beklentiyi", "aştı", "tavan",
            // English whole-word tokens (inflections explicitly enumerated so substrings never match)
            "rally", "rallies", "rallied", "surge", "surges", "surged", "soar", "soars", "soared",
            "jump", "jumps", "jumped", "climb", "climbs", "climbed",
            "upgrade", "upgrades", "upgraded", "beat", "beats", "beating",
            "gain", "gains", "gained", "boost", "boosts", "boosted",
            "growth", "bullish", "profit", "profits", "profitable", "record", "records",
            "outperform", "outperforms", "outperformed", "strong", "stronger", "strongest",
            "rise", "rises", "rose", "rising", "dividend", "dividends", "expand", "expands", "expanded",
            "robust", "breakthrough", "buyback", "optimism", "upbeat", "recovery", "rebound", "rebounds",
            // German positive
            "rekord", "wachstum", "gewinn", "dividende", "anstieg", "aufwärtstrend", "stark"
        };

        static const std::unordered_set<std::string> positive_bigrams = {
            "not artırımı", "net kar", "net kâr", "hedef yükseltti", "alım tavsiyesi",
            "rate cut", "soft landing", "raises guidance", "raised guidance", "beats estimates",
            "beat estimates", "share buyback", "alltime high", "record high"
        };

        static const std::unordered_set<std::string> negative_words = {
            // Turkish bearish / risk
            "düşüş", "dusus", "düştü", "zarar", "kayıp", "baskı", "gerileme", "geriledi",
            "kriz", "ceza", "resesyon", "daralma", "daraldı", "zayıf", "endişe", "uyarı",
            "iptal", "açık", "enflasyonist",
            // English whole-word tokens
            "slump", "slumps", "slumped", "drop", "drops", "dropped", "tumble", "tumbles", "tumbled",
            "loss", "losses", "fall", "falls", "fell", "falling", "slide", "slides", "slid",
            "downgrade", "downgrades", "downgraded", "bearish", "miss", "misses", "missed",
            "warning", "warnings", "slowdown", "recession", "weak", "weaker", "weakness",
            "deficit", "lawsuit", "probe", "tariffs", "layoffs", "headwinds", "decline", "declines", "declined",
            // German negative
            "verlust", "rückgang", "krise", "rezession", "warnung", "schwach", "abwertung"
        };

        static const std::unordered_set<std::string> negative_bigrams = {
            "satış baskısı", "hedef düşürdü", "zarar açıkladı", "yabancı çıkışı", "jeopolitik gerilim",
            "cuts guidance", "cut guidance", "lowered guidance", "lowers guidance",
            "misses estimates", "missed estimates", "rate hike", "trade war"
        };

        static const std::unordered_set<std::string> pre_negations = {
            "no", "not", "without", "never", "neither", "avoid", "avoids", "avoided", "less", "kein", "keine"
        };

        static const std::unordered_set<std::string> post_negations_tr = {
            "değil", "degil", "yok", "olmadı", "olmadi", "beklenmiyor", "görülmedi"
        };

        auto is_negated = [&](size_t idx, size_t span_len) -> bool {
            if (idx >= 1 && pre_negations.contains(tokens[idx - 1])) return true;
            if (idx >= 2 && pre_negations.contains(tokens[idx - 2])) return true;
            if (idx + span_len < tokens.size() && post_negations_tr.contains(tokens[idx + span_len])) return true;
            return false;
        };

        double pos_score = 0.0;
        double neg_score = 0.0;

        // 1. Score Bigrams first and mark consumed indices so single words aren't double-counted
        std::vector<bool> consumed(tokens.size(), false);
        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            std::string bg = tokens[i] + " " + tokens[i + 1];
            bool neg = is_negated(i, 2);
            if (severe_negative_bigrams.contains(bg)) {
                if (neg) pos_score += 1.0;
                else neg_score += 3.0;
                consumed[i] = consumed[i + 1] = true;
            } else if (positive_bigrams.contains(bg)) {
                if (neg) neg_score += 1.2;
                else pos_score += 1.8;
                consumed[i] = consumed[i + 1] = true;
            } else if (negative_bigrams.contains(bg)) {
                if (neg) pos_score += 0.8;
                else neg_score += 1.8;
                consumed[i] = consumed[i + 1] = true;
            }
        }

        // 2. Score Unigrams on unconsumed tokens
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (consumed[i]) continue;
            const std::string& w = tokens[i];
            bool neg = is_negated(i, 1);

            if (severe_negative_words.contains(w)) {
                if (neg) pos_score += 0.8;
                else neg_score += 2.8;
            } else if (negative_words.contains(w)) {
                if (neg) pos_score += 0.6;
                else neg_score += 1.35;
            } else if (positive_words.contains(w)) {
                if (neg) neg_score += 1.0;
                else pos_score += 1.15;
            }
        }

        if (pos_score == 0.0 && neg_score == 0.0) {
            return 0.0; // Strictly neutral when no financial polarity words appear
        }

        // Dampen single-word headlines slightly so 1 mild word isn't +-1.00
        double raw = (pos_score - neg_score) / (pos_score + neg_score + 0.65);
        return std::clamp(raw, -1.0, 1.0);
    }

    // Visual 80s CRT sentiment polarity gauge: [───████┼──────] or [──────┼████──]
    static std::string format_sentiment_bar(double score, int half_width = 6) {
        int fill = std::clamp(static_cast<int>(std::round(std::abs(score) * half_width)), 0, half_width);
        std::string bar = "[";

        if (score < -0.05) {
            for (int i = 0; i < half_width - fill; ++i) bar += "─";
            for (int i = 0; i < fill; ++i) bar += "█";
            bar += "┼";
            for (int i = 0; i < half_width; ++i) bar += "─";
        } else if (score > 0.05) {
            for (int i = 0; i < half_width; ++i) bar += "─";
            bar += "┼";
            for (int i = 0; i < fill; ++i) bar += "█";
            for (int i = 0; i < half_width - fill; ++i) bar += "─";
        } else {
            for (int i = 0; i < half_width; ++i) bar += "─";
            bar += "┼";
            for (int i = 0; i < half_width; ++i) bar += "─";
        }

        bar += "]";
        return bar;
    }

    static std::string format_sentiment_label(double score, bool has_news = true) {
        if (!has_news) {
            return I18n::tr("HABER AKIŞI YOK / NÖTR (0.00)",
                            "NO LIVE NEWS / NEUTRAL (0.00)",
                            "KEINE LIVE-NEWS / NEUTRAL (0.00)");
        }
        if (score >= 0.45) {
            return I18n::tr(std::format("GÜÇLÜ BOĞA ({:+.2f})", score),
                            std::format("STRONG BULLISH ({:+.2f})", score),
                            std::format("STARK BULLISCH ({:+.2f})", score));
        }
        if (score >= 0.12) {
            return I18n::tr(std::format("ILIMLI POZİTİF ({:+.2f})", score),
                            std::format("MODERATELY POSITIVE ({:+.2f})", score),
                            std::format("MODERAT POSITIV ({:+.2f})", score));
        }
        if (score > -0.12) {
            return I18n::tr(std::format("NÖTR / DENGELİ ({:+.2f})", score),
                            std::format("NEUTRAL / BALANCED ({:+.2f})", score),
                            std::format("NEUTRAL / AUSGEWOGEN ({:+.2f})", score));
        }
        if (score > -0.45) {
            return I18n::tr(std::format("TEMKİNLİ NEGATİF ({:+.2f})", score),
                            std::format("CAUTIOUSLY NEGATIVE ({:+.2f})", score),
                            std::format("VORSICHTIG NEGATIV ({:+.2f})", score));
        }
        return I18n::tr(std::format("GÜÇLÜ AYI / RİSK BASKISI ({:+.2f})", score),
                        std::format("STRONG BEARISH / STRESS ({:+.2f})", score),
                        std::format("STARK BÄRISCH / STRESS ({:+.2f})", score));
    }

    // Live Borsa Istanbul (BIST 100 + USD/TRY + Live News) Macro Regime
    static RegionalMacroSentiment build_bist_macro_sentiment(LiveMarketDataClient* client = nullptr) {
        RegionalMacroSentiment macro;
        macro.region_name = "Borsa İstanbul (BIST 100)";

        double regime_score = 0.0;
        int signal_parts = 0;

        if (client) {
            auto xu100 = client->fetch_asset("XU100.IS", "3mo");
            if (xu100.is_live && xu100.historical_prices.size() >= 5) {
                macro.benchmark_spot = xu100.current_price;
                macro.benchmark_volatility_pct = xu100.annualized_volatility * 100.0;
                size_t n = xu100.historical_prices.size();
                size_t back = (n > 21) ? (n - 21) : 0;
                double ret20 = (xu100.historical_prices[back] > 0.0)
                    ? ((xu100.current_price / xu100.historical_prices[back]) - 1.0) * 100.0
                    : 0.0;
                macro.benchmark_return_20d_pct = ret20;

                double idx_pol = std::clamp(std::tanh(ret20 / 9.0), -0.85, 0.85);
                regime_score += idx_pol;
                ++signal_parts;

                macro.macro_developments.push_back({
                    I18n::tr(std::format("BIST 100 Endeks Rejimi (Canlı): {:.0f} puan │ 20-İş Günü Değişim: %{:+.2f} │ Yıllık Volatilite: %{:.1f}",
                                         xu100.current_price, ret20, macro.benchmark_volatility_pct),
                             std::format("BIST 100 Index Regime (Live): {:.0f} pts │ 20-Day Return: {:+.2f}% │ Annual Vol: {:.1f}%",
                                         xu100.current_price, ret20, macro.benchmark_volatility_pct),
                             std::format("BIST 100 Index-Regime (Live): {:.0f} Pkt │ 20-Tage-Rendite: {:+.2f}% │ Volatilität: {:.1f}%",
                                         xu100.current_price, ret20, macro.benchmark_volatility_pct)),
                    "Yahoo Finance / Borsa İstanbul [CANLI VERİ]",
                    I18n::tr("Endeks momentum ve oynaklık rejimi hisse senedi beta risk iştahını doğrudan belirler.",
                             "Index momentum and volatility regime govern local equity beta risk appetite.",
                             "Index-Momentum und Volatilität bestimmen den lokalen Aktien-Risikoappetit."),
                    idx_pol
                });

                if (ret20 <= -10.0 || (ret20 <= -6.0 && xu100.annualized_volatility >= 0.38)) {
                    macro.is_crisis_regime = true;
                }
            }

            auto usdtry = client->fetch_asset("TRY=X", "3mo");
            if (usdtry.is_live && usdtry.historical_prices.size() >= 5) {
                size_t n = usdtry.historical_prices.size();
                size_t back = (n > 21) ? (n - 21) : 0;
                double fx_chg = (usdtry.historical_prices[back] > 0.0)
                    ? ((usdtry.current_price / usdtry.historical_prices[back]) - 1.0) * 100.0
                    : 0.0;
                // Sudden FX spike (>5% in 20d) stresses domestic multiples; mild crawl is neutral
                double fx_pol = (fx_chg > 5.0) ? std::clamp(-(fx_chg - 3.0) / 10.0, -0.75, 0.0)
                                               : std::clamp(0.10 - std::abs(fx_chg - 1.5) * 0.03, -0.25, 0.20);
                regime_score += fx_pol;
                ++signal_parts;

                macro.macro_developments.push_back({
                    I18n::tr(std::format("USD/TRY Kur & Likidite Dengesi (Canlı): {:.2f} TL (20-İş Günü Değişim: %{:+.2f})",
                                         usdtry.current_price, fx_chg),
                             std::format("USD/TRY FX & Liquidity Balance (Live): {:.2f} TRY (20-Day Change: {:+.2f}%)",
                                         usdtry.current_price, fx_chg),
                             std::format("USD/TRY Wechselkurs & Liquidität (Live): {:.2f} TRY (20-Tage: {:+.2f}%)",
                                         usdtry.current_price, fx_chg)),
                    "FX Spot Feed [CANLI VERİ]",
                    I18n::tr("İhracatçı (THYAO, FROTO, ASELS) ve döviz açık/fazla pozisyonlu şirketlerin FAVÖK marjlarını etkiler.",
                             "Directly impacts EBITDA margins of exporters and FX-exposed corporates.",
                             "Beeinflusst direkt die EBITDA-Margen exportorientierter Unternehmen."),
                    fx_pol
                });
            }

            auto live_bist_news = client->fetch_live_headlines("Borsa Istanbul BIST 100 Turkey", 3);
            for (const auto& item : live_bist_news) {
                double pol = score_headline(item.title);
                macro.macro_developments.push_back({
                    item.title,
                    item.publisher + " [CANLI HABER]",
                    I18n::tr("Borsa İstanbul likidite ve kurumsal fiyatlama akışına etki eden güncel haber.",
                             "Live news headline impacting Borsa Istanbul liquidity and institutional pricing.",
                             "Aktuelle Live-Nachricht mit Einfluss auf Borsa Istanbul."),
                    pol
                });
                regime_score += pol;
                ++signal_parts;
            }
        }

        if (macro.macro_developments.empty()) {
            macro.macro_developments.push_back({
                I18n::tr("BIST 100 canlı endeks/haber akışına şu an erişilemedi (Çevrimdışı Nötr Mod).",
                         "BIST 100 live index/news stream currently unreachable (Offline Neutral Mode).",
                         "BIST 100 Live-Stream derzeit nicht erreichbar (Offline Neutral-Modus)."),
                "Sistem Durumu",
                I18n::tr("Varsayımsal kriz/boğa senaryosu enjekte edilmeden nötr (0.00) baz alındı.",
                         "Neutral (0.00) baseline applied without injecting synthetic crisis/bull bias.",
                         "Neutrale Basis (0.00) ohne künstliche Verzerrung."),
                0.0
            });
        }

        macro.overall_sentiment_score = (signal_parts > 0)
            ? std::clamp(regime_score / static_cast<double>(signal_parts), -1.0, 1.0)
            : 0.0;
        if (macro.overall_sentiment_score <= -0.55) {
            macro.is_crisis_regime = true;
        }
        macro.overall_sentiment_label = format_sentiment_label(macro.overall_sentiment_score, signal_parts > 0);
        macro.overall_gauge_bar = format_sentiment_bar(macro.overall_sentiment_score, 6);

        if (macro.is_crisis_regime) {
            macro.macro_synthesis = I18n::tr(
                std::format("BIST 100 endeksinde son 20 iş gününde %{:+.1f} sert geri çekilme ve yüksek oynaklık (%{:.1f}) gözlenmektedir. Savunmacı nakit / düşük kaldıraçlı (Net Nakit) bilançolar önceliklendirilmelidir.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
                std::format("BIST 100 exhibits severe 20-day drawdown ({:+.1f}%) and elevated volatility ({:.1f}%). Prioritize defensive cash and net-cash balance sheets.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
                std::format("BIST 100 zeigt starken Rückgang ({:+.1f}%) bei hoher Volatilität ({:.1f}%). Defensive Net-Cash-Bilanzen bevorzugen.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct)
            );
        } else if (macro.overall_sentiment_score >= 0.15) {
            macro.macro_synthesis = I18n::tr(
                std::format("BIST 100 endeksi pozitif momentum (%{:+.1f} / 20G) ve dengeli kur görünümüyle hareket ediyor; güçlü FAVÖK büyümesi ve makul FD/FAVÖK çarpanlı sanayi/ihracat şirketleri öne çıkıyor.",
                            macro.benchmark_return_20d_pct),
                std::format("BIST 100 trades with constructive 20-day momentum ({:+.1f}%) and stable FX; strong EBITDA growers at reasonable EV/EBITDA multiples lead.",
                            macro.benchmark_return_20d_pct),
                std::format("BIST 100 mit positivem 20-Tage-Momentum ({:+.1f}%); Unternehmen mit starkem EBITDA-Wachstum führen.",
                            macro.benchmark_return_20d_pct)
            );
        } else {
            macro.macro_synthesis = I18n::tr(
                std::format("Borsa İstanbul seçici/dengeli rejimde (20G Endeks: %{:+.1f}, Vol: %{:.1f}); yüksek faiz ortamında Net Borç/FAVÖK oranı düşük ve nakit akışı güçlü hisselerde seçici pozisyonlanma rasyoneldir.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
                std::format("Borsa Istanbul is in a selective regime (20D Index: {:+.1f}%, Vol: {:.1f}%); favor low Net-Debt/EBITDA and high free-cash-flow equities.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
                std::format("Borsa Istanbul in selektivem Regime (20T Index: {:+.1f}%, Vol: {:.1f}%); niedrige Nettoschuld/EBITDA bevorzugen.",
                            macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct)
            );
        }
        return macro;
    }

    // Live Frankfurt XETRA (DAX 40 + EUR/USD + Live News) Macro Regime
    static RegionalMacroSentiment build_xetra_macro_sentiment(LiveMarketDataClient& client) {
        RegionalMacroSentiment macro;
        macro.region_name = "Frankfurt XETRA (DAX 40)";

        double regime_score = 0.0;
        int signal_parts = 0;

        auto dax = client.fetch_asset("^GDAXI", "3mo");
        if (dax.is_live && dax.historical_prices.size() >= 5) {
            macro.benchmark_spot = dax.current_price;
            macro.benchmark_volatility_pct = dax.annualized_volatility * 100.0;
            size_t n = dax.historical_prices.size();
            size_t back = (n > 21) ? (n - 21) : 0;
            double ret20 = (dax.historical_prices[back] > 0.0)
                ? ((dax.current_price / dax.historical_prices[back]) - 1.0) * 100.0
                : 0.0;
            macro.benchmark_return_20d_pct = ret20;

            double idx_pol = std::clamp(std::tanh(ret20 / 6.0), -0.85, 0.85);
            regime_score += idx_pol;
            ++signal_parts;

            macro.macro_developments.push_back({
                I18n::tr(std::format("DAX 40 Endeks Performansı (Canlı): {:.0f} puan │ 20-İş Günü Getiri: %{:+.2f} │ Volatilite: %{:.1f}",
                                     dax.current_price, ret20, macro.benchmark_volatility_pct),
                         std::format("DAX 40 Index Performance (Live): {:.0f} pts │ 20-Day Return: {:+.2f}% │ Volatility: {:.1f}%",
                                     dax.current_price, ret20, macro.benchmark_volatility_pct),
                         std::format("DAX 40 Index-Performance (Live): {:.0f} Pkt │ 20-Tage-Rendite: {:+.2f}% │ Volatilität: {:.1f}%",
                                     dax.current_price, ret20, macro.benchmark_volatility_pct)),
                "XETRA / Yahoo Finance [CANLI VERİ]",
                I18n::tr("Avrupa kurumsal sanayi, yazılım ve finans hisselerinin ana trend göstergesi.",
                         "Primary trend gauge for European industrial, software, and financial blue-chips.",
                         "Haupttrendindikator für europäische Industrie-, Software- und Finanztitel."),
                idx_pol
            });

            if (ret20 <= -8.0 || (ret20 <= -5.0 && dax.annualized_volatility >= 0.30)) {
                macro.is_crisis_regime = true;
            }
        }

        auto eurusd = client.fetch_asset("EURUSD=X", "3mo");
        if (eurusd.is_live && eurusd.current_price > 0.0) {
            macro.macro_developments.push_back({
                std::format("EUR/USD Paritesi (Canlı): {:.4f}", eurusd.current_price),
                "ECB / FX Feed [CANLI VERİ]",
                I18n::tr("DAX küresel ihracatçılarının (SAP, Siemens, Rheinmetall) dolar bazlı rekabetçiliğini belirler.",
                         "Shapes USD competitiveness of global DAX exporters.",
                         "Bestimmt die USD-Wettbewerbsfähigkeit globaler DAX-Exporteure."),
                0.05
            });
        }

        auto live_dax_news = client.fetch_live_headlines("DAX Germany European stocks ECB", 3);
        for (const auto& item : live_dax_news) {
            double pol = score_headline(item.title);
            macro.macro_developments.push_back({
                item.title,
                item.publisher + " [CANLI HABER]",
                I18n::tr("XETRA piyasasında kurumsal fon akışlarını etkileyen canlı haber.",
                         "Live headline influencing institutional flows on Frankfurt XETRA.",
                         "Live-Nachricht mit Einfluss auf institutionelle XETRA-Ströme."),
                pol
            });
            regime_score += pol;
            ++signal_parts;
        }

        macro.overall_sentiment_score = (signal_parts > 0)
            ? std::clamp(regime_score / static_cast<double>(signal_parts), -1.0, 1.0)
            : 0.0;
        macro.overall_sentiment_label = format_sentiment_label(macro.overall_sentiment_score, signal_parts > 0);
        macro.overall_gauge_bar = format_sentiment_bar(macro.overall_sentiment_score, 6);

        macro.macro_synthesis = I18n::tr(
            std::format("Frankfurt XETRA (DAX 40) son 20 iş gününde %{:+.2f} değişim ve %{:.1f} yıllık volatilite ile işlem görmektedir.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
            std::format("Frankfurt XETRA (DAX 40) trades with a {:+.2f}% 20-day return and {:.1f}% annualized volatility.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
            std::format("Frankfurt XETRA (DAX 40) handelt mit {:+.2f}% 20-Tage-Rendite und {:.1f}% Volatilität.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct)
        );
        return macro;
    }

    // Live Wall Street (S&P 500 + CBOE VIX + US Treasury Yield Curve + Live News) Macro Regime
    static RegionalMacroSentiment build_us_macro_sentiment(LiveMarketDataClient& client) {
        RegionalMacroSentiment macro;
        macro.region_name = "Wall Street (S&P 500 & NASDAQ)";

        double regime_score = 0.0;
        int signal_parts = 0;

        auto spx = client.fetch_asset("^GSPC", "3mo");
        if (spx.is_live && spx.historical_prices.size() >= 5) {
            macro.benchmark_spot = spx.current_price;
            macro.benchmark_volatility_pct = spx.annualized_volatility * 100.0;
            size_t n = spx.historical_prices.size();
            size_t back = (n > 21) ? (n - 21) : 0;
            double ret20 = (spx.historical_prices[back] > 0.0)
                ? ((spx.current_price / spx.historical_prices[back]) - 1.0) * 100.0
                : 0.0;
            macro.benchmark_return_20d_pct = ret20;

            double idx_pol = std::clamp(std::tanh(ret20 / 5.5), -0.85, 0.85);
            regime_score += idx_pol;
            ++signal_parts;

            macro.macro_developments.push_back({
                I18n::tr(std::format("S&P 500 Endeks Rejimi (Canlı): {:.2f} │ 20-İş Günü Getiri: %{:+.2f} │ Gerçekleşen Vol: %{:.1f}",
                                     spx.current_price, ret20, macro.benchmark_volatility_pct),
                         std::format("S&P 500 Index Regime (Live): {:.2f} │ 20-Day Return: {:+.2f}% │ Realized Vol: {:.1f}%",
                                     spx.current_price, ret20, macro.benchmark_volatility_pct),
                         std::format("S&P 500 Index-Regime (Live): {:.2f} │ 20-Tage-Rendite: {:+.2f}% │ Volatilität: {:.1f}%",
                                     spx.current_price, ret20, macro.benchmark_volatility_pct)),
                "CME / S&P Dow Jones [CANLI VERİ]",
                I18n::tr("Küresel hisse senedi risk iştahı ve sermaye akımlarının birincil referansı.",
                         "Primary global equity risk-appetite and institutional allocation benchmark.",
                         "Primärer globaler Referenzwert für Aktien-Risikoappetit."),
                idx_pol
            });

            if (ret20 <= -8.0) macro.is_crisis_regime = true;
        }

        auto vix = client.fetch_asset("^VIX", "1mo");
        auto yc = client.fetch_treasury_yields();
        if (vix.is_live && vix.current_price > 0.0) {
            double vix_pol = std::clamp((20.0 - vix.current_price) / 14.0, -0.85, 0.55);
            regime_score += vix_pol;
            ++signal_parts;
            if (vix.current_price >= 32.0) macro.is_crisis_regime = true;

            double slope_bps = (yc.ten_year_yield - yc.three_month_yield) * 10000.0;
            macro.macro_developments.push_back({
                I18n::tr(std::format("CBOE VIX Korku Endeksi: {:.2f} │ ABD Hazine 3A: %{:.2f} / 10Y: %{:.2f} (Eğim: {:+.0f} bps)",
                                     vix.current_price, yc.three_month_yield * 100.0, yc.ten_year_yield * 100.0, slope_bps),
                         std::format("CBOE VIX Volatility: {:.2f} │ US Treasury 3M: {:.2f}% / 10Y: {:.2f}% (Slope: {:+.0f} bps)",
                                     vix.current_price, yc.three_month_yield * 100.0, yc.ten_year_yield * 100.0, slope_bps),
                         std::format("CBOE VIX: {:.2f} │ US-Staatsanleihen 3M: {:.2f}% / 10J: {:.2f}% (Steigung: {:+.0f} bps)",
                                     vix.current_price, yc.three_month_yield * 100.0, yc.ten_year_yield * 100.0, slope_bps)),
                "CBOE / US Treasury [CANLI VERİ]",
                I18n::tr("Opsiyon ima edilen oynaklığı (VIX) ve iskonto oranları çarpan genişlemesini/daralmasını belirler.",
                         "Option implied volatility (VIX) and discount rates drive multiple expansion/compression.",
                         "Implizite Volatilität (VIX) und Diskontsätze steuern die Bewertungsmultiplikatoren."),
                vix_pol
            });
        }

        auto live_spy_news = client.fetch_live_headlines("S&P 500 Wall Street Federal Reserve earnings", 3);
        for (const auto& item : live_spy_news) {
            double pol = score_headline(item.title);
            macro.macro_developments.push_back({
                item.title,
                item.publisher + " [CANLI HABER]",
                I18n::tr("Wall Street endekslerinde kurumsal fiyatlamayı yönlendiren canlı piyasa haberi.",
                         "Live market headline shaping institutional pricing on Wall Street.",
                         "Live-Marktnachricht mit Einfluss auf die Wall Street."),
                pol
            });
            regime_score += pol;
            ++signal_parts;
        }

        macro.overall_sentiment_score = (signal_parts > 0)
            ? std::clamp(regime_score / static_cast<double>(signal_parts), -1.0, 1.0)
            : 0.0;
        macro.overall_sentiment_label = format_sentiment_label(macro.overall_sentiment_score, signal_parts > 0);
        macro.overall_gauge_bar = format_sentiment_bar(macro.overall_sentiment_score, 6);

        macro.macro_synthesis = I18n::tr(
            std::format("Wall Street (S&P 500) son 20 iş gününde %{:+.2f} getiri ve %{:.1f} gerçekleşen volatilite ile fiyatlanmaktadır.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
            std::format("Wall Street (S&P 500) prices in a {:+.2f}% 20-day return with {:.1f}% realized volatility.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct),
            std::format("Wall Street (S&P 500) notiert mit {:+.2f}% 20-Tage-Rendite bei {:.1f}% Volatilität.",
                        macro.benchmark_return_20d_pct, macro.benchmark_volatility_pct)
        );
        return macro;
    }

    // Live, unbiased per-asset news sentiment profile with exponential recency decay (NO hardcoded ticker biases!)
    static AssetSentimentProfile get_asset_sentiment(const std::string& ticker,
                                                     const std::string& company_name,
                                                     LiveMarketDataClient& client) {
        AssetSentimentProfile prof;
        prof.ticker = LiveMarketDataClient::normalize_symbol(ticker);
        prof.company_name = company_name;

        // Build clean search query tailored to asset type
        std::string query = prof.ticker;
        if (prof.ticker == "GRAM-ALTIN.TL" || prof.ticker == "GC=F") {
            query = "Gold price bullion XAU";
        } else if (prof.ticker == "GRAM-GUMUS.TL" || prof.ticker == "SI=F") {
            query = "Silver price bullion XAG";
        }

        auto live = client.fetch_live_headlines(query, 5);
        if (live.empty() && !company_name.empty() && company_name != prof.ticker) {
            // Fallback search by company name, filtering for relevance to base ticker or first company token
            std::string base_sym = prof.ticker;
            auto dot = base_sym.find('.');
            if (dot != std::string::npos) base_sym = base_sym.substr(0, dot);
            std::string base_lower = utf8_to_lower(base_sym);
            auto name_tokens = tokenize_words(utf8_to_lower(company_name));
            std::string first_word = name_tokens.empty() ? "" : name_tokens.front();

            auto alt_news = client.fetch_live_headlines(company_name, 5);
            for (const auto& item : alt_news) {
                std::string tl = utf8_to_lower(item.title);
                if ((!base_lower.empty() && tl.find(base_lower) != std::string::npos) ||
                    (first_word.size() >= 3 && tl.find(first_word) != std::string::npos)) {
                    live.push_back(item);
                }
            }
        }

        int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!live.empty()) {
            prof.has_live_news = true;
            prof.live_headline_count = static_cast<int>(live.size());

            double weighted_sum = 0.0;
            double weight_total = 0.0;
            for (const auto& h : live) {
                double pol = score_headline(h.title);
                double age_days = (h.publish_time > 0 && now_sec > h.publish_time)
                    ? static_cast<double>(now_sec - h.publish_time) / 86400.0
                    : 2.0;
                // Exponential recency decay with tau = 7.0 days
                double w = std::exp(-std::clamp(age_days, 0.0, 60.0) / 7.0);
                prof.headlines.push_back({h.title, h.publisher, pol, h.publish_time, w});
                weighted_sum += w * pol;
                weight_total += w;
            }

            double raw_avg = (weight_total > 0.0) ? (weighted_sum / weight_total) : 0.0;
            // Empirical Bayes shrinkage for small headline count so 1 headline doesn't overfit
            double shrink = static_cast<double>(live.size()) / (static_cast<double>(live.size()) + 1.5);
            prof.sentiment_score = std::clamp(raw_avg * shrink, -1.0, 1.0);
        } else {
            prof.has_live_news = false;
            prof.live_headline_count = 0;
            prof.sentiment_score = 0.0;
        }

        // Dynamic, asset-appropriate catalyst & risk context (informational only; never biases score)
        if (prof.ticker == "GRAM-ALTIN.TL" || prof.ticker == "GC=F" ||
            prof.ticker == "GRAM-GUMUS.TL" || prof.ticker == "SI=F") {
            prof.macro_catalyst = I18n::tr(
                "Küresel merkez bankası rezerv alımları, ABD reel tahvil faizleri (TIPS) ve kur dinamikleri.",
                "Central bank reserve accumulation, US real yields (TIPS), and currency dynamics.",
                "Zentralbank-Reservekäufe, US-Realrenditen und Wechselkursdynamik."
            );
            prof.macro_risk = I18n::tr(
                "ABD Dolar Endeksi (DXY) güçlenmesi ve reel faizlerde yukarı yönlü şoklar.",
                "US Dollar Index (DXY) strength and upward shocks in real Treasury yields.",
                "Stärke des US-Dollar-Index (DXY) und steigende Realzinsen."
            );
        } else if (prof.ticker.ends_with(".IS")) {
            prof.macro_catalyst = I18n::tr(
                "Canlı haber akışı, çeyreklik FAVÖK marj evrimi, ihracat/iç talep dengesi ve yabancı kurumsal akımlar.",
                "Live news flow, quarterly EBITDA margin trajectory, export/domestic demand mix, and institutional flows.",
                "Live-Nachrichtenfluss, EBITDA-Margenentwicklung und institutionelle Kapitalströme."
            );
            prof.macro_risk = I18n::tr(
                "Yüksek TL finansman maliyeti (Net Borç/FAVÖK baskısı), kur/enflasyon geçişkenliği ve likidite oynaklığı.",
                "High TRY financing costs (Net Debt/EBITDA pressure), FX/inflation pass-through, and liquidity volatility.",
                "Hohe TRY-Finanzierungskosten (Nettoschuld/EBITDA-Druck) und Liquiditätsvolatilität."
            );
        } else if (prof.ticker.ends_with(".DE")) {
            prof.macro_catalyst = I18n::tr(
                "ECB para politikası, Avrupa sanayi/teknoloji sipariş defteri ve serbest nakit akışı büyümesi.",
                "ECB monetary stance, European industrial/tech order backlog, and free cash flow expansion.",
                "EZB-Geldpolitik, europäischer Auftragsbestand und Free-Cashflow-Wachstum."
            );
            prof.macro_risk = I18n::tr(
                "Euro Bölgesi PMI yavaşlaması, enerji maliyetleri ve küresel gümrük tarifesi belirsizlikleri.",
                "Eurozone PMI slowdown, energy input costs, and global tariff uncertainties.",
                "Eurozonen-PMI-Abschwächung, Energiekosten und globale Zollunsicherheiten."
            );
        } else {
            prof.macro_catalyst = I18n::tr(
                "Çeyreklik bilanço/FAVÖK büyümesi, analist rehberlik (guidance) revizyonları ve FED likidite döngüsü.",
                "Quarterly earnings/EBITDA growth, forward guidance revisions, and Fed liquidity cycle.",
                "Quartals-EBITDA-Wachstum, Guidance-Revisionen und Fed-Liquiditätszyklus."
            );
            prof.macro_risk = I18n::tr(
                "ABD Hazine tahvil getirilerinde yükseliş, çarpan daralması ve sektörel rekabet baskısı.",
                "Rising US Treasury yields, valuation multiple compression, and sector competition.",
                "Steigende US-Anleiherenditen, Bewertungskompression und Branchenwettbewerb."
            );
        }

        prof.sentiment_label = format_sentiment_label(prof.sentiment_score, prof.has_live_news);
        prof.sentiment_bar = format_sentiment_bar(prof.sentiment_score, 6);
        return prof;
    }
};

} // namespace quant
