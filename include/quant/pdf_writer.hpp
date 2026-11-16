#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quant {

struct PdfRgb {
  double r{0.0};
  double g{0.0};
  double b{0.0};

  static constexpr PdfRgb hex(uint32_t rgb) {
    return {
        static_cast<double>((rgb >> 16) & 0xFF) / 255.0,
        static_cast<double>((rgb >> 8) & 0xFF) / 255.0,
        static_cast<double>(rgb & 0xFF) / 255.0,
    };
  }
};

enum class PdfAlign { Left, Center, Right };

// Clean TrueType parser (head, hhea, maxp, hmtx, cmap Format 4) for native UTF-8 PDF Type0 embedding
class PdfTrueTypeFont {
public:
  bool loaded{false};
  std::vector<uint8_t> raw_bytes;
  uint16_t units_per_em{1000};
  int16_t x_min{-100};
  int16_t y_min{-250};
  int16_t x_max{1000};
  int16_t y_max{900};
  int16_t ascent{800};
  int16_t descent{-200};
  uint16_t num_glyphs{0};
  std::vector<int> widths_1000;
  std::unordered_map<uint16_t, uint16_t> cp_to_gid;

  static uint16_t rd_u16(const std::vector<uint8_t> &d, size_t off) {
    if (off + 2 > d.size())
      return 0;
    return static_cast<uint16_t>((d[off] << 8) | d[off + 1]);
  }
  static int16_t rd_i16(const std::vector<uint8_t> &d, size_t off) {
    return static_cast<int16_t>(rd_u16(d, off));
  }
  static uint32_t rd_u32(const std::vector<uint8_t> &d, size_t off) {
    if (off + 4 > d.size())
      return 0;
    return (static_cast<uint32_t>(d[off]) << 24) |
           (static_cast<uint32_t>(d[off + 1]) << 16) |
           (static_cast<uint32_t>(d[off + 2]) << 8) |
           static_cast<uint32_t>(d[off + 3]);
  }

  bool load_from_paths(const std::vector<std::string> &candidates) {
    for (const auto &path : candidates) {
      if (load_file(path))
        return true;
    }
    return false;
  }

  bool load_file(const std::string &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
      return false;
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
      return false;
    auto sz = ifs.tellg();
    if (sz <= 128)
      return false;
    raw_bytes.resize(static_cast<size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read(reinterpret_cast<char *>(raw_bytes.data()), sz)) {
      raw_bytes.clear();
      return false;
    }

    uint32_t sfnt = rd_u32(raw_bytes, 0);
    if (sfnt != 0x00010000 && sfnt != 0x74727565) {
      raw_bytes.clear();
      return false;
    }
    uint16_t num_tables = rd_u16(raw_bytes, 4);
    if (12 + static_cast<size_t>(num_tables) * 16 > raw_bytes.size()) {
      raw_bytes.clear();
      return false;
    }

    struct Tbl {
      uint32_t off{0};
      uint32_t len{0};
    };
    std::unordered_map<std::string, Tbl> tables;
    for (uint16_t i = 0; i < num_tables; ++i) {
      size_t rec = 12 + static_cast<size_t>(i) * 16;
      std::string tag(reinterpret_cast<const char *>(&raw_bytes[rec]), 4);
      uint32_t off = rd_u32(raw_bytes, rec + 8);
      uint32_t len = rd_u32(raw_bytes, rec + 12);
      if (static_cast<size_t>(off) + len <= raw_bytes.size()) {
        tables[tag] = {off, len};
      }
    }

    for (const char *req : {"head", "hhea", "maxp", "hmtx", "cmap"}) {
      if (!tables.contains(req)) {
        raw_bytes.clear();
        return false;
      }
    }

    // head
    uint32_t head_off = tables["head"].off;
    units_per_em = std::max<uint16_t>(16, rd_u16(raw_bytes, head_off + 18));
    x_min = rd_i16(raw_bytes, head_off + 36);
    y_min = rd_i16(raw_bytes, head_off + 38);
    x_max = rd_i16(raw_bytes, head_off + 40);
    y_max = rd_i16(raw_bytes, head_off + 42);

    // hhea & maxp
    uint32_t hhea_off = tables["hhea"].off;
    ascent = rd_i16(raw_bytes, hhea_off + 4);
    descent = rd_i16(raw_bytes, hhea_off + 6);
    uint16_t num_hmetrics = rd_u16(raw_bytes, hhea_off + 34);
    uint32_t maxp_off = tables["maxp"].off;
    num_glyphs = rd_u16(raw_bytes, maxp_off + 4);
    if (num_glyphs == 0 || num_hmetrics == 0) {
      raw_bytes.clear();
      return false;
    }

    // hmtx
    uint32_t hmtx_off = tables["hmtx"].off;
    widths_1000.assign(num_glyphs, 500);
    int last_adv = 500;
    for (uint16_t g = 0; g < num_glyphs; ++g) {
      if (g < num_hmetrics) {
        size_t pos = hmtx_off + static_cast<size_t>(g) * 4;
        if (pos + 2 <= raw_bytes.size()) {
          uint16_t adv = rd_u16(raw_bytes, pos);
          last_adv = static_cast<int>(
              std::llround(static_cast<double>(adv) * 1000.0 / units_per_em));
        }
      }
      widths_1000[g] = last_adv;
    }

    // cmap (prefer Windows Unicode BMP Platform 3 Encoding 1, Format 4)
    uint32_t cmap_off = tables["cmap"].off;
    uint16_t n_sub = rd_u16(raw_bytes, cmap_off + 2);
    size_t sub_off = 0;
    size_t fallback_sub = 0;
    for (uint16_t i = 0; i < n_sub; ++i) {
      size_t rec = cmap_off + 4 + static_cast<size_t>(i) * 8;
      uint16_t pid = rd_u16(raw_bytes, rec);
      uint16_t eid = rd_u16(raw_bytes, rec + 2);
      uint32_t roff = rd_u32(raw_bytes, rec + 4);
      size_t abs_off = static_cast<size_t>(cmap_off) + roff;
      if (abs_off + 16 > raw_bytes.size())
        continue;
      if (rd_u16(raw_bytes, abs_off) == 4) {
        if (pid == 3 && eid == 1) {
          sub_off = abs_off;
          break;
        }
        if (pid == 0 && fallback_sub == 0) {
          fallback_sub = abs_off;
        }
      }
    }
    if (sub_off == 0)
      sub_off = fallback_sub;
    if (sub_off == 0) {
      raw_bytes.clear();
      return false;
    }

    uint16_t seg_count_x2 = rd_u16(raw_bytes, sub_off + 6);
    uint16_t seg_count = seg_count_x2 / 2;
    size_t end_off = sub_off + 14;
    size_t start_off = end_off + static_cast<size_t>(seg_count) * 2 + 2;
    size_t delta_off = start_off + static_cast<size_t>(seg_count) * 2;
    size_t range_off = delta_off + static_cast<size_t>(seg_count) * 2;
    if (range_off + static_cast<size_t>(seg_count) * 2 > raw_bytes.size()) {
      raw_bytes.clear();
      return false;
    }

    cp_to_gid.clear();
    for (uint16_t s = 0; s < seg_count; ++s) {
      uint16_t end_c = rd_u16(raw_bytes, end_off + s * 2);
      uint16_t start_c = rd_u16(raw_bytes, start_off + s * 2);
      int16_t delta = rd_i16(raw_bytes, delta_off + s * 2);
      uint16_t roff = rd_u16(raw_bytes, range_off + s * 2);
      if (start_c == 0xFFFF && end_c == 0xFFFF)
        continue;
      for (uint32_t c = start_c; c <= end_c; ++c) {
        uint16_t gid = 0;
        if (roff == 0) {
          gid = static_cast<uint16_t>((static_cast<int32_t>(c) + delta) & 0xFFFF);
        } else {
          size_t rpos = range_off + s * 2 + roff + 2 * (c - start_c);
          if (rpos + 2 <= raw_bytes.size()) {
            uint16_t raw_g = rd_u16(raw_bytes, rpos);
            if (raw_g != 0) {
              gid = static_cast<uint16_t>(
                  (static_cast<int32_t>(raw_g) + delta) & 0xFFFF);
            }
          }
        }
        if (gid != 0 && gid < num_glyphs) {
          cp_to_gid[static_cast<uint16_t>(c)] = gid;
        }
      }
    }

    loaded = !cp_to_gid.empty();
    return loaded;
  }

  double to_pdf_scale(int16_t val) const {
    return std::round(static_cast<double>(val) * 1000.0 / units_per_em);
  }

  int glyph_width(uint16_t gid) const {
    if (gid < widths_1000.size())
      return widths_1000[gid];
    return 500;
  }
};

// Cross-platform ISO 32000-1 PDF-1.4 Writer with full UTF-8 TrueType support & vector plotting
class PdfDocument {
public:
  static constexpr double kA4Width = 595.28;
  static constexpr double kA4Height = 841.89;

  PdfDocument() {
    load_default_fonts_();
    new_page();
  }

  bool has_unicode_fonts() const { return font_reg_.loaded && font_bold_.loaded; }

  double page_width() const { return kA4Width; }
  double page_height() const { return kA4Height; }
  size_t current_page_index() const { return pages_.size(); }

  void new_page() {
    pages_.emplace_back();
    if (pages_.size() > 1) {
      draw_page_chrome_();
    }
  }

  void set_header_footer_meta(std::string title, std::string right_tag,
                              std::string footer_left) {
    header_title_ = std::move(title);
    header_right_ = std::move(right_tag);
    footer_left_ = std::move(footer_left);
  }

  // Vector primitives using intuitive top-left coordinates (x, top_y)
  void fill_rect(double x, double top_y, double w, double h, PdfRgb c) {
    double by = to_pdf_y_(top_y + h);
    auto &s = cur_stream_();
    s << std::format("q {:.3f} {:.3f} {:.3f} rg {:.2f} {:.2f} {:.2f} {:.2f} re f Q\n",
                     c.r, c.g, c.b, x, by, w, h);
  }

  void stroke_rect(double x, double top_y, double w, double h, PdfRgb c,
                   double lw = 0.75) {
    double by = to_pdf_y_(top_y + h);
    auto &s = cur_stream_();
    s << std::format("q {:.3f} {:.3f} {:.3f} RG {:.2f} w {:.2f} {:.2f} {:.2f} {:.2f} re S Q\n",
                     c.r, c.g, c.b, lw, x, by, w, h);
  }

  void fill_and_stroke_rect(double x, double top_y, double w, double h,
                            PdfRgb fill, PdfRgb stroke, double lw = 0.75) {
    fill_rect(x, top_y, w, h, fill);
    stroke_rect(x, top_y, w, h, stroke, lw);
  }

  void draw_line(double x1, double top_y1, double x2, double top_y2, PdfRgb c,
                 double lw = 0.75, bool dashed = false) {
    auto &s = cur_stream_();
    s << "q ";
    if (dashed)
      s << "[3 2] 0 d ";
    s << std::format("{:.3f} {:.3f} {:.3f} RG {:.2f} w {:.2f} {:.2f} m {:.2f} {:.2f} l S Q\n",
                     c.r, c.g, c.b, lw, x1, to_pdf_y_(top_y1), x2,
                     to_pdf_y_(top_y2));
  }

  void fill_polygon(const std::vector<std::pair<double, double>> &pts, PdfRgb c) {
    if (pts.size() < 3)
      return;
    auto &s = cur_stream_();
    s << std::format("q {:.3f} {:.3f} {:.3f} rg ", c.r, c.g, c.b);
    s << std::format("{:.2f} {:.2f} m ", pts[0].first, to_pdf_y_(pts[0].second));
    for (size_t i = 1; i < pts.size(); ++i) {
      s << std::format("{:.2f} {:.2f} l ", pts[i].first,
                       to_pdf_y_(pts[i].second));
    }
    s << "h f Q\n";
  }

  void draw_polyline(const std::vector<std::pair<double, double>> &pts, PdfRgb c,
                     double lw = 1.2, bool dashed = false) {
    if (pts.size() < 2)
      return;
    auto &s = cur_stream_();
    s << "q ";
    if (dashed)
      s << "[3 2] 0 d ";
    s << std::format("{:.3f} {:.3f} {:.3f} RG {:.2f} w {:.2f} {:.2f} m ", c.r,
                     c.g, c.b, lw, pts[0].first, to_pdf_y_(pts[0].second));
    for (size_t i = 1; i < pts.size(); ++i) {
      s << std::format("{:.2f} {:.2f} l ", pts[i].first,
                       to_pdf_y_(pts[i].second));
    }
    s << "S Q\n";
  }

  void draw_circle(double cx, double top_cy, double r, PdfRgb fill) {
    // 4-segment cubic Bezier circle approximation
    constexpr double k = 0.5522847498;
    double cy = to_pdf_y_(top_cy);
    auto &s = cur_stream_();
    s << std::format("q {:.3f} {:.3f} {:.3f} rg ", fill.r, fill.g, fill.b);
    s << std::format("{:.2f} {:.2f} m ", cx + r, cy);
    s << std::format("{:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} c ",
                     cx + r, cy + r * k, cx + r * k, cy + r, cx, cy + r);
    s << std::format("{:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} c ",
                     cx - r * k, cy + r, cx - r, cy + r * k, cx - r, cy);
    s << std::format("{:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} c ",
                     cx - r, cy - r * k, cx - r * k, cy - r, cx, cy - r);
    s << std::format("{:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} c f Q\n",
                     cx + r * k, cy - r, cx + r, cy - r * k, cx + r, cy);
  }

  double measure_text(std::string_view text, double size, bool bold = false) const {
    const auto &f = bold ? font_bold_ : font_reg_;
    if (!f.loaded) {
      return static_cast<double>(sanitize_ascii_(text).size()) * size * 0.52;
    }
    auto cps = prepare_codepoints_(text, f);
    double sum_1000 = 0.0;
    for (uint16_t cp : cps) {
      auto it = f.cp_to_gid.find(cp);
      uint16_t gid = (it != f.cp_to_gid.end()) ? it->second : 0;
      sum_1000 += static_cast<double>(f.glyph_width(gid));
    }
    return sum_1000 * size / 1000.0;
  }

  std::string fit_text(std::string_view text, double max_w, double size,
                       bool bold = false) const {
    if (measure_text(text, size, bold) <= max_w)
      return std::string(text);
    // Truncate on UTF-8 codepoint boundaries safely
    std::vector<std::pair<size_t, size_t>> utf8_spans;
    for (size_t i = 0; i < text.size();) {
      unsigned char c = static_cast<unsigned char>(text[i]);
      size_t len = 1;
      if ((c & 0xE0) == 0xC0)
        len = 2;
      else if ((c & 0xF0) == 0xE0)
        len = 3;
      else if ((c & 0xF8) == 0xF0)
        len = 4;
      len = std::min(len, text.size() - i);
      utf8_spans.emplace_back(i, len);
      i += len;
    }
    size_t lo = 0, hi = utf8_spans.size();
    while (lo < hi) {
      size_t mid = (lo + hi + 1) / 2;
      size_t byte_end =
          (mid > 0) ? (utf8_spans[mid - 1].first + utf8_spans[mid - 1].second)
                    : 0;
      std::string cand = std::string(text.substr(0, byte_end)) + "...";
      if (measure_text(cand, size, bold) <= max_w)
        lo = mid;
      else
        hi = mid - 1;
    }
    if (lo == 0)
      return {};
    size_t byte_end = utf8_spans[lo - 1].first + utf8_spans[lo - 1].second;
    return std::string(text.substr(0, byte_end)) + "...";
  }

  std::vector<std::string> wrap_text(std::string_view text, double max_w,
                                     double size, bool bold = false) const {
    std::vector<std::string> lines;
    std::string clean_in(text);
    // Split on spaces carefully
    std::istringstream iss(clean_in);
    std::string word;
    std::string cur;
    while (iss >> word) {
      std::string trial = cur.empty() ? word : (cur + " " + word);
      if (measure_text(trial, size, bold) <= max_w) {
        cur = std::move(trial);
      } else {
        if (!cur.empty()) {
          lines.push_back(std::move(cur));
          cur.clear();
        }
        if (measure_text(word, size, bold) <= max_w) {
          cur = std::move(word);
        } else {
          lines.push_back(fit_text(word, max_w, size, bold));
        }
      }
    }
    if (!cur.empty())
      lines.push_back(std::move(cur));
    if (lines.empty())
      lines.emplace_back("");
    return lines;
  }

  void draw_text(double x, double top_baseline_y, std::string_view text,
                 double size, bool bold, PdfRgb color,
                 PdfAlign align = PdfAlign::Left) {
    double tx = x;
    if (align == PdfAlign::Center) {
      tx -= measure_text(text, size, bold) * 0.5;
    } else if (align == PdfAlign::Right) {
      tx -= measure_text(text, size, bold);
    }

    const double ty = to_pdf_y_(top_baseline_y);
    const auto &f = bold ? font_bold_ : font_reg_;
    const char *fname = bold ? "/F2" : "/F1";
    auto &s = cur_stream_();

    if (f.loaded) {
      auto &used_map = bold ? used_gids_bold_ : used_gids_reg_;
      used_map.try_emplace(0, 0x0020);
      auto cps = prepare_codepoints_(text, f);
      std::string hex_str;
      hex_str.reserve(cps.size() * 4);
      for (uint16_t cp : cps) {
        auto it = f.cp_to_gid.find(cp);
        uint16_t gid = (it != f.cp_to_gid.end()) ? it->second : 0;
        if (gid == 0 && cp != 0x20) {
          auto sp_it = f.cp_to_gid.find(0x0020);
          gid = (sp_it != f.cp_to_gid.end()) ? sp_it->second : 0;
        }
        used_map[gid] = cp;
        hex_str += std::format("{:04X}", gid);
      }
      s << std::format(
          "q BT {:.3f} {:.3f} {:.3f} rg {} {:.2f} Tf 1 0 0 1 {:.2f} {:.2f} Tm <{}> Tj ET Q\n",
          color.r, color.g, color.b, fname, size, tx, ty, hex_str);
    } else {
      std::string esc = escape_pdf_literal_(sanitize_ascii_(text));
      s << std::format(
          "q BT {:.3f} {:.3f} {:.3f} rg {} {:.2f} Tf 1 0 0 1 {:.2f} {:.2f} Tm ({}) Tj ET Q\n",
          color.r, color.g, color.b, fname, size, tx, ty, esc);
    }
  }

  // Draws wrapped paragraph and returns bottom baseline y
  double draw_wrapped_text(double x, double top_baseline_y, double max_w,
                           std::string_view text, double size, double leading,
                           bool bold, PdfRgb color) {
    auto lines = wrap_text(text, max_w, size, bold);
    double y = top_baseline_y;
    for (const auto &line : lines) {
      draw_text(x, y, line, size, bold, color, PdfAlign::Left);
      y += leading;
    }
    return y;
  }

  void finalize_page_numbers(std::string_view page_word = "Sayfa") {
    const size_t total = pages_.size();
    for (size_t i = 0; i < total; ++i) {
      active_page_override_ = static_cast<int>(i);
      std::string pg_txt = std::format("{} {} / {}", page_word, i + 1, total);
      draw_text(kA4Width - 36.0, kA4Height - 18.0, pg_txt, 8.0, false,
                PdfRgb::hex(0x64748B), PdfAlign::Right);
    }
    active_page_override_ = -1;
  }

  bool save(const std::string &output_path) {
    std::error_code ec;
    std::filesystem::path p(output_path);
    if (p.has_parent_path()) {
      std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::vector<std::string> objects;
    objects.reserve(32 + pages_.size());
    auto add_obj = [&](std::string body) -> int {
      objects.push_back(std::move(body));
      return static_cast<int>(objects.size()); // 1-based PDF object id
    };

    // Reserve ids cleanly: 1 = Catalog, 2 = Pages, then Fonts, then Pages + Contents
    int catalog_id = add_obj("");
    int pages_id = add_obj("");

    int f1_id = 0;
    int f2_id = 0;
    if (has_unicode_fonts()) {
      f1_id = emit_truetype_font_objs_(font_reg_, used_gids_reg_,
                                       "ModernQuantSegoeUI", add_obj);
      f2_id = emit_truetype_font_objs_(font_bold_, used_gids_bold_,
                                       "ModernQuantSegoeUIBold", add_obj);
    } else {
      f1_id = add_obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
      f2_id =
          add_obj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>");
    }

    std::vector<int> page_ids;
    page_ids.reserve(pages_.size());
    for (const auto &pg_stream : pages_) {
      std::string body = pg_stream.str();
      int cs_id = add_obj(
          std::format("<< /Length {} >>\nstream\n{}endstream", body.size(), body));
      int pid = add_obj(std::format(
          "<< /Type /Page /Parent {} 0 R /MediaBox [0 0 {:.2f} {:.2f}] "
          "/Resources << /Font << /F1 {} 0 R /F2 {} 0 R >> >> "
          "/Contents {} 0 R >>",
          pages_id, kA4Width, kA4Height, f1_id, f2_id, cs_id));
      page_ids.push_back(pid);
    }

    std::ostringstream kids;
    for (size_t i = 0; i < page_ids.size(); ++i) {
      if (i > 0)
        kids << ' ';
      kids << page_ids[i] << " 0 R";
    }
    objects[catalog_id - 1] =
        std::format("<< /Type /Catalog /Pages {} 0 R >>", pages_id);
    objects[pages_id - 1] = std::format("<< /Type /Pages /Kids [{}] /Count {} >>",
                                        kids.str(), page_ids.size());

    // Serialize binary PDF with exact byte-offset xref table
    std::ofstream ofs(output_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
      return false;

    std::string header = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    ofs.write(header.data(), static_cast<std::streamsize>(header.size()));
    size_t cursor = header.size();

    std::vector<size_t> offsets(objects.size() + 1, 0);
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets[i + 1] = cursor;
      std::string chunk =
          std::format("{} 0 obj\n{}\nendobj\n", i + 1, objects[i]);
      ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
      cursor += chunk.size();
    }

    size_t xref_start = cursor;
    std::ostringstream xref;
    xref << "xref\n0 " << (objects.size() + 1) << "\n";
    xref << "0000000000 65535 f \n";
    for (size_t i = 1; i <= objects.size(); ++i) {
      xref << std::format("{:010} 00000 n \n", offsets[i]);
    }
    xref << std::format("trailer\n<< /Size {} /Root {} 0 R >>\nstartxref\n{}\n%%EOF\n",
                        objects.size() + 1, catalog_id, xref_start);
    std::string tail = xref.str();
    ofs.write(tail.data(), static_cast<std::streamsize>(tail.size()));
    ofs.flush();
    return ofs.good();
  }

private:
  PdfTrueTypeFont font_reg_;
  PdfTrueTypeFont font_bold_;
  std::map<uint16_t, uint16_t> used_gids_reg_;
  std::map<uint16_t, uint16_t> used_gids_bold_;
  std::vector<std::ostringstream> pages_;
  int active_page_override_{-1};

  std::string header_title_;
  std::string header_right_;
  std::string footer_left_;

  std::ostringstream &cur_stream_() {
    if (active_page_override_ >= 0 &&
        static_cast<size_t>(active_page_override_) < pages_.size()) {
      return pages_[static_cast<size_t>(active_page_override_)];
    }
    return pages_.back();
  }

  static double to_pdf_y_(double top_y) { return kA4Height - top_y; }

  void draw_page_chrome_() {
    fill_rect(0.0, 0.0, kA4Width, 32.0, PdfRgb::hex(0x0F172A));
    fill_rect(0.0, 32.0, kA4Width, 2.0, PdfRgb::hex(0xD97706));
    if (!header_title_.empty()) {
      draw_text(36.0, 20.5, header_title_, 9.0, true, PdfRgb::hex(0xF8FAFC));
    }
    if (!header_right_.empty()) {
      draw_text(kA4Width - 36.0, 20.5, header_right_, 8.5, false,
                PdfRgb::hex(0xCBD5E1), PdfAlign::Right);
    }
    draw_line(36.0, kA4Height - 28.0, kA4Width - 36.0, kA4Height - 28.0,
              PdfRgb::hex(0xCBD5E1), 0.5);
    if (!footer_left_.empty()) {
      draw_text(36.0, kA4Height - 18.0, footer_left_, 7.5, false,
                PdfRgb::hex(0x64748B));
    }
  }

  void load_default_fonts_() {
    font_reg_.load_from_paths({
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
    });
    font_bold_.load_from_paths({
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/Library/Fonts/Arial Bold.ttf",
    });
    if (font_reg_.loaded && !font_bold_.loaded) {
      font_bold_ = font_reg_;
    }
  }

  static std::vector<uint32_t> decode_utf8_(std::string_view s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      if (c < 0x80) {
        out.push_back(c);
        ++i;
      } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        out.push_back(cp);
        i += 2;
      } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        uint32_t cp = ((c & 0x0F) << 12) |
                      ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        out.push_back(cp);
        i += 3;
      } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
        uint32_t cp = ((c & 0x07) << 18) |
                      ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                      ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        out.push_back(cp);
        i += 4;
      } else {
        out.push_back('?');
        ++i;
      }
    }
    return out;
  }

  static std::vector<uint16_t> prepare_codepoints_(std::string_view text,
                                                   const PdfTrueTypeFont &f) {
    auto cps = decode_utf8_(text);
    std::vector<uint16_t> out;
    out.reserve(cps.size());
    for (uint32_t cp : cps) {
      // Strip variation selectors / zero-width joiners cleanly
      if (cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D)
        continue;
      // Map emoji markers from Forecast/Screener into crisp supported glyphs
      if (cp == 0x1F680 || cp == 0x1F4C8) cp = 0x25B2; // rocket/chart-up -> ▲
      else if (cp == 0x1F53B || cp == 0x1F4C9) cp = 0x25BC; // chart-down -> ▼
      else if (cp == 0x2696) cp = 0x25C6; // balance scale -> ◆
      else if (cp == 0x26A1) cp = '*';
      else if (cp == 0x26A0 || cp == 0x1F6A8) cp = '!';
      else if (cp == 0x2713 || cp == 0x2714) { out.push_back('O'); out.push_back('K'); continue; }
      else if (cp == 0x2718) { out.push_back('X'); continue; }
      else if (cp > 0xFFFF) {
        continue; // Skip unsupported SMP color emojis cleanly (no tofu boxes)
      }

      uint16_t u16 = static_cast<uint16_t>(cp);
      if (f.cp_to_gid.contains(u16)) {
        out.push_back(u16);
      } else if (u16 == 0x20BA) { // ₺ -> TL if missing
        out.push_back('T');
        out.push_back('L');
      } else if (u16 >= 0x2000 && u16 <= 0x206F) {
        out.push_back('-');
      }
    }
    return out;
  }

  static std::string sanitize_ascii_(std::string_view s) {
    auto cps = decode_utf8_(s);
    std::string out;
    out.reserve(s.size());
    for (uint32_t cp : cps) {
      if (cp < 0x80 && cp >= 0x20) {
        out.push_back(static_cast<char>(cp));
        continue;
      }
      switch (cp) {
      case 0x00E7: out.push_back('c'); break;
      case 0x00C7: out.push_back('C'); break;
      case 0x011F: out.push_back('g'); break;
      case 0x011E: out.push_back('G'); break;
      case 0x0131: out.push_back('i'); break;
      case 0x0130: out.push_back('I'); break;
      case 0x00F6: out.push_back('o'); break;
      case 0x00D6: out.push_back('O'); break;
      case 0x015F: out.push_back('s'); break;
      case 0x015E: out.push_back('S'); break;
      case 0x00FC: out.push_back('u'); break;
      case 0x00DC: out.push_back('U'); break;
      case 0x00E4: out.push_back('a'); break;
      case 0x00C4: out.push_back('A'); break;
      case 0x00DF: out += "ss"; break;
      case 0x20AC: out += "EUR"; break;
      case 0x20BA: out += "TL"; break;
      case 0x00B1: out += "+/-"; break;
      case 0x03C3: out += "sigma"; break;
      case 0x2022: out.push_back('*'); break;
      case 0x2013:
      case 0x2014: out.push_back('-'); break;
      case 0x25B2:
      case 0x1F680:
      case 0x1F4C8: out.push_back('^'); break;
      case 0x25BC:
      case 0x1F53B:
      case 0x1F4C9: out.push_back('v'); break;
      default:
        if (cp <= 0xFFFF && cp != 0xFE0F)
          out.push_back(' ');
        break;
      }
    }
    return out;
  }

  static std::string escape_pdf_literal_(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
      if (c == '(' || c == ')' || c == '\\')
        out.push_back('\\');
      out.push_back(c);
    }
    return out;
  }

  template <typename AddObjFn>
  static int emit_truetype_font_objs_(const PdfTrueTypeFont &font,
                                      const std::map<uint16_t, uint16_t> &used,
                                      const std::string &base_name,
                                      AddObjFn &add_obj) {
    std::string font_bytes(font.raw_bytes.begin(), font.raw_bytes.end());
    int font_file_id = add_obj(
        std::format("<< /Length {} /Length1 {} >>\nstream\n{}endstream",
                    font_bytes.size(), font_bytes.size(), font_bytes));
    int bbox_x1 = static_cast<int>(font.to_pdf_scale(font.x_min));
    int bbox_y1 = static_cast<int>(font.to_pdf_scale(font.y_min));
    int bbox_x2 = static_cast<int>(font.to_pdf_scale(font.x_max));
    int bbox_y2 = static_cast<int>(font.to_pdf_scale(font.y_max));
    int asc_s = static_cast<int>(font.to_pdf_scale(font.ascent));
    int des_s = static_cast<int>(font.to_pdf_scale(font.descent));

    std::string desc_body = std::format(
        "<< /Type /FontDescriptor /FontName /{} /Flags 4 "
        "/FontBBox [{} {} {} {}] /ItalicAngle 0 /Ascent {} /Descent {} "
        "/CapHeight {} /StemV 80 /FontFile2 {} 0 R >>",
        base_name, bbox_x1, bbox_y1, bbox_x2, bbox_y2, asc_s, des_s,
        std::max(600, asc_s - 80), font_file_id);
    int desc_id = add_obj(std::move(desc_body));

    std::ostringstream w_arr;
    w_arr << "[";
    bool first_w = true;
    for (const auto &[gid, _cp] : used) {
      if (!first_w)
        w_arr << ' ';
      first_w = false;
      w_arr << gid << " [" << font.glyph_width(gid) << "]";
    }
    w_arr << "]";

    // Build ToUnicode CMap in chunks of <= 100 entries (ISO 32000-1 limit)
    std::ostringstream tounicode;
    tounicode
        << "/CIDInit /ProcSet findresource begin\n"
        << "12 dict begin\n"
        << "begincmap\n"
        << "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
        << "/CMapName /Adobe-Identity-UCS def\n"
        << "/CMapType 2 def\n"
        << "1 begincodespacerange\n"
        << "<0000> <FFFF>\n"
        << "endcodespacerange\n";
    std::vector<std::pair<uint16_t, uint16_t>> pairs(used.begin(), used.end());
    for (size_t off = 0; off < pairs.size(); off += 100) {
      size_t cnt = std::min<size_t>(100, pairs.size() - off);
      tounicode << cnt << " beginbfchar\n";
      for (size_t k = 0; k < cnt; ++k) {
        tounicode << std::format("<{:04X}> <{:04X}>\n", pairs[off + k].first,
                                 pairs[off + k].second);
      }
      tounicode << "endbfchar\n";
    }
    tounicode << "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n";
    std::string tu_str = tounicode.str();
    int tounicode_id = add_obj(
        std::format("<< /Length {} >>\nstream\n{}endstream", tu_str.size(), tu_str));

    int cid_id = add_obj(std::format(
        "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /{} "
        "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
        "/FontDescriptor {} 0 R /DW 1000 /W {} /CIDToGIDMap /Identity >>",
        base_name, desc_id, w_arr.str()));

    int type0_id = add_obj(std::format(
        "<< /Type /Font /Subtype /Type0 /BaseFont /{} "
        "/Encoding /Identity-H /DescendantFonts [{} 0 R] /ToUnicode {} 0 R >>",
        base_name, cid_id, tounicode_id));
    return type0_id;
  }
};

} // namespace quant
