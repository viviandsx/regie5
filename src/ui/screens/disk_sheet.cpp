/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/disk_sheet.h"

#include "core/disks.h"
#include "core/i18n.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kW = 1500, kH = 860;
constexpr float kX = (gfx::W - kW) / 2, kY = (gfx::H - kH) / 2 + 24;
constexpr float kButtonH = 62;

uint32_t state_color(disks::State s)
{
    return s == disks::State::Passed ? kGreen : s == disks::State::Unknown ? 0xff8e8e93u : kRed;
}

uint32_t heat_color(disks::Heat h) { return h == disks::Heat::Hot ? kRed : h == disks::Heat::Warm ? kOrange : kText; }

} // namespace

void DiskSheet::open(const std::string &key)
{
    m_open = true;
    m_key = key;
    m_show.snap(0.f);
    m_show.to(1.f);
}

void DiskSheet::input(const input::State &in)
{
    if (in.pressed & (input::Circle | input::Cross))
        m_open = false;
}

void DiskSheet::draw(const model::Disk &d, int threshold, const Load<model::DiskDetail> &detail, double now,
                     float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 12.f))
        *animating = true;
    const float a = smoothstep(m_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(kBg, 0.55f * a));
    gfx::push_opacity(a);
    const float y0 = kY + (1.f - a) * 30;
    glass_panel({kX, y0, kW, kH}, 36, 1.f, true);
    const float x = kX + 56, w = kW - 112, c1 = x + 380;
    const time_t now_t = time(nullptr);

    /* Which disk, and its state on the right. */
    const disks::State st = disks::state(d, threshold);
    const std::string state = fmt::disk_state(st);
    const float sw = chip_width(state, 24);
    draw_chip(x + w - sw, y0 + 52, state, state_color(st), 1.f, 24);
    gfx::text(x, y0 + 84, disks::name(d), {gfx::Bold, 44, w - sw - 40}, kText);
    std::string what = fmt::disk_kind(d);
    if (d.capacity > 0)
        what += " · " + fmt::size(d.capacity);
    if (!d.label.empty() && !d.device.empty())
        what += " · " + d.device;
    gfx::text(x, y0 + 124, what, {gfx::Regular, 24, w}, kText2);

    const gfx::TextStyle head{gfx::SemiBold, 28}, cell{gfx::Medium, 26, w - 380};
    float y = y0 + 196;
    auto line = [&](i18n::S label, const std::string &value, uint32_t ink = kText2) {
        gfx::fill({x, y - 36, w, 1}, alpha(0xffffffffu, 0.07f));
        gfx::text(x, y, i18n::tr(label), head, kText);
        gfx::text(c1, y, value, cell, ink);
        y += 52;
    };

    /* What is wrong, in words. */
    std::string state_line = state;
    for (const auto &al : disks::alerts(d, threshold, now_t))
        if (al.kind == disks::Alert::Stale)
            state_line += " · " + fmt::disk_alert(al, d, now_t);
    line(i18n::DiskState, state_line, st == disks::State::Passed || st == disks::State::Unknown ? kText2 : 0xffff8a80u);

    /* The temperature against Régie5's thresholds (Scrutiny has none). */
    const disks::Heat heat = disks::heat(d);
    const bool nvme = d.kind == model::Disk::Nvme;
    line(i18n::DiskTemperature, d.temp > 0 ? fmt::f("%d °C", d.temp) : "?", heat_color(heat));
    gfx::text(c1, y - 18, fmt::f(i18n::tr(i18n::DiskHeatNote), nvme ? 70 : 50, nvme ? 80 : 60), {gfx::Regular, 20, w - 380},
              kText3);
    y += 18;
    if (d.hours > 0)
        line(i18n::DiskHours, fmt::f(i18n::tr(i18n::DiskHoursValue), fmt::grouped(d.hours).c_str()));
    if (detail.ready() && detail.value.power_cycles > 0)
        line(i18n::DiskCycles, fmt::grouped(detail.value.power_cycles));
    if (d.collected > 0)
        line(i18n::DiskLastReading, fmt::day(d.collected, now_t) + " · " + fmt::clock(d.collected) + " · " +
                                        fmt::ago(d.collected, now_t),
             now_t - d.collected > disks::kStale ? kOrange : kText2);

    /* The last 7 days' temperatures: a bar a day, its value above. */
    gfx::fill({x, y - 36, w, 1}, alpha(0xffffffffu, 0.07f));
    gfx::text(x, y, i18n::tr(i18n::DiskWeek), head, kText);
    const gfx::Rect chart{c1, y - 30, std::min(w - 380, 7 * 96.f), 110};
    if (detail.ready() && !detail.value.temps.empty()) {
        const auto &t = detail.value.temps;
        const size_t first = t.size() > 7 ? t.size() - 7 : 0;
        int lo = 200, hi = 0;
        for (size_t i = first; i < t.size(); i++)
            lo = std::min(lo, t[i].second), hi = std::max(hi, t[i].second);
        const float slot = chart.w / 7;
        for (size_t i = first; i < t.size(); i++) {
            model::Disk at = d;
            at.temp = t[i].second;
            const float k = hi > lo ? (float)(t[i].second - lo) / (hi - lo) : 1.f;
            const float bh = 18 + k * (chart.h - 52);
            const float bx = chart.x + (i - first) * slot;
            const uint32_t c = disks::heat(at) == disks::Heat::Ok ? 0xff64d2ffu : heat_color(disks::heat(at));
            gfx::fill({bx + 14, chart.y + chart.h - bh, slot - 28, bh}, alpha(c, 0.75f), 6);
            gfx::text(bx + slot / 2, chart.y + chart.h - bh - 10, std::to_string(t[i].second), {gfx::Medium, 20}, kText2, 1);
            gfx::text(bx + slot / 2, chart.y + chart.h + 28, fmt::weekday(t[i].first), {gfx::Regular, 18}, kText3, 1);
        }
    } else if (detail.state == load::Failed) {
        gfx::text(c1, y, fmt::labelled(config::service_name(config::Scrutiny), fmt::error(detail.error, detail.http_status)),
                  {gfx::Regular, 24, w - 380}, 0xffff8a80u);
    } else if (!detail.ready()) {
        draw_skeleton(chart, 12, now, animating);
    }
    y += chart.h + 40;

    /* The attributes that are not fine, named by the app. */
    gfx::fill({x, y - 36, w, 1}, alpha(0xffffffffu, 0.07f));
    gfx::text(x, y, i18n::tr(i18n::DiskAttributes), head, kText);
    const float limit = y0 + kH - kButtonH - 60;
    if (detail.ready()) {
        if (detail.value.attributes.empty())
            gfx::text(c1, y, i18n::tr(i18n::DiskAttrsNone), cell, kText2);
        for (const auto &at : detail.value.attributes) {
            if (y > limit)
                break;
            const bool failed = (at.status & 5) != 0;
            const i18n::S named = disks::attribute_name(at.id);
            const std::string name = named == i18n::Count ? fmt::f(i18n::tr(i18n::DiskAttrOther), at.id.c_str())
                                                          : std::string(i18n::tr(named));
            gfx::fill({c1, y - 14, 12, 12}, failed ? kRed : kOrange, 6);
            const float nw = gfx::text(c1 + 26, y, name, {gfx::Medium, 26, w - 380 - 420}, kText);
            gfx::text(c1 + 26 + nw + 16, y, fmt::f(i18n::tr(i18n::DiskAttrRaw), fmt::grouped(at.raw).c_str()),
                      {gfx::Regular, 22}, kText3);
            gfx::text(x + w, y, i18n::tr(failed ? i18n::DiskAttrFailed : i18n::DiskAttrWarn), {gfx::Medium, 24},
                      failed ? 0xffff8a80u : kOrange, 2);
            y += 44;
        }
    } else if (detail.state != load::Failed) {
        draw_skeleton({c1, y - 30, w - 380, 40}, 10, now, animating);
    }

    /* Close, alone at the right. */
    const std::string label = i18n::tr(i18n::Close);
    const float bw = gfx::text_width(label, {gfx::SemiBold, 28}) + 80;
    const float by = y0 + kH - kButtonH - 40;
    const gfx::Rect b{x + w - bw, by, bw, kButtonH};
    glass_panel({b.x - 6, by - 6, bw + 12, kButtonH + 12}, (kButtonH + 12) / 2, 1.f, false);
    m_drop.to(b, 0);
    m_drop.draw(dt, 1.f, animating);
    gfx::text(b.x + bw / 2, by + 41, label, {gfx::Bold, 28}, kText, 1);
    gfx::pop_opacity();
}

} // namespace ui
