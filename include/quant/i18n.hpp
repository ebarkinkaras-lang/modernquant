#pragma once

#include <string>
#include <string_view>
#include <format>
#include <algorithm>
#include <vector>

namespace quant {

enum class Language {
    Turkish,
    English,
    German
};

class I18n {
public:
    static I18n& instance() {
        static I18n inst;
        return inst;
    }

    static Language lang() {
        return instance().current_lang_;
    }

    static void set_lang(Language l) {
        instance().current_lang_ = l;
    }

    static std::string lang_code() {
        switch (instance().current_lang_) {
            case Language::English: return "EN";
            case Language::German:  return "DE";
            case Language::Turkish:
            default:                return "TR";
        }
    }

    static std::string lang_name() {
        switch (instance().current_lang_) {
            case Language::English: return "English";
            case Language::German:  return "Deutsch";
            case Language::Turkish:
            default:                return "Türkçe";
        }
    }

    static std::string lang_flag() {
        switch (instance().current_lang_) {
            case Language::English: return "🇬🇧";
            case Language::German:  return "🇩🇪";
            case Language::Turkish:
            default:                return "🇹🇷";
        }
    }

    // Core ternary translation function
    static std::string tr(std::string_view tr_txt, std::string_view en_txt, std::string_view de_txt) {
        switch (instance().current_lang_) {
            case Language::English: return std::string(en_txt);
            case Language::German:  return std::string(de_txt);
            case Language::Turkish:
            default:                return std::string(tr_txt);
        }
    }

    // Common system UI strings
    static std::string menu_title() {
        return tr("ANA MENÜ", "MAIN MENU", "HAUPTMENÜ");
    }

    static std::string select_prompt() {
        return tr("Seçiminizi yapın: ", "Select an option: ", "Ihre Auswahl: ");
    }

    static std::string return_prompt() {
        return tr("Ana menüye dönmek için Enter'a basın...",
                  "Press Enter to return to main menu...",
                  "Drücken Sie Enter, um zum Hauptmenü zurückzukehren...");
    }

    static std::string invalid_option() {
        return tr("[HATA] Geçersiz seçenek. Menüdeki tuşlardan birini seçin.",
                  "[ERROR] Invalid option. Please select one of the keys from the menu.",
                  "[FEHLER] Ungültige Option. Bitte wählen Sie eine Taste aus dem Menü.");
    }

    static std::string exit_message() {
        return tr("[SYS] 1980s Unix Quant Terminal Kapatılıyor. Başarılı yatırımlar dileriz.",
                  "[SYS] Shutting down 1980s Unix Quant Terminal. Wishing you successful investments.",
                  "[SYS] 1980s Unix Quant Terminal wird beendet. Wir wünschen erfolgreiche Investments.");
    }

    static std::string subtitle_terminal() {
        return tr("Modern C++ Quant Terminal v2.0", "Modern C++ Quant Terminal v2.0", "Modernes C++ Quant-Terminal v2.0");
    }

    static std::string status_sources() {
        return tr("Kaynaklar: Daniel Hanson (Modern C++) • Jason Strimpel • Yves Hilpisch",
                  "Sources: Daniel Hanson (Modern C++) • Jason Strimpel • Yves Hilpisch",
                  "Quellen: Daniel Hanson (Modern C++) • Jason Strimpel • Yves Hilpisch");
    }

    static std::string status_standards() {
        return tr("Standart: ISO C++20 • Eigen3 • Boost • LLVM Clang • libcurl",
                  "Standards: ISO C++20 • Eigen3 • Boost • LLVM Clang • libcurl",
                  "Standards: ISO C++20 • Eigen3 • Boost • LLVM Clang • libcurl");
    }

    static std::string theme_label() {
        return tr("Tema: ", "Theme: ", "Design: ");
    }

    static std::string language_label() {
        return tr("Dil: ", "Language: ", "Sprache: ");
    }

private:
    I18n() = default;
    Language current_lang_{Language::Turkish};
};

} // namespace quant
