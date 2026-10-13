/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/font.h"

#include "platform/platform.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace font {

namespace {

const char *const kFiles[4] = {
    "fonts/Inter-Regular.ttf",
    "fonts/Inter-Medium.ttf",
    "fonts/Inter-SemiBold.ttf",
    "fonts/Inter-Bold.ttf",
};

FT_Library s_ft = nullptr;
struct Face {
    FT_Face face = nullptr;
    std::vector<uint8_t> data;   /* FreeType reads the font from here: kept alive */
    long size26 = 0;             /* the size active now, 26.6 */
    /* One FreeType size object per size used: switching is then only an
     * activation. Setting a size anew runs the font's hinting setup each time,
     * and a screen of texts in several sizes paid it over and over. */
    std::unordered_map<long, FT_Size> sizes;
};
Face s_faces[4];
bool s_ready = false;

struct Glyph {
    std::vector<uint8_t> alpha;
    int w = 0, h = 0, left = 0, top = 0;
    float advance = 0;
    unsigned index = 0;
};
std::unordered_map<uint64_t, Glyph> s_glyphs;

constexpr uint32_t kEllipsis = 0x2026;

int clamp_weight(int w) { return w < 0 ? 0 : w > 3 ? 3 : w; }

long size26_of(float size_px) { return std::max(1L, std::lround(size_px * 64.f)); }

Face &face_at(int weight, long size26)
{
    Face &f = s_faces[clamp_weight(weight)];
    if (f.size26 == size26)
        return f;
    auto it = f.sizes.find(size26);
    if (it != f.sizes.end()) {
        FT_Activate_Size(it->second);
    } else {
        FT_Size sz = nullptr;
        if (f.sizes.size() < 64 && FT_New_Size(f.face, &sz) == 0) {   /* freed with the face */
            FT_Activate_Size(sz);
            f.sizes.emplace(size26, sz);
        }
        FT_Set_Char_Size(f.face, 0, size26, 72, 72);
    }
    f.size26 = size26;
    return f;
}

const Glyph &glyph(int weight, long size26, uint32_t cp)
{
    const uint64_t key = ((uint64_t)clamp_weight(weight) << 62) | ((uint64_t)(size26 & 0x3fffffff) << 32) | cp;
    auto it = s_glyphs.find(key);
    if (it != s_glyphs.end())
        return it->second;
    if (s_glyphs.size() > 20000)
        s_glyphs.clear();   /* a bound, never reached in normal use */
    Glyph g;
    Face &f = face_at(weight, size26);
    g.index = FT_Get_Char_Index(f.face, cp);
    if (FT_Load_Glyph(f.face, g.index, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT) == 0) {
        const FT_GlyphSlot s = f.face->glyph;
        g.advance = (float)s->linearHoriAdvance / 65536.f;
        g.w = (int)s->bitmap.width;
        g.h = (int)s->bitmap.rows;
        g.left = s->bitmap_left;
        g.top = s->bitmap_top;
        if (g.w > 0 && g.h > 0 && s->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            g.alpha.resize((size_t)g.w * g.h);
            for (int y = 0; y < g.h; y++)
                std::memcpy(&g.alpha[(size_t)y * g.w], s->bitmap.buffer + (ptrdiff_t)y * s->bitmap.pitch, g.w);
        } else {
            g.w = g.h = 0;
        }
    }
    return s_glyphs.emplace(key, std::move(g)).first->second;
}

std::vector<uint32_t> decode(const std::string &s)
{
    std::vector<uint32_t> out;
    out.reserve(s.size());
    const unsigned char *p = (const unsigned char *)s.data();
    for (size_t i = 0, n = s.size(); i < n;) {
        uint32_t c = p[i];
        int len = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
        if (len == 0 || i + len > n) {
            out.push_back(0xfffd);
            i++;
            continue;
        }
        if (len > 1) {
            c &= 0xff >> (len + 1);
            for (int k = 1; k < len; k++)
                c = (c << 6) | (p[i + k] & 0x3f);
        }
        out.push_back(c);
        i += len;
    }
    return out;
}

float kerning(int weight, long size26, unsigned prev, unsigned cur)
{
    Face &f = face_at(weight, size26);
    if (!prev || !FT_HAS_KERNING(f.face))
        return 0;
    FT_Vector k;
    if (FT_Get_Kerning(f.face, prev, cur, FT_KERNING_UNFITTED, &k) != 0)
        return 0;
    return (float)k.x / 64.f;
}

float measure(int weight, long size26, const uint32_t *cp, size_t n)
{
    float w = 0;
    unsigned prev = 0;
    for (size_t i = 0; i < n; i++) {
        const Glyph &g = glyph(weight, size26, cp[i]);
        w += kerning(weight, size26, prev, g.index) + g.advance;
        prev = g.index;
    }
    return w;
}

using Line = std::vector<uint32_t>;

/* line cut to fit max_w, ending in an ellipsis. */
Line ellipsize(int weight, long size26, Line line, float max_w)
{
    const float ell = glyph(weight, size26, kEllipsis).advance;
    while (!line.empty() && measure(weight, size26, line.data(), line.size()) + ell > max_w)
        line.pop_back();
    while (!line.empty() && line.back() == ' ')
        line.pop_back();
    line.push_back(kEllipsis);
    return line;
}

std::vector<Line> wrap(int weight, long size26, const std::vector<uint32_t> &text, float max_w, int max_lines)
{
    std::vector<Line> lines;
    if (max_lines <= 1 || max_w <= 0) {
        Line l(text.begin(), text.end());
        for (auto &c : l)
            if (c == '\n')
                c = ' ';
        if (max_w > 0 && measure(weight, size26, l.data(), l.size()) > max_w)
            l = ellipsize(weight, size26, l, max_w);
        lines.push_back(l);
        return lines;
    }
    /* Greedy word wrap; a word wider than the line is broken where it must be. */
    size_t i = 0;
    const size_t n = text.size();
    while (i < n && (int)lines.size() < max_lines) {
        while (i < n && text[i] == ' ')
            i++;
        size_t end = i, last_fit = i;
        while (end < n && text[end] != '\n') {
            size_t word_end = end;
            while (word_end < n && text[word_end] == ' ')
                word_end++;
            while (word_end < n && text[word_end] != ' ' && text[word_end] != '\n')
                word_end++;
            if (measure(weight, size26, &text[i], word_end - i) <= max_w) {
                end = last_fit = word_end;
                continue;
            }
            if (last_fit == i) {   /* the first word alone is too wide: break it */
                size_t k = i + 1;
                while (k < word_end && measure(weight, size26, &text[i], k + 1 - i) <= max_w)
                    k++;
                last_fit = k;
            }
            break;
        }
        if (end >= n || text[end] == '\n')
            last_fit = end;
        lines.emplace_back(text.begin() + i, text.begin() + last_fit);
        i = last_fit;
        if (i < n && text[i] == '\n')
            i++;
    }
    while (i < n && (text[i] == ' ' || text[i] == '\n'))
        i++;
    if (i < n && !lines.empty()) {   /* more than fits: the last line ends in an ellipsis */
        Line &last = lines.back();
        last.insert(last.end(), text.begin() + i, text.end());
        last = ellipsize(weight, size26, last, max_w);
    }
    return lines;
}

} // namespace

bool init()
{
    if (s_ready)
        return true;
    if (FT_Init_FreeType(&s_ft) != 0) {
        std::fprintf(stderr, "font: FreeType did not start\n");
        return false;
    }
    for (int i = 0; i < 4; i++) {
        Face &f = s_faces[i];
        if (!platform::read_asset(kFiles[i], &f.data) ||
            FT_New_Memory_Face(s_ft, f.data.data(), (FT_Long)f.data.size(), 0, &f.face) != 0) {
            std::fprintf(stderr, "font: cannot load %s\n", kFiles[i]);
            return false;
        }
    }
    s_ready = true;
    return true;
}

float width(int weight, float size_px, const std::string &utf8)
{
    if (!s_ready || utf8.empty())
        return 0;
    const std::vector<uint32_t> cp = decode(utf8);
    return measure(weight, size26_of(size_px), cp.data(), cp.size());
}

GlyphImage glyph_image(int weight, int size_key, uint32_t cp)
{
    GlyphImage gi;
    if (!s_ready)
        return gi;
    const Glyph &g = glyph(weight, size_key, cp);
    if (g.w > 0 && g.h > 0)
        gi.alpha = g.alpha.data(), gi.w = g.w, gi.h = g.h;
    return gi;
}

Run layout(int weight, float size_px, const std::string &utf8, float max_w, int max_lines, float line_h)
{
    Run run;
    if (!s_ready || utf8.empty())
        return run;
    const long size26 = size26_of(size_px);
    run.size_key = (int)size26;
    const std::vector<Line> lines = wrap(weight, size26, decode(utf8), max_w, max_lines);
    const FT_Size_Metrics &m = face_at(weight, size26).face->size->metrics;
    const float asc = (float)m.ascender / 64.f, desc = -(float)m.descender / 64.f;
    if (line_h <= 0)
        line_h = size_px * 1.4f;
    run.ascent = (int)std::ceil(asc);
    run.height = (int)std::ceil(run.ascent + (lines.size() - 1) * line_h + desc);

    for (size_t li = 0; li < lines.size(); li++) {
        const Line &l = lines[li];
        const int base = (int)std::lround(li * line_h);
        float pen = 0;
        unsigned prev = 0;
        for (uint32_t c : l) {
            const Glyph &g = glyph(weight, size26, c);
            pen += kerning(weight, size26, prev, g.index);
            prev = g.index;
            if (g.w > 0 && g.h > 0)
                run.glyphs.push_back({c, (int)std::lround(pen) + g.left, base - g.top});
            pen += g.advance;
        }
        run.advance = std::max(run.advance, pen);
    }
    return run;
}

} // namespace font
