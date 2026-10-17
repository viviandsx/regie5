/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/whats_new_sheet.h"

#include "core/i18n.h"
#include "core/whats_new.h"
#include "ui/format.h"
#include "ui/style.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

constexpr float kPanelW = 1240, kIn = 80;
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonOn{gfx::Bold, 28};
constexpr uint32_t kAccent = 0xff7c8cffu;   /* the wordmark's "5" */

} // namespace

void WhatsNewSheet::open()
{
    m_open = true;
    m_show.snap(0.f);
    m_show.to(1.f);
    m_drop.hide();
}

void WhatsNewSheet::input(const input::State &in)
{
    if (in.pressed & (input::Cross | input::Circle))
        m_open = false;
}

void WhatsNewSheet::draw(float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 10.f))
        *animating = true;
    const float a = smoothstep(m_show.value);

    /* The lines this version brings: up to five; "-" stands for none (every
     * key needs a text in every language). A line that names Jellyfin is for
     * Jellyfin only: left out when the media server is Emby or Plex. */
    std::vector<const char *> items;
    for (i18n::S k : {i18n::WnItem1, i18n::WnItem2, i18n::WnItem3, i18n::WnItem4, i18n::WnItem5}) {
        const std::string text = i18n::plain(k);
        if (text != "-" && !(i18n::media() != config::MediaServer::Jellyfin && text.find("Jellyfin") != std::string::npos))
            items.push_back(i18n::tr(k));
    }
    const float w = kPanelW - 2 * kIn;
    float h = kIn + 70 + 40;
    std::vector<float> item_h;
    for (const char *t : items) {
        const int lines = std::clamp((int)(gfx::text_width(t, {gfx::Regular, 32}) / (w - 50)) + 1, 1, 3);
        item_h.push_back(lines * 44.f + 26);
        h += item_h.back();
    }
    h += 30 + kButtonH + kIn;

    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.55f * a));
    const gfx::Rect r{(gfx::W - kPanelW) / 2, (gfx::H - h) / 2 + (1 - a) * 40, kPanelW, h};
    glass_panel(r, 44, a, true);
    gfx::push_opacity(a);
    const float x = r.x + kIn;
    float y = r.y + kIn + 40;
    const std::string series = whats_new::series(m_ctx.version);
    gfx::text(x, y, fmt::f(i18n::tr(i18n::WnTitle), series.empty() ? m_ctx.version.c_str() : series.c_str()),
              {gfx::Bold, 52, w}, kText);
    y += 70;
    for (size_t i = 0; i < items.size(); i++) {
        y += 40;
        gfx::fill({x + 4, y - 18, 14, 14}, kAccent, 7);
        gfx::text(x + 50, y, items[i], {gfx::Regular, 32, w - 50, 3, 44}, kText);
        y += item_h[i] - 40;
    }
    const float bw = 200;
    const gfx::Rect bar{x, r.y + r.h - kIn - kButtonH - kBarPad, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
    glass_panel(bar, bar.h / 2, a, false);
    gfx::pop_opacity();
    const gfx::Rect button{bar.x + kBarPad, bar.y + kBarPad, bw, kButtonH};
    m_drop.to(button, 0, 0, 0);
    m_drop.draw(dt, a, animating);
    gfx::text(button.x + bw / 2, button.y + 42, i18n::tr(i18n::WnOk), kButtonOn, alpha(kText, a), 1);
}

} // namespace ui
