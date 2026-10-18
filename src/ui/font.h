/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Inter through FreeType: measuring and laying out UTF-8 text, and each
 * glyph's coverage (ui/gfx_text puts them in an atlas). Sizes here are output
 * pixels (the caller multiplies by gfx::scale()). Portable: FreeType is there
 * on the PS5 too.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace font {

/* Loads the four Inter weights (once). false when one is missing. The CJK
 * fallback is read later, at the first character Inter has not. */
bool init();
/* Whether that fallback has been read (and was there). */
bool fallback_loaded();

/* weight: gfx::Weight (0 Regular .. 3 Bold). */
float width(int weight, float size_px, const std::string &utf8);

/* A glyph's coverage, as FreeType drew it (w x h, top-left at the placed
 * position). Null alpha for an empty glyph (a space). Valid until the next
 * call into font. */
struct GlyphImage {
    const uint8_t *alpha = nullptr;
    int w = 0, h = 0;
};
GlyphImage glyph_image(int weight, int size_key, uint32_t cp);

/* Text laid out: each visible glyph and where its bitmap's top-left goes, in
 * whole output pixels from the pen's start on the first baseline. */
struct Placed {
    uint32_t cp;
    int x, y;
};
struct Run {
    std::vector<Placed> glyphs;
    int size_key = 0;    /* the size the glyphs are at (for glyph_image) */
    int ascent = 0;      /* px above the first baseline the line box reaches */
    int height = 0;      /* px from that top to the last line's bottom */
    float advance = 0;   /* width of the widest line, px */
};

/* One line (max_lines 1: cut with an ellipsis past max_w) or wrapped text (at
 * most max_lines lines within max_w, the last ellipsised), line_h apart.
 * max_w 0: no limit. */
Run layout(int weight, float size_px, const std::string &utf8, float max_w, int max_lines, float line_h);

} // namespace font
