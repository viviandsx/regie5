/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The fonts: Inter, the CJK fallback read only when needed, □ for what
 * neither has, nothing at all for characters without a shape.
 */
#include "check.h"

#include "ui/font.h"

namespace {

bool drawn(int weight, uint32_t cp)
{
    const font::GlyphImage g = font::glyph_image(weight, 40 * 64, cp);
    return g.alpha && g.w > 0 && g.h > 0;
}

float w(const char *s, int weight = 0) { return font::width(weight, 40, s); }

} // namespace

TEST(fonts_fall_back_for_cjk_and_box_the_rest)
{
    CHECK(font::init());
    /* Latin text never reads the fallback. */
    CHECK(w("Régie5 — Bibliothèque") > 0);
    CHECK(!font::fallback_loaded());
    /* Japanese, Chinese (simplified, traditional), Korean: drawn, from the fallback. */
    for (uint32_t cp : {0x3042u /* あ */, 0x30A2u /* ア */, 0x65E5u /* 日 */, 0x8FD9u /* 这 */, 0x9019u /* 這 */,
                        0xD55Cu /* 한 */})
        CHECK(drawn(0, cp) && drawn(3, cp));
    CHECK(font::fallback_loaded());
    CHECK(w("千と千尋の神隠し") > w("千と千尋"));
    /* Not in either (Thai, Arabic): the same □ as U+25A1, never .notdef. */
    CHECK(w("\xe0\xb8\x81") == w("\xe2\x96\xa1"));    /* ก */
    CHECK(w("\xd8\xb9") == w("\xe2\x96\xa1"));        /* ع */
    /* No shape, no width: a zero-width space, a variation selector. */
    CHECK(w("\xe2\x80\x8b") == 0.f);
    CHECK(w("A\xef\xb8\x8f" "B") == w("AB"));
    /* Inter's kerning between Inter's glyphs only. */
    CHECK(w("AV") <= w("A") + w("V"));
    CHECK(w("A日") == w("A") + w("日"));
}
