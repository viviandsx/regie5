/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * glass_panel, Lifts and row_scroll are Jelly5's (app/src/ui/screen.cpp,
 * GPL-3.0-or-later).
 */
#include "ui/style.h"

#include <algorithm>
#include <cmath>

namespace ui {

void prewarm_text()
{
    static const char *kChars = "0123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ "
                                "àâäçéèêëîïôöùûüÿœæ ÀÂÇÉÈÊËÎÏÔÙÛÜŒ .,:;!?'’\"«»()-–—/%+&#·…°×";
    /* The weights and sizes the screens use most (counted in src/ui). */
    static const gfx::TextStyle kStyles[] = {
        {gfx::Medium, 24},   {gfx::SemiBold, 28}, {gfx::Regular, 22}, {gfx::Regular, 24},
        {gfx::SemiBold, 26}, {gfx::Medium, 26},   {gfx::SemiBold, 30}, {gfx::SemiBold, 32},
    };
    for (const gfx::TextStyle &st : kStyles)
        gfx::prewarm_glyphs(kChars, st);
}

uint32_t alpha(uint32_t c, float a)
{
    a = a < 0 ? 0 : a > 1 ? 1 : a;
    return ((uint32_t)std::lround((c >> 24) * a) << 24) | (c & 0xffffffu);
}

void glass_panel(const gfx::Rect &r, float radius, float a, bool shadow, float lift)
{
    if (a <= 0.01f)
        return;
    if (lift > 0) {
        /* The focus drop: that glass lifted - brighter, a sheen from above, a lit rim. */
        if (shadow)
            gfx::shadow(r, radius, 30, 0.35f * a, 10);
        gfx::fill(r, alpha(0x2effffffu, a * lift), radius);
        gfx::fill_vgradient(r, alpha(0x30ffffffu, a * lift), 0x00000000u, radius);
        gfx::rim(r, radius, 0.45f * a);
        return;
    }
    if (shadow)
        gfx::shadow(r, radius, 40, 0.4f * a, 14);
    const int glass = gfx::backdrop_blur(r, radius, 18.f, a, 0.f);
    if (glass == 2)
        return;   /* the shader drew the whole pane */
    if (glass == 1) {
        gfx::fill(r, alpha(0x4d0c0c12u, a), radius);
        gfx::fill_vgradient(r, alpha(0x22ffffffu, a), 0x00000000u, radius);
        gfx::rim(r, radius, 0.9f * a);
    } else {   /* no blur to be had: the solid dark glass */
        gfx::fill(r, alpha(0xdc1c1c22u, a), radius);
        gfx::fill({r.x + radius * 0.6f, r.y, r.w - radius * 1.2f, 1.5f}, alpha(0x2effffffu, a));
    }
}

void glass_flat(const gfx::Rect &r, float radius, float a)
{
    if (a <= 0.01f)
        return;
    gfx::fill(r, alpha(0x5c1a1a22u, a), radius);
    gfx::fill_vgradient(r, alpha(0x1effffffu, a), 0x00000000u, radius);
    gfx::rim(r, radius, 0.7f * a);
}

float Lifts::step(const std::string &key, bool focused, float dt, bool *animating)
{
    Anim &l = m_lift[key];
    l.to(focused ? 1.f : 0.f);
    if (l.step(dt, 14.f) && animating)
        *animating = true;
    return l.value;
}

float row_scroll(Anim &a, float row_w, float room, float c, float dt, bool *animating)
{
    if (row_w <= room) {
        a.snap(0);
        return 0;
    }
    a.to(std::max(0.f, std::min(row_w - room, c - room / 2)));
    if (a.step(dt, 12.f) && animating)
        *animating = true;
    return a.value;
}

} // namespace ui
