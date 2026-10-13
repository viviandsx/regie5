/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/poster.h"

#include "ui/art.h"
#include "ui/style.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

/* A muted colour of hue h (0..1), value v. */
uint32_t tone(float h, float s, float v)
{
    const float c = v * s, x = c * (1 - std::fabs(std::fmod(h * 6, 2.f) - 1)), m = v - c;
    const int k = (int)(h * 6) % 6;
    const float rgb[6][3] = {{c, x, 0}, {x, c, 0}, {0, c, x}, {0, x, c}, {x, 0, c}, {c, 0, x}};
    auto b = [&](int i) { return (uint32_t)std::lround(255.f * (rgb[k][i] + m)); };
    return 0xff000000u | b(0) << 16 | b(1) << 8 | b(2);
}

gfx::Rect lifted(const gfx::Rect &base, float lift)
{
    const float k = 1.f + 0.1f * lift;
    return {base.x - base.w * (k - 1) / 2, base.y - base.h * (k - 1) / 2, base.w * k, base.h * k};
}

} // namespace

uint32_t seed_of(const std::string &s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

void draw_generated_card(const gfx::Rect &r, const std::string &title, uint32_t seed, float radius, float a)
{
    const float h = (seed % 1000) / 1000.f;
    gfx::fill_vgradient(r, alpha(tone(h, 0.55f, 0.42f), a), alpha(tone(std::fmod(h + 0.08f, 1.f), 0.6f, 0.16f), a),
                        radius);
    gfx::rim(r, radius, 0.5f * a);
    if (r.w < 130) {   /* a thumbnail: the name would not fit, its first letter does */
        size_t n = title.empty() ? 0 : 1;
        while (n < title.size() && ((unsigned char)title[n] & 0xC0) == 0x80)
            n++;
        const float size = std::round(r.w * 0.5f);
        gfx::text(r.x + r.w / 2, r.y + r.h / 2 + size * 0.36f, title.substr(0, n), {gfx::Bold, size},
                  alpha(kText2, a), 1);
        return;
    }
    /* The type from a rounded width: a lift must not rasterise the name anew each frame. */
    const float w = std::round(r.w / 20.f) * 20.f;
    const float size = std::max(18.f, std::min(30.f, w / 8.f)), pad = std::max(14.f, w * 0.09f);
    /* At the top, as far in as from the sides (as Jelly5 does). */
    gfx::text(r.x + pad, r.y + pad + size, title, {gfx::Bold, size, w - 2 * pad, 4, size * 1.22f}, alpha(kText, a));
}

void draw_title_card(const gfx::Rect &base, const std::string &title, uint32_t seed, float lift, float a)
{
    const gfx::Rect r = lifted(base, lift);
    const float radius = 14 * (1.f + 0.1f * lift);
    if (lift > 0.01f)
        gfx::shadow(r, radius, 26, 0.3f * lift * a, 10 * lift);
    draw_generated_card({r.x, r.y, r.w, r.h}, title, seed, radius, a);
}

void draw_poster_card(const gfx::Rect &base, const art::Ref &ref, const std::string &title, uint32_t seed, float lift,
                      float a)
{
    const gfx::Rect r = lifted(base, lift);
    const float radius = 14 * (1.f + 0.1f * lift);
    if (lift > 0.01f)
        gfx::shadow(r, radius, 26, 0.35f * lift * a, 10 * lift);
    art::draw_poster(r, ref, title, seed, radius, a, (int)std::lround(base.w * gfx::scale() * 1.1f / 40) * 40);
}

} // namespace ui
