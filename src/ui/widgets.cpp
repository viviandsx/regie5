/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/widgets.h"

#include "core/i18n.h"
#include "ui/hints.h"
#include "ui/style.h"

#include <algorithm>
#include <cmath>

namespace ui {

void draw_skeleton(const gfx::Rect &r, float radius, double now, bool *animating)
{
    const float breathe = 0.55f + 0.25f * (float)std::sin(now * 3.0);
    glass_flat(r, radius, breathe);
    if (animating)
        *animating = true;
}

void draw_empty(const gfx::Rect &r, const std::string &title, const std::string &detail, bool error)
{
    glass_flat(r, 24, 0.85f);
    const float cy = r.y + r.h / 2;
    const uint32_t ink = error ? 0xffff8a80u : kText2;
    if (detail.empty()) {
        gfx::text(r.x + r.w / 2, cy + 11, title, {gfx::Medium, 30, r.w - 80}, ink, 1);
    } else {
        gfx::text(r.x + r.w / 2, cy - 6, title, {gfx::SemiBold, 30, r.w - 80}, ink, 1);
        gfx::text(r.x + r.w / 2, cy + 36, detail, {gfx::Regular, 24, r.w - 80}, kText3, 1);
    }
}

namespace {
constexpr float kPillH = 60, kPillPad = 6;
const gfx::TextStyle kPillText{gfx::SemiBold, 26};
float pill_width(const std::string &s) { return gfx::text_width(s, kPillText) + 64; }
} // namespace

float pills_width(const std::vector<std::string> &labels)
{
    float w = kPillPad;
    for (const auto &l : labels)
        w += pill_width(l) + kPillPad;
    return w;
}

std::vector<gfx::Rect> draw_pills(float x, float y, const std::vector<std::string> &labels, int chosen, int on,
                                  bool focus, Drop &drop, float dt, bool *animating, bool right_aligned)
{
    const float total = pills_width(labels);
    if (right_aligned)
        x -= total;
    glass_panel({x, y, total, kPillH + 2 * kPillPad}, (kPillH + 2 * kPillPad) / 2, 1.f, false);
    std::vector<gfx::Rect> rects;
    float px = x + kPillPad;
    for (const auto &l : labels) {
        rects.push_back({px, y + kPillPad, pill_width(l), kPillH});
        px += pill_width(l) + kPillPad;
    }
    if (on >= 0 && on < (int)rects.size()) {
        drop.to(rects[on], on);
        drop.draw(dt, focus ? 1.f : 0.35f, animating);
    }
    for (int i = 0; i < (int)labels.size(); i++) {
        const bool lit = i == chosen || (focus && i == on);
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 40, labels[i],
                  lit ? gfx::TextStyle{gfx::Bold, 26} : kPillText, lit ? kText : kText2, 1);
    }
    return rects;
}

float chip_width(const std::string &label, float size) { return gfx::text_width(label, {gfx::SemiBold, size}) + size * 2.3f; }

float draw_chip(float x, float y, const std::string &label, uint32_t dot, float a, float size)
{
    const float w = chip_width(label, size), h = size * 1.75f;
    gfx::fill({x, y, w, h}, alpha(0xb8141418u, a), h / 2);
    gfx::rim({x, y, w, h}, h / 2, 0.35f * a);
    const float d = size * 0.42f;
    gfx::fill({x + size * 0.62f, y + h / 2 - d / 2, d, d}, alpha(dot, a), d / 2);
    gfx::text(x + size * 1.35f, y + h / 2 + size * 0.36f, label, {gfx::SemiBold, size}, alpha(kText, a));
    return w;
}

void draw_progress(const gfx::Rect &r, float v, uint32_t color, float a)
{
    v = std::clamp(v, 0.f, 1.f);
    gfx::fill(r, alpha(0x33ffffffu, a), r.h / 2);
    if (v > 0)
        gfx::fill({r.x, r.y, std::max(r.h, r.w * v), r.h}, alpha(color, a), r.h / 2);
}

/* ---- notes ---- */

namespace {
struct Note {
    std::string text;
    bool ok = true;
    double at = -100;
    double show = 3.0;
};
Note s_note;
} // namespace

void notify(const std::string &text, bool ok, double now, double seconds) { s_note = {text, ok, now, seconds}; }

void draw_notes(double now, bool *animating)
{
    const double age = now - s_note.at;
    const double kShow = s_note.show;
    if (age < 0 || age > kShow)
        return;
    if (animating)
        *animating = true;
    const float in = (float)std::min(1.0, age / 0.25), out = (float)std::min(1.0, (kShow - age) / 0.4);
    const float a = smoothstep(std::min(in, out));
    const gfx::TextStyle st{gfx::SemiBold, 28};
    const float w = gfx::text_width(s_note.text, st) + 110, h = 72;
    const gfx::Rect r{(gfx::W - w) / 2, 128 - (1 - a) * 30, w, h};
    glass_panel(r, h / 2, a, true);
    gfx::fill({r.x + 34, r.y + h / 2 - 8, 16, 16}, alpha(s_note.ok ? kGreen : kRed, a), 8);
    gfx::text(r.x + 66, r.y + h / 2 + 10, s_note.text, st, alpha(kText, a));
}

/* ---- the sheet ---- */

namespace {
const gfx::TextStyle kSheetButton{gfx::SemiBold, 28};
float sheet_button_w(const std::string &s) { return gfx::text_width(s, kSheetButton) + 80; }
} // namespace

void Sheet::open(std::string title, std::string body, std::vector<std::string> buttons, int focus,
                 std::function<void(int)> chosen, bool danger_last)
{
    m_open = true;
    m_title = std::move(title);
    m_body = std::move(body);
    m_buttons = std::move(buttons);
    m_focus = std::clamp(focus, 0, (int)m_buttons.size() - 1);
    m_chosen = std::move(chosen);
    m_danger_last = danger_last;
    m_show.snap(0.f);
    m_show.to(1.f);
    m_drop.hide();
}

void Sheet::input(const input::State &in)
{
    if (!m_open)
        return;
    const uint32_t p = in.pressed;
    int choice = -2;
    if (p & input::Left)
        m_focus = std::max(0, m_focus - 1);
    else if (p & input::Right)
        m_focus = std::min((int)m_buttons.size() - 1, m_focus + 1);
    else if (p & input::Cross)
        choice = m_focus;
    else if (p & input::Circle)
        choice = -1;
    if (choice == -2)
        return;
    m_open = false;   /* closed before the callback: it may open another */
    auto cb = std::move(m_chosen);
    m_chosen = nullptr;
    if (cb)
        cb(choice);
}

void Sheet::draw(float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 12.f) && animating)
        *animating = true;
    const float a = smoothstep(m_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.55f * a));

    const float w = 1040;
    float bw = 0;
    for (const auto &b : m_buttons)
        bw += sheet_button_w(b) + 12;
    bw = bw - 12 + 12;
    /* The body on as many lines as it needs, up to four: the sheet grows with it. */
    const float body_w = w - 140;
    const int lines = m_body.empty() ? 0
                                     : std::min(4, 1 + (int)(gfx::text_width(m_body, {gfx::Regular, 28}) / (body_w * 0.92f)));
    const float title_extra = gfx::text_width(m_title, {gfx::Bold, 40}) > (w - 120) * 0.95f ? 50.f : 0.f;
    const float h = (m_body.empty() ? 300 : 360 + std::max(0, lines - 2) * 38) + title_extra;
    const gfx::Rect r{(gfx::W - w) / 2, (gfx::H - h) / 2 + (1 - a) * 40, w, h};
    glass_panel(r, 40, a, true);
    gfx::text(r.x + w / 2, r.y + 92, m_title, {gfx::Bold, 40, w - 120, 2, 50}, alpha(kText, a), 1);
    if (!m_body.empty())
        gfx::text(r.x + w / 2, r.y + 154 + title_extra, m_body, {gfx::Regular, 28, body_w, 4, 38}, alpha(kText2, a),
                  1);

    const float by = r.y + h - 118;
    const gfx::Rect bar{r.x + (w - bw) / 2, by, bw, 76};
    glass_panel(bar, 38, a, false);
    float x = bar.x + 6;
    gfx::Rect on{};
    for (int i = 0; i < (int)m_buttons.size(); i++) {
        const float bwi = sheet_button_w(m_buttons[i]);
        if (i == m_focus)
            on = {x, by + 6, bwi, 64};
        x += bwi + 12;
    }
    m_drop.to(on, m_focus);
    m_drop.draw(dt, a, animating);
    x = bar.x + 6;
    for (int i = 0; i < (int)m_buttons.size(); i++) {
        const float bwi = sheet_button_w(m_buttons[i]);
        const bool f = i == m_focus;
        const bool danger = m_danger_last && i == (int)m_buttons.size() - 1;
        const uint32_t ink = danger ? 0xffff8a80u : f ? kText : kText2;
        gfx::text(x + bwi / 2, by + 6 + 42, m_buttons[i], f ? gfx::TextStyle{gfx::Bold, 28} : kSheetButton,
                  alpha(ink, a), 1);
        x += bwi + 12;
    }
}

void ListSheet::open(std::string title, std::vector<Item> items, std::string empty_text,
                     std::function<void(int)> chosen)
{
    m_open = true;
    m_chosen = std::move(chosen);
    m_title = std::move(title);
    m_items = std::move(items);
    m_empty = std::move(empty_text);
    m_focus = 0;
    m_show.snap(0.f);
    m_show.to(1.f);
    m_scroll.snap(0.f);
    m_drop.hide();
}

void ListSheet::update(std::vector<Item> items)
{
    m_items = std::move(items);
    m_focus = std::min(m_focus, std::max(0, (int)m_items.size() - 1));
}

bool ListSheet::any_action() const
{
    for (const Item &it : m_items)
        if (!it.action.empty())
            return true;
    return false;
}

void ListSheet::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    if (p & (input::Circle | input::Cross)) {
        m_open = false;
        if ((p & input::Cross) && m_chosen && m_focus < (int)m_items.size() &&
            (!any_action() || !m_items[m_focus].action.empty())) {
            std::function<void(int)> chosen = std::move(m_chosen);
            chosen(m_focus);   /* may open another sheet */
        }
        return;
    }
    if (p & input::Up)
        m_focus = std::max(0, m_focus - 1);
    else if (p & input::Down)
        m_focus = std::min((int)m_items.size() - 1, m_focus + 1);
}

void ListSheet::draw(float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 12.f) && animating)
        *animating = true;
    const float a = smoothstep(m_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.55f * a));

    constexpr float kW = 1360, kRow = 142, kGap = 10, kHead = 120, kFoot = 84;
    const int n = (int)m_items.size();
    const float list_h = n == 0 ? 120 : n * (kRow + kGap) - kGap;
    const float view_h = std::min(list_h, 5 * (kRow + kGap) - kGap);
    const float h = kHead + view_h + kFoot;
    const gfx::Rect r{(gfx::W - kW) / 2, (gfx::H - h) / 2 + (1 - a) * 40, kW, h};
    glass_panel(r, 40, a, true);
    gfx::text(r.x + 56, r.y + 78, m_title, {gfx::Bold, 40, kW - 112}, alpha(kText, a));

    const float top = r.y + kHead;
    if (n == 0) {
        gfx::text(r.x + kW / 2, top + 70, m_empty, {gfx::Medium, 30, kW - 120}, alpha(kText2, a), 1);
    } else {
        const float scroll = list_scroll(m_scroll, m_focus * (kRow + kGap), kRow, view_h, list_h, dt, animating);
        gfx::push_scissor({r.x, top - 6, kW, view_h + 12});
        for (int i = 0; i < n; i++)
            glass_flat({r.x + 36, top + i * (kRow + kGap) - scroll, kW - 72, kRow}, 22, 0.8f * a);
        m_drop.to({r.x + 36, top + m_focus * (kRow + kGap) - scroll, kW - 72, kRow}, m_focus, 0, -scroll);
        m_drop.draw(dt, a, animating, 22);
        for (int i = 0; i < n; i++) {
            const Item &it = m_items[i];
            const float y = top + i * (kRow + kGap) - scroll;
            if (y + kRow < top - 6 || y > top + view_h + 6)
                continue;
            float x = r.x + 64;
            for (const auto &c : it.chips)
                x += draw_chip(x, y + 18, c.first, c.second, a, 20) + 10;
            gfx::text(x + 8, y + 46, it.text, {gfx::SemiBold, 28, r.x + kW - 64 - x - 8, 2, 36}, alpha(kText, a));
            if (!it.detail.empty())
                gfx::text(r.x + 64, y + 122, it.detail, {gfx::Regular, 22, kW - 128}, alpha(kText3, a));
        }
        gfx::pop_scissor();
    }
    const std::string action = m_focus < n ? m_items[m_focus].action : std::string();
    if (m_chosen && !action.empty())
        draw_pad_hints(r.x + kW / 2, r.y + h - 42, {{PadButton::Cross, action}, {PadButton::Circle, i18n::tr(i18n::Close)}},
                       1, 28.f, a);
    else if (m_chosen && !any_action())
        draw_pad_hints(r.x + kW / 2, r.y + h - 42,
                       {{PadButton::Cross, i18n::tr(i18n::HintSelect)}, {PadButton::Circle, i18n::tr(i18n::Cancel)}}, 1,
                       28.f, a);
    else
        draw_pad_hints(r.x + kW / 2, r.y + h - 42, {{PadButton::Circle, i18n::tr(i18n::Close)}}, 1, 28.f, a);
}

float list_scroll(Anim &a, float focus_y, float focus_h, float view_h, float list_h, float dt, bool *animating)
{
    /* A low view (a series' seasons under a long sheet) gets a smaller
     * margin: 120 on both sides pushed the focused row out of it. */
    const float margin = std::clamp((view_h - focus_h) / 2, 0.f, 120.f);
    float t = a.target;
    if (focus_y - t < margin)
        t = focus_y - margin;
    if (focus_y + focus_h - t > view_h - margin)
        t = focus_y + focus_h - view_h + margin;
    t = std::clamp(t, 0.f, std::max(0.f, list_h - view_h));
    a.to(t);
    if (a.step(dt, 12.f) && animating)
        *animating = true;
    return a.value;
}

} // namespace ui
