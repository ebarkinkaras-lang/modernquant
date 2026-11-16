#pragma once

#include "live_market_data.hpp"
#include "i18n.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <map>
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

struct StickyHeadlineObs {
    int64_t publish_time{0};
    double polarity{0.0};
    double relevance{1.0};
};

struct StickySentimentSummary {
    double sticky_score{0.0};
    double positive_state{0.0};
    double negative_state{0.0};
    double count_log{0.0};
    double strength{0.0};
    double dispersion{0.0};
    bool is_event_shock{false};
};

struct AssetSentimentProfile {
    std::string ticker;
    std::string company_name;
    double sentiment_score{0.0};       // Combined calibrated sentiment (-1.00 to +1.00)
    double sticky_sentiment_score{0.0}; // 2-stream sticky memory (-1.00 to +1.00)
    double sticky_positive_state{0.0};  // Sustained positive memory [0.00, 1.00]
    double sticky_negative_state{0.0};  // Sustained negative memory [-1.00, 0.00]
    double sent_count_log{0.0};         // log1p(headline_count) news volume factor
    double sent_strength{0.0};          // sticky_sentiment_score * sent_count_log
    double sent_dispersion{0.0};        // StdDev of headline polarities (disagreement)
    bool is_event_shock{false};         // True when a major shock headline persists in memory
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
    // UTF-8 aware lowercasing preserving Turkish characters for financial NLP tokenization
    static std::string utf8_to_lower(std::string_view text) {
        std::string out;
        out.reserve(text.size() + 8);
        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c >= 'A' && c <= 'Z') {
                // In Turkish/ISO mapping, ASCII 'I' lowers to 'i' cleanly for mixed EN/TR headlines
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
                if (c == 0xC3 && n == 0xAE) { out += "i"; ++i; continue; }          // î -> i
                if (c == 0xC3 && n == 0x8E) { out += "i"; ++i; continue; }          // Î -> i
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

    // ASCII-folded lowercasing (ş->s, ı->i, ğ->g, ü->u, ö->o, ç->c) for cross-script entity matching
    static std::string fold_ascii_lower(std::string_view text) {
        std::string lower = utf8_to_lower(text);
        std::string out;
        out.reserve(lower.size());
        for (size_t i = 0; i < lower.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(lower[i]);
            if (i + 1 < lower.size()) {
                unsigned char n = static_cast<unsigned char>(lower[i + 1]);
                if (c == 0xC4 && n == 0xB1) { out.push_back('i'); ++i; continue; } // ı -> i
                if (c == 0xC4 && n == 0x9F) { out.push_back('g'); ++i; continue; } // ğ -> g
                if (c == 0xC5 && n == 0x9F) { out.push_back('s'); ++i; continue; } // ş -> s
                if (c == 0xC3 && n == 0xA7) { out.push_back('c'); ++i; continue; } // ç -> c
                if (c == 0xC3 && n == 0xB6) { out.push_back('o'); ++i; continue; } // ö -> o
                if (c == 0xC3 && n == 0xBC) { out.push_back('u'); ++i; continue; } // ü -> u
                if (c == 0xC3 && n == 0xA4) { out.push_back('a'); ++i; continue; } // ä -> a
                if (c == 0xC3 && n == 0xA2) { out.push_back('a'); ++i; continue; } // â -> a
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

    // Word-boundary, morphology- & contrast-aware financial NLP sentiment scoring (Turkish, English & German)
    static double score_headline(const std::string& text) {
        if (text.empty()) return 0.0;
        std::string lower = utf8_to_lower(text);
        std::vector<std::string> tokens = tokenize_words(lower);
        if (tokens.empty()) return 0.0;

        static const std::unordered_set<std::string> severe_negative_words = {
            // Turkish severe market / corporate / legal scandal & shock
            "çakılma", "çakıldı", "çöküş", "çöktü", "panik", "iflas", "konkordato",
            "temerrüt", "tasfiye", "soruşturma", "skandal", "skandalı", "skandalına",
            "casusluk", "suçlandı", "suçlama", "suçlamalar", "gözaltı", "tutuklama", "tutuklandı",
            "istifa", "istifası", "görevden", "tehlike", "tehlikeye",
            // English severe shock / scandal / criminal / executive crisis
            "crash", "crashes", "crashed", "plunge", "plunges", "plunged", "plummet", "plummets", "plummeted",
            "selloff", "bankruptcy", "bankrupt", "default", "defaults", "liquidation",
            "collapse", "collapses", "collapsed", "fraud", "sank", "sink", "sinks", "tumble", "tumbles", "tumbled",
            "scandal", "scandals", "espionage", "spy", "spying", "indicted", "indictment",
            "arrest", "arrested", "embezzlement", "bribery", "subpoena", "accused", "allegation", "allegations",
            "crisis", "fallout", "slashed", "slash", "slashes", "ousted", "fired",
            "resignation", "resigns", "resigned", "sanction", "sanctions", "sanctioned",
            // German severe shock / scandal
            "einbruch", "absturz", "insolvenz", "panikverkäufe", "skandal", "spionage", "betrug",
            "anklage", "rücktritt", "entlassen", "sanktionen"
        };

        static const std::unordered_set<std::string> severe_negative_trigrams = {
            "price target cut", "price target lowered", "cut price target", "slashes price target",
            "cuts full year", "etf net outflow", "is in crisis", "not düşürüldü", "fiyat hedefi düşürüldü",
            "standart endeksinden çıkardı", "standart endeksinden çıkarıldı",
            "standart endeksten çıkardı", "standart endeksten çıkarıldı",
            "ana endeksten çıkardı", "ana endeksten çıkarıldı",
            "küçük ölçekli endekse", "yakın izleme pazarına", "yakın izleme pazarı",
            "milyarlarca liralık zarar", "net dönem zararı", "ortak pay satışı",
            "removed from msci", "dropped from msci", "deleted from msci", "excluded from msci",
            "removed from index", "dropped from index", "deleted from index", "excluded from index",
            "aus dem index", "abstieg aus dem"
        };

        static const std::unordered_set<std::string> severe_negative_bigrams = {
            "statü uyarısı", "küme düşme", "panik satışı", "devre kesici", "devre kesti",
            "sermaye kaçışı", "likidite krizi", "not indirimi", "taban oldu", "hedef fiyat düşürdü",
            "yasak aşk", "tehlikeye atabilir", "yolun sonuna",
            "endeksinden çıkardı", "endeksinden çıkarıldı",
            "endeksten çıkardı", "endeksten çıkarıldı", "endeksten çıkarılma",
            "endeksten çıkartıldı", "endeks dışı", "endekse indi", "endekse düştü",
            "listeden çıktı", "listeden silindi", "listeden çıkarıldı", "listeden çıkardı",
            "kottan çıkarıldı", "kota dışı", "brüt takas", "işlem yasağı",
            "pay satışı", "ortak satışı", "blok satış", "bedelli sermaye",
            "msci çıkardı", "msci sildi",
            "capital flight", "margin call", "circuit breaker", "credit downgrade", "profit warning",
            "sell off", "sells off", "bad news", "cuts outlook", "slashes outlook", "bearish pattern",
            "ceo exit", "ceo exits", "ceo resigned", "ceo resignation", "stepped down", "steps down",
            "is out", "slashed its", "slashed price", "slashed target", "closes below", "losing year",
            "hit with", "etf outflows", "etf outflow",
            "index deletion", "index exclusion", "msci deletion", "msci exclusion", "msci downgrade",
            "small cap", "relegated to", "relegated from", "removed from", "dropped from",
            "deleted from", "excluded from", "secondary offering", "insider selling", "block trade",
            "index abstieg", "abstieg aus"
        };

        static const std::unordered_set<std::string> positive_words = {
            // Turkish bullish / strong fundamentals
            "rekor", "büyüme", "büyüdü", "kâr", "kar", "kârı", "sipariş", "ihracat", "artış", "artiş", "arttı",
            "yükseliş", "yükseldi", "ralli", "temettü", "yatırım", "zirve", "kazanç", "kazandı", "kazandırdı",
            "başarı", "ihale", "onay", "güçlü", "beklentiyi", "aştı", "tavan", "destekledi", "primlendi",
            // English whole-word tokens (inflections explicitly enumerated so substrings never match)
            "rally", "rallies", "rallied", "surge", "surges", "surged", "soar", "soars", "soared",
            "jump", "jumps", "jumped", "climb", "climbs", "climbed",
            "upgrade", "upgrades", "upgraded", "beat", "beats", "beating",
            "gain", "gains", "gained", "boost", "boosts", "boosted",
            "growth", "bullish", "bull", "profit", "profits", "profitable", "record", "records",
            "outperform", "outperforms", "outperformed", "overweight", "strong", "stronger", "strongest",
            "rise", "rises", "rose", "rising", "dividend", "dividends", "expand", "expands", "expanded", "expansion",
            "robust", "breakthrough", "buyback", "optimism", "upbeat", "recovery", "rebound", "rebounds",
            "win", "wins", "won", "winning", "great", "undervalued", "upside", "momentum", "boom", "booming",
            "tops", "topped", "exceed", "exceeds", "exceeded", "partnership", "contract", "contracts",
            "accelerates", "accelerated", "resilient", "stabilizes", "stabilized", "eases", "easing",
            // German positive
            "rekord", "wachstum", "gewinn", "dividende", "anstieg", "aufwärtstrend", "stark", "kaufempfehlung", "übertraf"
        };

        static const std::unordered_set<std::string> positive_trigrams = {
            "all time high", "price target raised", "raised price target", "beats earnings estimates",
            "new all time", "etkisiz hale getirdi", "hedef fiyat yükseltti",
            "endekse dahil edildi", "added to msci", "added to index", "included in index"
        };

        static const std::unordered_set<std::string> positive_bigrams = {
            "not artırımı", "net kar", "net kâr", "hedef yükseltti", "alım tavsiyesi",
            "anlaşma imzaladı", "sipariş aldı", "ihale kazandı", "rekor kırdı", "en çok kazandıran",
            "endekse eklendi", "endekse ekledi", "endekse alındı", "endekse girdi", "endekse dahil",
            "endeksine eklendi", "endeksine ekledi", "endeksine alındı", "endeksine girdi", "endeksine dahil",
            "rate cut", "soft landing", "raises guidance", "raised guidance", "beats estimates",
            "beat estimates", "share buyback", "alltime high", "record high", "great news",
            "good news", "new contract", "new deal", "etf inflow", "etf inflows", "index inclusion"
        };

        static const std::unordered_set<std::string> negative_words = {
            // Turkish bearish / risk
            "düşüş", "dusus", "düştü", "düşürüldü", "düşürdü", "zarar", "kayıp", "kaybetti", "baskı", "gerileme", "geriledi",
            "kriz", "ceza", "resesyon", "daralma", "daraldı", "zayıf", "endişe", "uyarı", "uyarısı",
            "iptal", "açık", "enflasyonist", "yaptırım", "kısıtlama", "kaybettirdi", "başarısız", "ayrılığı", "ayrılışı",
            "silindi", "kottan", "dışlandı", "kısıtlandı",
            // English whole-word tokens
            "slump", "slumps", "slumped", "drop", "drops", "dropped",
            "loss", "losses", "loser", "losers", "losing", "lost", "fall", "falls", "fell", "falling", "slide", "slides", "slid",
            "downgrade", "downgrades", "downgraded", "bearish", "bear", "miss", "misses", "missed",
            "warning", "warnings", "warn", "warns", "warned", "slowdown", "recession", "weak", "weaker", "weakness",
            "deficit", "lawsuit", "lawsuits", "probe", "tariff", "tariffs", "layoff", "layoffs", "headwind", "headwinds",
            "decline", "declines", "declined", "bad", "cut", "cuts",
            "drag", "drags", "pressure", "pressured", "downside", "overvalued", "underperform", "underweight",
            "fear", "fears", "risk", "risks", "turmoil", "sluggish", "struggle", "struggles", "struggling",
            "contraction", "disappoints", "disappointing", "exit", "exits", "exited", "concern", "concerns",
            "worries", "worry", "fail", "failed", "failing", "sec", "doj", "delisted", "delisting",
            // German negative
            "verlust", "rückgang", "krise", "krisen", "rezession", "warnung", "schwach", "abwertung", "zinsanhebung", "verkaufsdruck",
            "herabstufung", "abgestuft", "gestrichen"
        };

        static const std::unordered_set<std::string> negative_bigrams = {
            "satış baskısı", "hedef düşürdü", "zarar açıkladı", "yabancı çıkışı", "jeopolitik gerilim",
            "kaybettiren hisseler", "değer kaybetti", "düşüş trendi", "taban serisi",
            "endeksi değişti", "pazar değişikliği",
            "cuts guidance", "cut guidance", "lowered guidance", "lowers guidance",
            "misses estimates", "missed estimates", "rate hike", "trade war", "job cuts",
            "extends downside", "downside targets", "alltime low", "transition concerns"
        };

        // Validated Turkish word-initial stems (length >= 5 UTF-8 chars or unambiguous root) for inflected forms
        static const std::vector<std::string> positive_tr_prefixes = {
            "yüksel", "rekor", "büyü", "kazan", "güçl", "artt", "artış",
            "sipariş", "temettü", "ihracat", "yatırım", "onaylan", "anlaşma",
            "başar", "priml", "kârl", "kârı"
        };
        static const std::vector<std::string> negative_tr_prefixes = {
            "düşüş", "düştü", "düşür", "zarar", "kayb", "kayıp", "kaybet", "gerile",
            "daral", "çök", "çakıl", "baskı", "endişe", "iflas", "konkordato",
            "soruştur", "gerilim", "kısıtla", "yaptırım", "kaybettir",
            "skandal", "casus", "suçla", "tutukla", "gözalt", "istifa", "tehlike", "başarısız", "uyar"
        };

        auto starts_with_any = [](const std::string& tok, const std::vector<std::string>& prefs) -> bool {
            if (tok.size() < 5) return false;
            for (const auto& p : prefs) {
                if (tok.starts_with(p)) return true;
            }
            return false;
        };

        static const std::unordered_set<std::string> pre_negations = {
            "no", "not", "without", "never", "neither", "avoid", "avoids", "avoided", "less", "kein", "keine"
        };

        static const std::unordered_set<std::string> post_negations_tr = {
            "değil", "degil", "yok", "olmadı", "olmadi", "beklenmiyor", "görülmedi"
        };

        // Precompute clause weights from contrastive pivots ("but/however/ancak" boost tail; "despite/rağmen" concede clause)
        std::vector<double> clause_w(tokens.size(), 1.0);
        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::string& w = tokens[i];
            if (w == "but" || w == "however" || w == "yet" || w == "ancak" || w == "fakat" || w == "ama") {
                for (size_t j = 0; j < i; ++j) clause_w[j] = std::min(clause_w[j], 0.70);
                for (size_t j = i + 1; j < tokens.size(); ++j) clause_w[j] = std::max(clause_w[j], 1.35);
            } else if (w == "despite" || w == "although") {
                for (size_t j = 0; j < i; ++j) clause_w[j] = std::max(clause_w[j], 1.25);
                for (size_t j = i + 1; j < std::min(tokens.size(), i + 5); ++j) clause_w[j] = std::min(clause_w[j], 0.45);
            } else if (w == "rağmen" || w == "ragmen") {
                for (size_t j = (i > 4 ? i - 4 : 0); j < i; ++j) clause_w[j] = std::min(clause_w[j], 0.45);
                for (size_t j = i + 1; j < tokens.size(); ++j) clause_w[j] = std::max(clause_w[j], 1.25);
            }
        }

        auto is_negated = [&](size_t idx, size_t span_len) -> bool {
            if (idx >= 1 && pre_negations.contains(tokens[idx - 1])) return true;
            if (idx >= 2 && pre_negations.contains(tokens[idx - 2])) return true;
            if (idx + span_len < tokens.size() && post_negations_tr.contains(tokens[idx + span_len])) return true;
            return false;
        };

        double pos_score = 0.0;
        double neg_score = 0.0;
        std::vector<bool> consumed(tokens.size(), false);

        // 1. Score Trigrams first
        for (size_t i = 0; i + 2 < tokens.size(); ++i) {
            std::string tg = tokens[i] + " " + tokens[i + 1] + " " + tokens[i + 2];
            bool neg = is_negated(i, 3);
            double cw = std::max({clause_w[i], clause_w[i + 1], clause_w[i + 2]});
            if (severe_negative_trigrams.contains(tg)) {
                if (neg) pos_score += 1.0 * cw;
                else neg_score += 3.0 * cw;
                consumed[i] = consumed[i + 1] = consumed[i + 2] = true;
            } else if (positive_trigrams.contains(tg)) {
                if (neg) neg_score += 1.2 * cw;
                else pos_score += 2.0 * cw;
                consumed[i] = consumed[i + 1] = consumed[i + 2] = true;
            }
        }

        // 2. Score Bigrams next and mark consumed indices
        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            if (consumed[i] || consumed[i + 1]) continue;
            std::string bg = tokens[i] + " " + tokens[i + 1];
            bool neg = is_negated(i, 2);
            double cw = std::max(clause_w[i], clause_w[i + 1]);
            if (severe_negative_bigrams.contains(bg)) {
                if (neg) pos_score += 1.0 * cw;
                else neg_score += 3.0 * cw;
                consumed[i] = consumed[i + 1] = true;
            } else if (positive_bigrams.contains(bg)) {
                if (neg) neg_score += 1.2 * cw;
                else pos_score += 1.8 * cw;
                consumed[i] = consumed[i + 1] = true;
            } else if (negative_bigrams.contains(bg)) {
                if (neg) pos_score += 0.8 * cw;
                else neg_score += 1.8 * cw;
                consumed[i] = consumed[i + 1] = true;
            }
        }

        // 3. Score Unigrams & Turkish inflected prefixes on unconsumed tokens
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (consumed[i]) continue;
            const std::string& w = tokens[i];
            bool neg = is_negated(i, 1);
            double cw = clause_w[i];

            if (severe_negative_words.contains(w)) {
                if (neg) pos_score += 0.8 * cw;
                else neg_score += 2.8 * cw;
            } else if (negative_words.contains(w) || starts_with_any(w, negative_tr_prefixes)) {
                if (neg) pos_score += 0.6 * cw;
                else neg_score += 1.35 * cw;
            } else if (positive_words.contains(w) || starts_with_any(w, positive_tr_prefixes)) {
                if (neg) neg_score += 1.0 * cw;
                else pos_score += 1.15 * cw;
            }
        }

        // 4. Cross-script separated-token index exclusion guard (MSCI/Index + removal/relegation)
        if (neg_score < 2.0) {
            static const std::unordered_set<std::string> idx_scope_tokens = {
                "msci", "endeks", "endeksi", "endekse", "endeksten", "endeksinden",
                "endeksine", "endeksin", "listeden", "kapsamdan", "kottan", "bist", "index"
            };
            static const std::unordered_set<std::string> idx_excl_tokens = {
                "cikardi", "cikarildi", "cikarilan", "cikarilma", "cikartti", "cikartildi",
                "cikti", "silindi", "sildi", "indi", "dusuruldu", "dislandi",
                "removed", "dropped", "deleted", "excluded", "relegated", "delisted",
                "gestrichen", "herabstufung", "abgestuft"
            };
            auto norm_tokens = tokenize_words(fold_ascii_lower(text));
            bool has_idx_scope = false;
            int excl_idx = -1;
            for (size_t i = 0; i < norm_tokens.size() && i < tokens.size(); ++i) {
                if (idx_scope_tokens.contains(norm_tokens[i])) has_idx_scope = true;
                if (idx_excl_tokens.contains(norm_tokens[i])) excl_idx = static_cast<int>(i);
            }
            if (has_idx_scope && excl_idx >= 0) {
                bool neg = is_negated(static_cast<size_t>(excl_idx), 1);
                double cw = clause_w[static_cast<size_t>(excl_idx)];
                if (neg) pos_score += 0.8 * cw;
                else neg_score += 3.0 * cw;
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

            auto live_bist_news = client->fetch_rss_headlines(
                "https://news.google.com/rss/search?q=%22BIST%20100%22%20OR%20%22Borsa%20%C4%B0stanbul%22&hl=tr&gl=TR&ceid=TR:tr",
                3);
            if (live_bist_news.empty()) {
                live_bist_news = client->fetch_live_headlines("XU100.IS", 3);
            }
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

        auto live_dax_news = client.fetch_rss_headlines(
            "https://news.google.com/rss/search?q=DAX%20OR%20EZB%20Frankfurt&hl=de&gl=DE&ceid=DE:de",
            3);
        if (live_dax_news.empty()) {
            live_dax_news = client.fetch_live_headlines("^GDAXI", 3);
        }
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

        auto live_spy_news = client.fetch_live_headlines("SPY", 3);
        if (live_spy_news.empty()) {
            live_spy_news = client.fetch_rss_headlines(
                "https://news.google.com/rss/search?q=%22S%26P%20500%22%20OR%20%22Federal%20Reserve%22&hl=en-US&gl=US&ceid=US:en",
                3);
        }
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

    // Strips legal suffixes & boilerplate words so news search queries stay crisp across TR/DE/US
    static std::string clean_company_query_(const std::string& company_name) {
        if (company_name.empty()) return {};
        static const std::unordered_set<std::string> legal_stop = {
            "anonim", "ortakligi", "ortaklığı", "sirketi", "şirketi", "sanayi", "ticaret",
            "ve", "elektronik", "birlesik", "birleşik", "magazalar", "mağazalar",
            "t.a.s", "tas", "a.s", "as", "a.o", "ao", "t.a.o", "holding", "holdings",
            "inc", "incorporated", "corp", "corporation", "ltd", "limited", "plc",
            "se", "ag", "co", "company", "sa", "nv", "group", "dec", "usd", "eur", "try"
        };
        std::string lower = utf8_to_lower(company_name);
        auto toks = tokenize_words(lower);
        std::string res;
        int kept = 0;
        for (const auto& t : toks) {
            if (legal_stop.contains(t)) continue;
            if (!res.empty()) res.push_back(' ');
            res += t;
            if (++kept >= 3) break;
        }
        return res;
    }

    // Scores target-company headline relevance (1.0 = direct mention, 0.25 = broad sector, 0.0 = rival/off-topic spillover)
    static double compute_headline_relevance_(const std::string& title,
                                              const std::string& norm_ticker,
                                              const std::string& company_name) {
        std::string norm_title = fold_ascii_lower(title);
        auto title_toks_vec = tokenize_words(norm_title);
        std::unordered_set<std::string> tok_set(title_toks_vec.begin(), title_toks_vec.end());

        std::string base = norm_ticker;
        for (const char* suf : {".IS", ".DE", "-USD", "-EUR", "=F", "=X"}) {
            if (base.ends_with(suf)) {
                base.resize(base.size() - std::char_traits<char>::length(suf));
                break;
            }
        }
        std::string base_lower = fold_ascii_lower(base);

        std::unordered_set<std::string> target_aliases;
        if (base_lower.size() >= 3) target_aliases.insert(base_lower);

        static const std::unordered_set<std::string> alias_stop = {
            "anonim", "ortakligi", "sirketi", "sanayi", "ticaret", "ve", "elektronik",
            "birlesik", "magazalar", "tas", "inc", "corp", "corporation", "ltd", "limited",
            "plc", "se", "ag", "co", "company", "sa", "group", "holding", "holdings",
            "turk", "turkiye", "turkey", "enerji", "insaat", "demir", "celik", "cam",
            "kimya", "hava", "tasimaciligi", "havalimanlari", "otomotiv", "perakende",
            "icecek", "rafinerileri", "savunma",
            "dec", "usd", "eur", "try", "price", "futures", "the"
        };
        auto name_toks = tokenize_words(fold_ascii_lower(company_name));
        for (const auto& nt : name_toks) {
            if (alias_stop.contains(nt)) continue;
            if (nt.size() >= 3) target_aliases.insert(nt);
        }

        // Cross-language / commodity / crypto canonical aliases
        bool is_target_match = false;
        for (const auto& al : target_aliases) {
            if (tok_set.contains(al)) {
                is_target_match = true;
                break;
            }
        }
        if (!is_target_match && (norm_ticker == "GRAM-ALTIN.TL" || norm_ticker == "GC=F")) {
            is_target_match = tok_set.contains("gold") || tok_set.contains("xau") ||
                              tok_set.contains("altin") || tok_set.contains("bullion");
            target_aliases.insert({"gold", "xau", "altin", "bullion"});
        } else if (!is_target_match && (norm_ticker == "GRAM-GUMUS.TL" || norm_ticker == "SI=F")) {
            is_target_match = tok_set.contains("silver") || tok_set.contains("xag") ||
                              tok_set.contains("gumus");
            target_aliases.insert({"silver", "xag", "gumus"});
        } else if (!is_target_match && norm_ticker.ends_with(".IS")) {
            if (base_lower == "thyao" &&
                (tok_set.contains("thy") || norm_title.find("turkish airlines") != std::string::npos ||
                 norm_title.find("turk hava") != std::string::npos)) {
                is_target_match = true;
            }
        }
        if (is_target_match) return 1.0;

        // If headline explicitly names a distinct competing company/crypto without naming the target, reject cross-ticker noise
        static const std::unordered_set<std::string> rival_entities = {
            "nvidia", "nvda", "amd", "intel", "intc", "apple", "aapl", "microsoft", "msft",
            "tesla", "tsla", "amazon", "amzn", "alphabet", "googl", "goog", "meta",
            "netflix", "nflx", "berkshire", "oracle", "orcl", "broadcom", "avgo", "tsmc",
            "bitcoin", "btc", "ethereum", "eth", "bitmine", "blackline", "snowflake",
            "palantir", "pltr", "supermicro", "smci", "qualcomm", "qcom", "micron", "ibm"
        };
        for (const auto& rv : rival_entities) {
            if (!target_aliases.contains(rv) && tok_set.contains(rv)) {
                return 0.0;
            }
        }
        return 0.25; // Broad sector/macro headline mentioning neither rival nor target directly
    }

    // Dual-stream chronological Sticky Sentiment Memory (preserves shocks across silent/neutral weekend days)
    static StickySentimentSummary compute_sticky_sentiment(const std::vector<StickyHeadlineObs>& obs,
                                                           int64_t now_sec = 0,
                                                           double decay_alpha = 0.85,
                                                           double neutral_threshold = 0.10) {
        StickySentimentSummary out;
        if (obs.empty()) return out;

        if (now_sec <= 0) {
            for (const auto& o : obs) {
                if (o.publish_time > now_sec) now_sec = o.publish_time;
            }
            if (now_sec <= 0) {
                now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            }
        }

        struct DayBucket {
            double w_sum{0.0};
            double w_tot{0.0};
            double min_direct{0.0};
            double max_direct{0.0};
        };
        std::map<int64_t, DayBucket> by_day;
        int64_t end_day = now_sec / 86400LL;
        double sum_p = 0.0;
        double sum_p2 = 0.0;

        for (const auto& o : obs) {
            const int64_t ts = (o.publish_time > 0) ? o.publish_time : now_sec;
            const int64_t d = ts / 86400LL;
            end_day = std::max(end_day, d);
            const double rel = std::max(0.05, o.relevance);
            const double pol = std::clamp(o.polarity, -1.0, 1.0);
            auto& b = by_day[d];
            b.w_sum += pol * rel;
            b.w_tot += rel;
            if (rel >= 0.85) {
                b.min_direct = std::min(b.min_direct, pol);
                b.max_direct = std::max(b.max_direct, pol);
            }
            sum_p += pol;
            sum_p2 += pol * pol;
        }

        const int64_t first_day = by_day.begin()->first;
        const int64_t start_day = std::max(first_day, end_day - 14);
        double pos = 0.0;
        double neg = 0.0;

        for (int64_t d = start_day; d <= end_day; ++d) {
            auto it = by_day.find(d);
            if (it == by_day.end() || it->second.w_tot <= 0.0) {
                pos *= decay_alpha;
                neg *= decay_alpha;
                continue;
            }
            const auto& b = it->second;
            double daily_eff = b.w_sum / b.w_tot;
            // Same-day shock preservation: do not let neutral fluff wash out an unambiguous same-day shock
            if (b.min_direct <= -0.50 && b.max_direct < 0.25) {
                daily_eff = std::min(daily_eff, 0.70 * b.min_direct);
            } else if (b.max_direct >= 0.50 && b.min_direct > -0.25) {
                daily_eff = std::max(daily_eff, 0.70 * b.max_direct);
            }
            daily_eff = std::clamp(daily_eff, -1.0, 1.0);

            if (daily_eff > neutral_threshold) {
                pos = std::max(pos * decay_alpha, daily_eff);
                neg *= (daily_eff > 0.45) ? (decay_alpha * 0.75) : decay_alpha;
            } else if (daily_eff < -neutral_threshold) {
                neg = std::min(neg * decay_alpha, daily_eff);
                pos *= (daily_eff < -0.45) ? (decay_alpha * 0.75) : decay_alpha;
            } else {
                // Neutral noise / fluff: preserve existing directional stream with gentle decay only
                pos *= decay_alpha;
                neg *= decay_alpha;
            }
        }

        const double n = static_cast<double>(obs.size());
        const double mean_p = sum_p / n;
        const double var_p = std::max(0.0, (sum_p2 / n) - mean_p * mean_p);

        out.positive_state = std::clamp(pos, 0.0, 1.0);
        out.negative_state = std::clamp(neg, -1.0, 0.0);
        out.sticky_score = std::clamp(pos + neg, -1.0, 1.0);
        out.count_log = std::log1p(n);
        out.strength = out.sticky_score * out.count_log;
        out.dispersion = std::sqrt(var_p);
        out.is_event_shock = (out.negative_state <= -0.32 || out.positive_state >= 0.45);
        return out;
    }

    // Live, unbiased per-asset news sentiment profile combining Yahoo Search, Yahoo RSS & Google News RSS
    static AssetSentimentProfile get_asset_sentiment(const std::string& ticker,
                                                     const std::string& company_name,
                                                     LiveMarketDataClient& client) {
        AssetSentimentProfile prof;
        prof.ticker = LiveMarketDataClient::normalize_symbol(ticker);
        prof.company_name = company_name;

        std::string base_sym = prof.ticker;
        for (const char* suf : {".IS", ".DE", "-USD", "-EUR", "=F", "=X"}) {
            if (base_sym.ends_with(suf)) {
                base_sym.resize(base_sym.size() - std::char_traits<char>::length(suf));
                break;
            }
        }
        std::string clean_name = clean_company_query_(company_name);

        std::vector<LiveMarketDataClient::LiveHeadline> candidates;
        std::unordered_set<std::string> seen_norm_titles;
        auto append_unique = [&](const std::vector<LiveMarketDataClient::LiveHeadline>& batch) {
            for (const auto& item : batch) {
                std::string key = fold_ascii_lower(item.title);
                if (key.empty() || seen_norm_titles.contains(key)) continue;
                seen_norm_titles.insert(key);
                candidates.push_back(item);
            }
        };

        // 1. Primary ticker feed (Yahoo v1 News Search + automatic Yahoo Ticker RSS supplement)
        if (prof.ticker == "GRAM-ALTIN.TL" || prof.ticker == "GC=F") {
            append_unique(client.fetch_live_headlines("GC=F", 5));
            append_unique(client.fetch_rss_headlines(
                "https://news.google.com/rss/search?q=Gold%20price%20XAU&hl=en-US&gl=US&ceid=US:en", 5));
            if (prof.ticker == "GRAM-ALTIN.TL") {
                append_unique(client.fetch_rss_headlines(
                    "https://news.google.com/rss/search?q=%22gram%20alt%C4%B1n%22%20fiyat%C4%B1&hl=tr&gl=TR&ceid=TR:tr", 4));
            }
        } else if (prof.ticker == "GRAM-GUMUS.TL" || prof.ticker == "SI=F") {
            append_unique(client.fetch_live_headlines("SI=F", 5));
            append_unique(client.fetch_rss_headlines(
                "https://news.google.com/rss/search?q=Silver%20price%20XAG&hl=en-US&gl=US&ceid=US:en", 5));
            if (prof.ticker == "GRAM-GUMUS.TL") {
                append_unique(client.fetch_rss_headlines(
                    "https://news.google.com/rss/search?q=%22gram%20g%C3%BCm%C3%BC%C5%9F%22%20fiyat%C4%B1&hl=tr&gl=TR&ceid=TR:tr", 4));
            }
        } else {
            append_unique(client.fetch_live_headlines(prof.ticker, 5));

            // 2. Market-localized RSS enrichment for BIST (.IS), XETRA (.DE), and Crypto when needed
            if (prof.ticker.ends_with(".IS")) {
                std::string q_tr;
                if (!clean_name.empty()) {
                    if (clean_name.size() <= 4) {
                        q_tr = std::format("\"{}\" hisse OR {}", clean_name, base_sym);
                    } else {
                        q_tr = std::format("{} OR \"{}\"", base_sym, clean_name);
                    }
                } else {
                    q_tr = base_sym + " hisse";
                }
                std::string gnews_tr = std::format(
                    "https://news.google.com/rss/search?q={}&hl=tr&gl=TR&ceid=TR:tr",
                    LiveMarketDataClient::url_encode(q_tr));
                append_unique(client.fetch_rss_headlines(gnews_tr, 10));
            } else if (prof.ticker.ends_with(".DE") && candidates.size() < 4) {
                std::string q_de = !clean_name.empty()
                    ? std::format("\"{}\" Aktie OR {}", clean_name, base_sym)
                    : (base_sym + " Aktie");
                std::string gnews_de = std::format(
                    "https://news.google.com/rss/search?q={}&hl=de&gl=DE&ceid=DE:de",
                    LiveMarketDataClient::url_encode(q_de));
                append_unique(client.fetch_rss_headlines(gnews_de, 6));
            } else {
                // Global equities & crypto: merge Google News EN wire coverage (Reuters/CNBC/Morningstar) so clickbait listicles cannot hide scandals/downgrades
                bool is_cryp = prof.ticker.ends_with("-USD") || prof.ticker.ends_with("-EUR");
                std::string q_en = (!clean_name.empty() && clean_name != base_sym)
                    ? std::format("\"{}\" OR {}{}", clean_name, base_sym, is_cryp ? "" : " stock")
                    : (base_sym + (is_cryp ? " crypto" : " stock OR news"));
                std::string gnews_en = std::format(
                    "https://news.google.com/rss/search?q={}&hl=en-US&gl=US&ceid=US:en",
                    LiveMarketDataClient::url_encode(q_en));
                append_unique(client.fetch_rss_headlines(gnews_en, 8));
                if (candidates.size() < 3 && !clean_name.empty() && clean_name != base_sym) {
                    append_unique(client.fetch_live_headlines(clean_name, 4));
                }
            }
        }

        // Filter out cross-ticker competitor noise; drop generic listicles whenever >=2 direct headlines exist
        struct ScoredCandidate {
            LiveMarketDataClient::LiveHeadline item;
            double relevance{1.0};
        };
        std::vector<ScoredCandidate> relevant;
        int direct_count = 0;
        for (auto& c : candidates) {
            double rel = compute_headline_relevance_(c.title, prof.ticker, company_name);
            if (rel <= 0.0) continue;
            if (rel >= 0.9) ++direct_count;
            relevant.push_back({std::move(c), rel});
        }
        if (direct_count >= 2) {
            std::erase_if(relevant, [](const ScoredCandidate& sc) { return sc.relevance < 0.9; });
        }
        std::stable_sort(relevant.begin(), relevant.end(), [](const ScoredCandidate& a, const ScoredCandidate& b) {
            bool a_dir = (a.relevance >= 0.9), b_dir = (b.relevance >= 0.9);
            if (a_dir != b_dir) return a_dir > b_dir;
            // Prioritize factual wire stories over generic question clickbait ("Should you buy...?")
            bool a_q = (!a.item.title.empty() && a.item.title.back() == '?');
            bool b_q = (!b.item.title.empty() && b.item.title.back() == '?');
            if (a_q != b_q) return !a_q;
            return a.item.publish_time > b.item.publish_time;
        });
        if (relevant.size() > 10) relevant.resize(10);

        int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!relevant.empty()) {
            prof.has_live_news = true;
            prof.live_headline_count = static_cast<int>(relevant.size());

            double weighted_sum = 0.0;
            double weight_total = 0.0;
            std::vector<StickyHeadlineObs> sticky_obs;
            sticky_obs.reserve(relevant.size());
            for (const auto& sc : relevant) {
                const auto& h = sc.item;
                double pol = score_headline(h.title);
                double age_days = (h.publish_time > 0 && now_sec > h.publish_time)
                    ? static_cast<double>(now_sec - h.publish_time) / 86400.0
                    : 2.0;
                // Exponential recency decay (tau = 7 days) scaled by direct-vs-sector relevance
                double w_time = std::exp(-std::clamp(age_days, 0.0, 60.0) / 7.0);
                double w_eff = w_time * sc.relevance;
                prof.headlines.push_back({h.title, h.publisher, pol, h.publish_time, w_eff});
                sticky_obs.push_back({h.publish_time > 0 ? h.publish_time : now_sec, pol, sc.relevance});
                weighted_sum += w_eff * pol;
                weight_total += w_eff;
            }

            double raw_avg = (weight_total > 0.0) ? (weighted_sum / weight_total) : 0.0;
            // Empirical Bayes shrinkage scaled by effective relevance weight so broad noise doesn't saturate score
            double eff_n = std::min(static_cast<double>(relevant.size()), weight_total + 1.0);
            double shrink = eff_n / (eff_n + 1.5);
            double raw_shrunk = std::clamp(raw_avg * shrink, -1.0, 1.0);

            auto sticky = compute_sticky_sentiment(sticky_obs, now_sec);
            prof.sticky_sentiment_score = sticky.sticky_score;
            prof.sticky_positive_state = sticky.positive_state;
            prof.sticky_negative_state = sticky.negative_state;
            prof.sent_count_log = sticky.count_log;
            prof.sent_strength = sticky.strength;
            prof.sent_dispersion = sticky.dispersion;
            prof.is_event_shock = sticky.is_event_shock;

            double blended = 0.45 * raw_shrunk + 0.55 * sticky.sticky_score;
            if (sticky.is_event_shock && std::abs(sticky.sticky_score) > std::abs(blended) &&
                (sticky.sticky_score * raw_shrunk >= 0.0 || std::abs(raw_shrunk) < 0.15)) {
                blended = 0.25 * raw_shrunk + 0.75 * sticky.sticky_score;
            }
            prof.sentiment_score = std::clamp(blended, -1.0, 1.0);
        } else {
            prof.has_live_news = false;
            prof.live_headline_count = 0;
            prof.sentiment_score = 0.0;
        }
        DataQualityTelemetry::record_news_fetch(prof.ticker, prof.has_live_news, prof.headlines.size());

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
