/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/guide.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kTop = 300;              /* the day names' row */
constexpr float kCardsTop = kTop + 40;   /* the first card */
constexpr float kGap = 12;
constexpr float kColW = (gfx::W - 2 * kPad - 6 * kGap) / 7;
constexpr float kCardH = 136, kCardGap = 10;
constexpr float kThumbW = 58, kThumbH = 87;

uint32_t state_color(schedule::State s)
{
    return s == schedule::Available   ? kGreen
           : s == schedule::Missing   ? kRed
           : s == schedule::InCinemas ? 0xffbf5af2u
                                      : 0xff0a84ffu;
}

const char *state_label(schedule::State s)
{
    return i18n::tr(s == schedule::Available ? i18n::GuideAvailable
                    : s == schedule::Missing   ? i18n::GuideMissing
                    : s == schedule::InCinemas ? i18n::GuideInCinemas
                                               : i18n::GuideUpcoming);
}

/* What a card says under its title: the episode and its time, or the kind of release. */
std::string card_line(const model::CalendarItem &c)
{
    if (c.from == config::Sonarr)
        return fmt::episode(c.season, c.episode) + " · " + fmt::clock(c.when);
    switch (c.release) {
    case model::CalendarItem::Cinema: return i18n::tr(i18n::ReleaseCinema);
    case model::CalendarItem::Digital: return i18n::tr(i18n::ReleaseDigital);
    case model::CalendarItem::Physical: return i18n::tr(i18n::ReleasePhysical);
    case model::CalendarItem::Air: break;
    }
    return {};
}

} // namespace

GuideView::GuideView(Context &ctx) : m_ctx(ctx) { m_monday = fmt::week_start(time(nullptr)); }

void GuideView::show_week_of(time_t t)
{
    const time_t monday = fmt::week_start(t);
    m_day = std::max(0, schedule::column_of(t, monday));
    if (monday != m_monday) {
        m_monday = monday;
        load(true);
    }
}

void GuideView::load(bool force)
{
    api::Services *svc = m_ctx.services;
    if (!svc)
        return;
    if (m_cal.empty())
        for (config::Service s : {config::Sonarr, config::Radarr})
            for (int i = 0; i < svc->instances(s); i++)
                m_cal.push_back({s, i, {}});
    if (!force && std::any_of(m_cal.begin(), m_cal.end(), [](const auto &p) { return p.load.state != load::Idle; }))
        return;
    const unsigned gen = ++m_gen;
    std::weak_ptr<bool> alive = m_alive;
    const time_t from = m_monday, to = m_monday + 7 * 86400 + 3600;
    for (size_t i = 0; i < m_cal.size(); i++) {
        m_cal[i].load.state = load::Loading;
        svc->calendar(
            m_cal[i].service, from, to,
            [this, alive, gen, i](api::Result<std::vector<model::CalendarItem>> r) {
                if (alive.expired() || gen != m_gen)
                    return;   /* another week was asked for since */
                m_cal[i].load.take(std::move(r), platform::now());
                rebuild();
            },
            m_cal[i].instance);
    }
    rebuild();
}

void GuideView::rebuild()
{
    const time_t now = time(nullptr);
    for (auto &d : m_days)
        d.clear();
    std::vector<model::CalendarItem> all;
    for (const auto &p : m_cal)
        if (p.load.ready())
            all.insert(all.end(), p.load.value.begin(), p.load.value.end());
    schedule::drop_duplicates(&all);   /* followed by two instances: once */
    for (const auto &item : all) {
        const int col = schedule::column_of(item.when, m_monday);
        if (col >= 0)   /* (Sonarr answers a little outside the range asked) */
            m_days[col].push_back({item, schedule::state_of(item, now)});
    }
    for (auto &d : m_days)
        std::stable_sort(d.begin(), d.end(), [](const Entry &a, const Entry &b) { return a.item.when < b.item.when; });
    for (int i = 0; i < 7; i++)
        m_index[i] = std::clamp(m_index[i], 0, std::max(0, (int)m_days[i].size() - 1));
}

const GuideView::Entry *GuideView::focused_entry() const
{
    const auto &d = m_days[m_day];
    return m_index[m_day] < (int)d.size() ? &d[m_index[m_day]] : nullptr;
}

Action GuideView::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    if (p & (input::L2 | input::R2)) {
        m_monday = fmt::week_start(m_monday + ((p & input::R2) ? 7 : -7) * 86400 + 12 * 3600);
        for (int &i : m_index)
            i = 0;
        load(true);
        return Action::None;
    }
    int &i = m_index[m_day];
    const int n = (int)m_days[m_day].size();
    if (p & input::Up) {
        if (i == 0)
            return Action::ToTabs;
        i--;
    } else if (p & input::Down) {
        if (i >= n - 1)
            return Action::Bump;
        i++;
    } else if (p & (input::Left | input::Right)) {
        const int d = m_day + ((p & input::Left) ? -1 : 1);
        if (d < 0 || d > 6)
            return Action::Bump;
        /* To the card at about the same height in the next day. */
        m_index[d] = std::min(i, std::max(0, (int)m_days[d].size() - 1));
        m_day = d;
    } else if (p & input::Cross) {
        if (const Entry *e = focused_entry()) {
            m_open = true;
            m_open_service = e->item.from;
            m_open_instance = e->item.instance;
            m_open_id = e->item.media_id;
        }
    }
    return Action::None;
}

bool GuideView::take_open(config::Service *s, int *instance, int *media_id)
{
    if (!m_open)
        return false;
    m_open = false;
    *s = m_open_service;
    *instance = m_open_instance;
    *media_id = m_open_id;
    return true;
}

std::vector<PadHint> GuideView::hints() const
{
    return {{PadButton::L2, ""}, {PadButton::R2, i18n::tr(i18n::HintWeek)}};
}

bool GuideView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    const Entry *e = focused_entry();
    if (!e)
        return false;
    *ref = {e->item.from, arr::mediacover_path(e->item.media_id, "fanart-360.jpg"), e->item.instance};
    *key = ref->path + std::to_string((int)e->item.from) + "." + std::to_string(e->item.instance);
    *seed = (int)(seed_of(e->item.title) % 997);
    return true;
}

void GuideView::draw(double now, float x0, bool focused, float dt, bool *animating)
{
    const time_t now_t = time(nullptr);
    const time_t this_week = fmt::week_start(now_t);

    /* Which week. */
    std::string which;
    if (m_monday == this_week)
        which = i18n::tr(i18n::GuideThisWeek);
    else if (std::llabs((long long)(m_monday - (this_week + 7 * 86400))) < 4 * 3600)
        which = i18n::tr(i18n::GuideNextWeek);
    else if (std::llabs((long long)(m_monday - (this_week - 7 * 86400))) < 4 * 3600)
        which = i18n::tr(i18n::GuideLastWeek);
    const std::string range = fmt::week_range(m_monday);
    gfx::text(x0, 262, which.empty() ? range : which + " · " + range, {gfx::Medium, 24}, kText3);

    /* Where the week's data stands. */
    bool loading = false, all_off = true;
    std::string failures;
    for (const auto &p : m_cal) {
        const auto &c = p.load;
        loading |= c.loading() || c.state == load::Idle;
        all_off &= c.state == load::Off;
        if (c.state == load::Failed)
            failures += (failures.empty() ? "" : " · ") +
                        fmt::labelled(m_ctx.services->config().at(p.service, p.instance).name,
                                      fmt::error(c.error, c.http_status));
    }
    const gfx::Rect area{x0, kCardsTop, gfx::W - 2 * kPad, 260};
    int total = 0;
    for (const auto &d : m_days)
        total += (int)d.size();
    if (all_off) {
        draw_empty(area, m_ctx.off_text(i18n::BothArrOff));
        return;
    }
    if (total == 0 && !failures.empty() && !loading) {
        draw_empty(area, failures, i18n::tr(i18n::Retry), true);
        return;
    }
    if (!failures.empty()) {   /* part of the week: say which service failed */
        const float w = gfx::text_width(which.empty() ? range : which + " · " + range, {gfx::Medium, 24});
        draw_chip(x0 + w + 18, 238, failures, kRed, 1.f, 22);
    }

    /* The days: a column each, today's lit. */
    const float view_h = gfx::H - 110 - kCardsTop;
    for (int d = 0; d < 7; d++) {
        const float cx = x0 + d * (kColW + kGap);
        const time_t day_t = m_monday + d * 86400 + 12 * 3600;
        const bool today = schedule::column_of(now_t, m_monday) == d;
        if (today)
            glass_flat({cx - 6, kTop - 34, kColW + 12, gfx::H - 110 - (kTop - 34)}, 18, 0.55f);
        gfx::text(cx + kColW / 2, kTop + 6, fmt::weekday(day_t), {today ? gfx::Bold : gfx::SemiBold, 24},
                  today ? kText : kText2, 1);
    }
    if (loading && total == 0) {
        for (int d = 0; d < 7; d++)
            for (int k = 0; k < 2; k++)
                draw_skeleton({x0 + d * (kColW + kGap), kCardsTop + k * (kCardH + kCardGap), kColW, kCardH}, 16,
                              now, animating);
        return;
    }
    if (total == 0) {
        draw_empty(area, i18n::tr(i18n::GuideEmpty));
        if (focused) {
            m_drop.to(area, -1);
            m_drop.draw(dt, 0.6f, animating, 24);
        }
        return;
    }

    /* The focused day scrolls to keep its card in view; the others stay where
     * they were. Panes first, then the drop, then what is on them. */
    float scroll[7];
    for (int d = 0; d < 7; d++) {
        const float list_h = m_days[d].size() * (kCardH + kCardGap);
        scroll[d] = d == m_day ? list_scroll(m_scroll[d], m_index[d] * (kCardH + kCardGap), kCardH, view_h, list_h,
                                             dt, animating)
                               : m_scroll[d].value;
    }
    auto card_y = [&](int d, int k) { return kCardsTop + k * (kCardH + kCardGap) - scroll[d]; };
    auto visible = [&](float y) { return y + kCardH >= kCardsTop - 16 && y <= gfx::H; };
    gfx::push_scissor({0, kCardsTop - 16, gfx::W, view_h + 16});
    for (int d = 0; d < 7; d++)
        for (int k = 0; k < (int)m_days[d].size(); k++)
            if (visible(card_y(d, k)))
                glass_flat({x0 + d * (kColW + kGap), card_y(d, k), kColW, kCardH}, 16, 0.85f);
    if (focused && m_index[m_day] < (int)m_days[m_day].size()) {
        m_drop.to({x0 + m_day * (kColW + kGap), card_y(m_day, m_index[m_day]), kColW, kCardH},
                  m_day * 1000 + m_index[m_day]);
        m_drop.draw(dt, 1.f, animating, 16);
    } else {
        m_drop.hide();
    }
    for (int d = 0; d < 7; d++) {
        const float cx = x0 + d * (kColW + kGap);
        for (int k = 0; k < (int)m_days[d].size(); k++) {
            const float y = card_y(d, k);
            if (!visible(y))
                continue;
            const Entry &e = m_days[d][k];
            const bool f = focused && d == m_day && k == m_index[d];
            const gfx::Rect thumb{cx + 10, y + (kCardH - kThumbH) / 2, kThumbW, kThumbH};
            art::draw_poster(thumb, {e.item.from, arr::mediacover_path(e.item.media_id, "poster-250.jpg"), e.item.instance},
                             e.item.title, seed_of(e.item.title), 8, 1.f, 120);
            const float tx = thumb.x + kThumbW + 12, tw = cx + kColW - 10 - tx;
            /* The title on up to two lines: columns are narrow. */
            gfx::text(tx, y + 32, e.item.title, {f ? gfx::Bold : gfx::SemiBold, 20, tw, 2, 23}, f ? kText : kText2);
            gfx::text(tx, y + 84, card_line(e.item), {gfx::Regular, 18, tw}, kText3);
            /* Where it stands: a dot and a word. */
            const float dy = y + 112;
            gfx::fill({tx, dy - 9, 9, 9}, state_color(e.state), 4.5f);
            gfx::text(tx + 15, dy, state_label(e.state), {gfx::Medium, 17, tw - 15}, kText2);
        }
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
