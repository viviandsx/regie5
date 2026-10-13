/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jelly5's draw_pad_hint (app/src/ui/screen.cpp, GPL-3.0-or-later).
 */
#include "ui/hints.h"

#include "ui/gfx.h"
#include "ui/style.h"

#include <algorithm>

namespace ui {

namespace {
bool shoulder(PadButton b)
{
    return b == PadButton::L1 || b == PadButton::R1 || b == PadButton::L2 || b == PadButton::R2;
}
} // namespace

float draw_pad_hint(float x, float cy, PadButton b, const std::string &label, float size, float a)
{
    const uint32_t disc = alpha(0xff3a3a42u, a), ink = alpha(0xfff5f5f7u, a);
    const float d = size, r = d / 2, st = std::max(2.f, d * 0.09f);   /* disc, radius, stroke */
    float w = d;
    switch (b) {
    case PadButton::Cross: {
        gfx::fill({x, cy - r, d, d}, disc, r);
        const int n = 10;
        const float s = d * 0.46f;
        for (int i = 0; i <= n; i++) {   /* two strokes from small squares */
            const float t = (float)i / n * s;
            gfx::fill({x + r - s / 2 + t - st / 2, cy - s / 2 + t - st / 2, st, st}, ink, st / 2);
            gfx::fill({x + r + s / 2 - t - st / 2, cy - s / 2 + t - st / 2, st, st}, ink, st / 2);
        }
        break;
    }
    case PadButton::Circle: {
        gfx::fill({x, cy - r, d, d}, disc, r);
        const float o = d * 0.52f, i = o - 2 * st;
        gfx::fill({x + r - o / 2, cy - o / 2, o, o}, ink, o / 2);
        gfx::fill({x + r - i / 2, cy - i / 2, i, i}, disc, i / 2);
        break;
    }
    case PadButton::Triangle: {
        gfx::fill({x, cy - r, d, d}, disc, r);
        auto tri = [&](float h, uint32_t c) {   /* a filled triangle from rows */
            const int n = (int)(h / 1.2f);
            for (int k = 0; k < n; k++) {
                const float row = (float)k / n, ww = h * 1.15f * row;
                gfx::fill({x + r - ww / 2, cy - h * 0.55f + row * h, std::max(1.f, ww), h / n + 0.6f}, c);
            }
        };
        tri(d * 0.5f, ink);
        tri(d * 0.5f - 2.6f * st, disc);
        break;
    }
    case PadButton::Square: {
        gfx::fill({x, cy - r, d, d}, disc, r);
        const float o = d * 0.44f, i = o - 2 * st;
        gfx::fill({x + r - o / 2, cy - o / 2, o, o}, ink, 1.5f);
        gfx::fill({x + r - i / 2, cy - i / 2, i, i}, disc, 1.f);
        break;
    }
    case PadButton::Options: {   /* a pill with three lines */
        w = d * 1.5f;
        gfx::fill({x, cy - r, w, d}, disc, r);
        const float lw = d * 0.5f;
        for (int k = -1; k <= 1; k++)
            gfx::fill({x + w / 2 - lw / 2, cy + k * d * 0.17f - st / 2, lw, st}, ink, st / 2);
        break;
    }
    case PadButton::L1:
    case PadButton::R1:
    case PadButton::L2:
    case PadButton::R2: {
        const char *t = b == PadButton::L1 ? "L1" : b == PadButton::R1 ? "R1" : b == PadButton::L2 ? "L2" : "R2";
        const gfx::TextStyle ts{gfx::Bold, d * 0.5f};
        w = gfx::text_width(t, ts) + d * 0.7f;
        gfx::fill({x, cy - r, w, d}, disc, r);
        gfx::text(x + w / 2, cy + d * 0.18f, t, ts, ink, 1);
        break;
    }
    }
    if (label.empty())
        return w;
    return w + 10 + gfx::text(x + w + 10, cy + d * 0.28f, label, {gfx::Medium, d * 0.72f}, alpha(kText2, a));
}

float pad_hint_width(PadButton b, const std::string &label, float size)
{
    float w = size;
    if (b == PadButton::Options)
        w = size * 1.5f;
    else if (shoulder(b))
        w = gfx::text_width("L1", {gfx::Bold, size * 0.5f}) + size * 0.7f;
    return label.empty() ? w : w + 10 + gfx::text_width(label, {gfx::Medium, size * 0.72f});
}

float draw_pad_hints(float x, float cy, const std::vector<PadHint> &hints, int align, float size, float a)
{
    const float gap = size * 0.9f;
    float total = 0;
    for (size_t i = 0; i < hints.size(); i++)
        total += pad_hint_width(hints[i].button, hints[i].label, size) + (i ? gap : 0);
    float hx = align == 1 ? x - total / 2 : align == 2 ? x - total : x;
    for (const PadHint &h : hints)
        hx += draw_pad_hint(hx, cy, h.button, h.label, size, a) + gap;
    return total;
}

} // namespace ui
