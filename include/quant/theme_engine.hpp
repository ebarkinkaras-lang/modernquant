#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <format>

namespace quant {

enum class CrtThemeId {
    Amber,
    Green,
    Cyan,
    Neon
};

struct CrtColorPalette {
    std::string id_str;
    std::string name;
    std::string icon;
    std::string base_rgb;   // ANSI escape for base
    std::string glow_rgb;   // ANSI escape for glowing text/header
    std::string dim_rgb;    // ANSI escape for dim text
    std::string green_rgb;  // For profit / up moves
    std::string red_rgb;    // For loss / down moves
    std::string reset_code{"\033[0m"};
};

class ThemeEngine {
public:
    static ThemeEngine& instance() {
        static ThemeEngine engine;
        return engine;
    }

    void set_theme(CrtThemeId id) {
        current_id_ = id;
    }

    CrtThemeId current_id() const {
        return current_id_;
    }

    const CrtColorPalette& palette() const {
        switch (current_id_) {
            case CrtThemeId::Green: return green_theme_;
            case CrtThemeId::Cyan:  return cyan_theme_;
            case CrtThemeId::Neon:  return neon_theme_;
            case CrtThemeId::Amber:
            default:                return amber_theme_;
        }
    }

    bool crt_flicker_enabled() const { return crt_flicker_; }
    void toggle_crt_flicker() { crt_flicker_ = !crt_flicker_; }

    // Authentic CRT Cathode Ray Tube warm-up flicker effect
    void play_crt_warmup(const std::string& text) const {
        if (!crt_flicker_) {
            std::cout << "\033[2J\033[H";
            std::cout << palette().glow_rgb << text << palette().reset_code << "\n";
            return;
        }

        std::cout << "\033[2J\033[H";
        // Flicker pulse 1 (Dim)
        std::cout << palette().dim_rgb << text << palette().reset_code << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        // Flicker blank
        std::cout << "\033[2J\033[H" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // Flicker pulse 2 (Base)
        std::cout << palette().base_rgb << text << palette().reset_code << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Steady (Glow)
        std::cout << "\033[2J\033[H";
        std::cout << palette().glow_rgb << text << palette().reset_code << "\n" << std::flush;
    }

    // Print a retro styled box panel
    void print_box(const std::string& title, const std::vector<std::string>& lines, int width = 76) const {
        const auto& p = palette();
        std::string top_border = "┌─[" + title + "]─";
        while (static_cast<int>(top_border.length()) < width - 1) top_border += "─";
        top_border += "┐";

        std::string bot_border = "└";
        for (int i = 0; i < width - 2; ++i) bot_border += "─";
        bot_border += "┘";

        std::cout << p.base_rgb << top_border << p.reset_code << "\n";
        for (const auto& line : lines) {
            std::cout << p.base_rgb << "│ " << p.reset_code
                      << line;
            // Pad remaining space
            int visible_len = 0;
            bool in_escape = false;
            for (char c : line) {
                if (c == '\033') in_escape = true;
                else if (in_escape && c == 'm') in_escape = false;
                else if (!in_escape) visible_len++;
            }
            int padding = std::max(0, width - 4 - visible_len);
            for (int s = 0; s < padding; ++s) std::cout << " ";
            std::cout << p.base_rgb << " │" << p.reset_code << "\n";
        }
        std::cout << p.base_rgb << bot_border << p.reset_code << "\n";
    }

    void print_rule(const std::string& title = "", int width = 76) const {
        const auto& p = palette();
        if (title.empty()) {
            std::string line;
            for (int i = 0; i < width; ++i) line += "─";
            std::cout << p.dim_rgb << line << p.reset_code << "\n";
        } else {
            std::string line = "─── " + title + " ";
            while (static_cast<int>(line.length()) < width) line += "─";
            std::cout << p.base_rgb << line << p.reset_code << "\n";
        }
    }

private:
    ThemeEngine() {
        // 80s Unix Amber CRT (VT220 / IBM 3101)
        amber_theme_ = {
            "amber", "80s Unix Amber CRT (VT220 / IBM 3101)", "🟨",
            "\033[38;2;255;176;0m",    // base amber (#ffb000)
            "\033[1;38;2;255;213;79m",  // glowing amber (#ffd54f)
            "\033[38;2;179;98;0m",     // dim amber (#b36200)
            "\033[1;38;2;34;197;94m",   // green profit
            "\033[1;38;2;239;68;68m"    // red loss
        };

        // 80s Green Phosphor (VT100 / Matrix CRT)
        green_theme_ = {
            "green", "80s Green Phosphor (VT100 / Matrix CRT)", "🟩",
            "\033[38;2;0;255;102m",    // base green (#00ff66)
            "\033[1;38;2;105;240;174m", // glowing green (#69f0ae)
            "\033[38;2;0;112;26m",     // dim green (#00701a)
            "\033[1;38;2;52;211;153m",  // bright green
            "\033[1;38;2;248;113;113m"  // soft red
        };

        // Unix Deep Blue & Cyan (VAX/VMS)
        cyan_theme_ = {
            "cyan", "Unix VAX Deep Blue & Cyan", "🟦",
            "\033[38;2;0;229;255m",    // base cyan (#00e5ff)
            "\033[1;38;2;128;216;255m", // glow cyan (#80d8ff)
            "\033[38;2;0;105;192m",    // dim cyan
            "\033[1;38;2;74;222;128m",
            "\033[1;38;2;248;113;113m"
        };

        // Cyberpunk Neon Synthwave
        neon_theme_ = {
            "neon", "Cyberpunk 80s Neon Synthwave", "🟪",
            "\033[38;2;255;51;187m",   // base neon pink (#ff33bb)
            "\033[1;38;2;255;128;223m", // glow pink (#ff80df)
            "\033[38;2;153;0;115m",    // dim purple
            "\033[1;38;2;52;211;153m",
            "\033[1;38;2;251;113;133m"
        };
    }

    CrtThemeId current_id_{CrtThemeId::Amber};
    bool crt_flicker_{true};

    CrtColorPalette amber_theme_;
    CrtColorPalette green_theme_;
    CrtColorPalette cyan_theme_;
    CrtColorPalette neon_theme_;
};

} // namespace quant
