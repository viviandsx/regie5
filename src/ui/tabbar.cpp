/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/tabbar.h"

#include "core/i18n.h"
#include "ui/style.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace ui {

const char *tab_label(int tab)
{
    static const i18n::S keys[TabCount] = {i18n::TabHome,     i18n::TabLibrary,  i18n::TabDownloads,
                                           i18n::TabRequests, i18n::TabJellyfin, i18n::TabSettings};
    return tab >= 0 && tab < TabCount ? i18n::tr(keys[tab]) : "";
}

namespace {

/* A gear from discs: eight teeth round a wheel, a hole in the middle. */
void draw_gear(float cx, float cy, float r, uint32_t ink, uint32_t hole)
{
    const float tooth = r * 0.34f;
    for (int k = 0; k < 8; k++) {
        const float ang = k * 0.7853982f;
        const float tx = cx + std::cos(ang) * r * 0.82f, ty = cy + std::sin(ang) * r * 0.82f;
        gfx::fill({tx - tooth / 2, ty - tooth / 2, tooth, tooth}, ink, tooth * 0.3f);
    }
    gfx::fill({cx - r * 0.78f, cy - r * 0.78f, r * 1.56f, r * 1.56f}, ink, r * 0.78f);
    gfx::fill({cx - r * 0.32f, cy - r * 0.32f, r * 0.64f, r * 0.64f}, hole, r * 0.32f);
}

uint32_t tone_color(badges::Tone t)
{
    switch (t) {
    case badges::Attention: return 0xff7c8cffu;   /* the wordmark's blue: someone waits */
    case badges::Live: return kGreen;
    case badges::Warning: return kOrange;
    case badges::Problem: return kRed;
    case badges::Neutral: break;
    }
    return 0xff5a5a6au;
}

} // namespace

void TabBar::draw_badge(float x, float y, int tab, const badges::Badge &b, float a, float dt, bool *animating)
{
    Anim &in = m_badge_in[tab];
    if (b.shown())
        m_badge_last[tab] = b;
    in.to(b.shown() ? 1.f : 0.f);
    if (in.step(dt, 9.f))
        *animating = true;
    if (in.value < 0.01f || !m_badge_last[tab].shown())
        return;
    const badges::Badge &show = m_badge_last[tab];
    /* A little overshoot as it appears (an eased back curve), none as it goes. */
    const float t = in.value, grow = in.target > 0.5f ? 1.f + 0.18f * std::sin(t * 3.14159f) : 1.f;
    const std::string text = badges::label(show.count);
    /* The text keeps one size (a glyph atlas entry per size): only the pill grows. */
    const gfx::TextStyle st{gfx::Bold, 19};
    const float w0 = std::max(28.f, gfx::text_width(text, st) + 16);
    const float h = 28 * grow, w = w0 + (h - 28);
    const gfx::Rect r{x + 14 - w / 2, y + 14 - h / 2, w, h};
    const float op = a * smoothstep(t);
    gfx::shadow(r, h / 2, 8, 0.45f * op, 0);
    gfx::fill(r, alpha(tone_color(show.tone), op), h / 2);
    gfx::text(x + 14, y + 14 + 7, text, st, alpha(kText, op), 1);
}

void TabBar::draw(float a, int active, bool focused, const badges::Badge *badges, float dt, bool *animating)
{
    if (a <= 0.01f)
        return;
    const float cy = 66;
    constexpr int kPill = TabSettings;   /* the tabs on the pill; Settings is the gear */

    /* The wordmark. */
    const float bw = gfx::text(kPad, cy + 12, "Régie", {gfx::Bold, 36}, alpha(kText, a));
    gfx::text(kPad + bw, cy + 12, "5", {gfx::Bold, 36}, alpha(0xff7c8cffu, a));

    /* The clock, and the gear before it. */
    const time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char clock[8];
    std::snprintf(clock, sizeof clock, "%02d:%02d", tm.tm_hour, tm.tm_min);
    const float clock_w = gfx::text(gfx::W - kPad, cy + 11, clock, {gfx::SemiBold, 30}, alpha(kText, a), 2);
    const gfx::Rect gear{gfx::W - kPad - clock_w - 36 - 74, cy - 37, 74, 74};
    glass_panel(gear, 37, a, false);

    /* The tab pill, centred. */
    const gfx::TextStyle st{gfx::SemiBold, 25};
    float widths[kPill], total = 14;
    for (int i = 0; i < kPill; i++) {
        widths[i] = gfx::text_width(tab_label(i), st) + 68;
        total += widths[i] + 6;
    }
    total -= 6;
    const float px = (gfx::W - total) / 2;
    glass_panel({px, cy - 37, total, 74}, 37, a, false);
    /* The labels first: the drop is a lens over them. */
    float x = px + 7;
    gfx::Rect on{gear.x + 4, gear.y + 4, gear.w - 8, gear.h - 8};
    for (int i = 0; i < kPill; i++) {
        if (i == active)
            on = {x - 4, cy - 33, widths[i] + 8, 66};
        gfx::text(x + widths[i] / 2, cy + 9, tab_label(i), st, alpha(i == active ? kText : kText2, a), 1);
        /* The counter sits on the label's top right corner, in the tab's margin. */
        draw_badge(x + widths[i] / 2 + (widths[i] - 68) / 2 - 2, cy - 30, i, badges ? badges[i] : badges::Badge{}, a, dt,
                   animating);
        x += widths[i] + 6;
    }
    m_drop.to(on, active, 0, cy);
    m_drop.draw(dt, a * (focused ? 1.f : 0.35f), animating);
    /* The open tab's label crisp on top, riding the drop; the gear over its glass. */
    if (active < kPill) {
        x = px + 7;
        for (int i = 0; i < active; i++)
            x += widths[i] + 6;
        gfx::text(x + widths[active] / 2, cy + 9, tab_label(active), {gfx::Bold, 25}, alpha(kText, a), 1);
    }
    const uint32_t ink = alpha(active == TabSettings ? kText : kText2, a);
    draw_gear(gear.x + gear.w / 2, gear.y + gear.h / 2, 15, ink, alpha(active == TabSettings ? 0xff3a3a44u : 0xff26262eu, a));
}

} // namespace ui
