/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/downloads.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/tabbar.h"
#include "ui/widgets.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kListTop = 352, kRowH = 132, kRowGap = 14;
constexpr float kBarY = 250;
constexpr float kListW = gfx::W - 2 * kPad;
constexpr double kEvery = 5.0;

struct Look {
    i18n::S label;
    uint32_t color;
};

Look look(model::QueueItem::Status s)
{
    using S = model::QueueItem;
    switch (s) {
    case S::Downloading: return {i18n::StDownloading, 0xff0a84ffu};
    case S::Queued: return {i18n::StQueued, 0xff8e8e93u};
    case S::Paused: return {i18n::StPaused, 0xff8e8e93u};
    case S::Importing: return {i18n::StImporting, 0xff64d2ffu};
    case S::Completed: return {i18n::StCompleted, kGreen};
    case S::Warning: return {i18n::StWarning, kOrange};
    case S::Error: return {i18n::StError, kRed};
    }
    return {i18n::StQueued, 0xff8e8e93u};
}

/* Downloading first, then the ones that need a look, then the waiting ones;
 * by time left within. */
int rank(model::QueueItem::Status s)
{
    using S = model::QueueItem;
    switch (s) {
    case S::Downloading: return 0;
    case S::Importing: return 1;
    case S::Error: return 2;
    case S::Warning: return 3;
    case S::Paused: return 4;
    case S::Queued: return 5;
    case S::Completed: return 6;
    }
    return 7;
}

} // namespace

DownloadsScreen::DownloadsScreen(Context &ctx) : m_ctx(ctx), m_journal(ctx) {}

std::string DownloadsScreen::key_of(const model::QueueItem &q)
{
    return std::to_string((int)q.from) + "." + std::to_string(q.instance) + ":" + std::to_string(q.id);
}

void DownloadsScreen::activate()
{
    if (platform::now() - m_last > 2.0)
        refresh();
    if (m_view == Journal)
        m_journal.load(false);
}

std::vector<PadHint> DownloadsScreen::hints() const
{
    if (m_view == Journal && !m_bar)
        return {{PadButton::L2, ""}, {PadButton::R2, i18n::tr(i18n::HintPage)}};
    return {};
}

void DownloadsScreen::refresh()
{
    if (!m_ctx.services)
        return;
    m_last = platform::now();
    std::weak_ptr<bool> alive = m_alive;
    if (m_queues.empty())
        for (config::Service s : {config::Sonarr, config::Radarr})
            for (int i = 0; i < m_ctx.services->instances(s); i++)
                m_queues.push_back({s, i, {}});
    for (size_t k = 0; k < m_queues.size(); k++) {
        Source &src = m_queues[k];
        if (src.queue.loading())
            continue;
        if (!src.queue.ready())   /* a refresh keeps the list up meanwhile */
            src.queue.state = load::Loading;
        m_ctx.services->queue(
            src.service,
            [this, alive, k](api::Result<std::vector<model::QueueItem>> r) {
                if (alive.expired() || k >= m_queues.size())
                    return;
                m_queues[k].queue.take(std::move(r), platform::now());
                merge();
            },
            src.instance);
    }
}

void DownloadsScreen::merge()
{
    m_items.clear();
    for (const auto &src : m_queues)
        if (src.queue.ready())
            m_items.insert(m_items.end(), src.queue.value.begin(), src.queue.value.end());
    std::stable_sort(m_items.begin(), m_items.end(), [](const model::QueueItem &a, const model::QueueItem &b) {
        if (rank(a.status) != rank(b.status))
            return rank(a.status) < rank(b.status);
        const int64_t ta = a.time_left < 0 ? INT64_MAX : a.time_left, tb = b.time_left < 0 ? INT64_MAX : b.time_left;
        return ta < tb;
    });
    /* The focus stays on the same item when the list moves under it. */
    m_focus = std::min(m_focus, std::max(0, (int)m_items.size() - 1));
    for (int i = 0; i < (int)m_items.size(); i++)
        if (key_of(m_items[i]) == m_focus_key)
            m_focus = i;
}

Action DownloadsScreen::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        art::retry_failed();
        refresh();
        if (m_view == Journal)
            m_journal.load(true);
        return Action::None;
    }
    if (m_bar) {   /* the views, then the journal's filters; ✕ chooses */
        const int slots = ViewCount + (m_view == Journal ? (int)m_journal.filter_labels().size() : 0);
        if (p & input::Up)
            return Action::ToTabs;
        if (p & (input::Left | input::Right)) {
            const int f = m_bar_focus + ((p & input::Left) ? -1 : 1);
            if (f < 0 || f >= slots)
                return Action::Bump;
            m_bar_focus = f;
        } else if (p & input::Cross) {
            if (m_bar_focus < ViewCount) {
                if (m_bar_focus != m_view) {
                    m_view = (View)m_bar_focus;
                    if (m_view == Journal)
                        m_journal.load(false);
                } else {
                    m_bar = false;
                }
            } else {
                m_journal.set_filter(m_bar_focus - ViewCount);
            }
        } else if (p & input::Down) {
            m_bar = false;
        }
        return Action::None;
    }
    if (m_view == Journal) {
        if (m_journal.input(in) == Action::ToTabs)
            m_bar = true, m_bar_focus = ViewCount + m_journal.filter();
        return Action::None;
    }
    const int n = (int)m_items.size();
    if (p & input::Up) {
        if (m_focus == 0) {
            m_bar = true, m_bar_focus = m_view;
            return Action::None;
        }
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    }
    if (m_focus < n)
        m_focus_key = key_of(m_items[m_focus]);
    return Action::None;
}

bool DownloadsScreen::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (!m_focused || m_bar)
        return false;
    if (m_view == Journal)
        return m_journal.backdrop(key, ref, seed);
    if (m_focus >= (int)m_items.size())
        return false;
    const model::QueueItem &q = m_items[m_focus];
    if (q.media_id <= 0)
        return false;
    *ref = {q.from, arr::mediacover_path(q.media_id, "fanart-360.jpg"), q.instance};
    *key = ref->path + std::to_string((int)q.from) + "." + std::to_string(q.instance);
    *seed = (int)(seed_of(q.title) % 997);
    return true;
}

void DownloadsScreen::draw(double now, float dt, float ox, bool *animating)
{
    const float x0 = kPad + ox;
    gfx::text(x0, 214, tab_label(TabDownloads), {gfx::Bold, 60}, kText);

    /* The views on the left; the journal's filters on the right. */
    std::vector<std::string> views = {i18n::tr(i18n::ViewQueue), i18n::tr(i18n::ViewJournal)};
    const bool on_bar = m_focused && m_bar;
    draw_pills(x0, kBarY, views, m_view, on_bar && m_bar_focus < ViewCount ? m_bar_focus : m_view,
               on_bar && m_bar_focus < ViewCount, m_view_drop, dt, animating);
    const float chips_x = x0 + pills_width(views) + 24;
    if (m_view == Journal) {
        const std::vector<std::string> filters = m_journal.filter_labels();
        const bool on_filters = on_bar && m_bar_focus >= ViewCount;
        draw_pills(x0 + kListW, kBarY, filters, m_journal.filter(),
                   on_filters ? m_bar_focus - ViewCount : m_journal.filter(), on_filters, m_filter_drop, dt, animating,
                   true);
        m_journal.load(false);
        m_journal.draw(now, x0, kListTop, chips_x, m_focused && !m_bar, dt, animating);
        return;
    }
    draw_queue(now, x0, chips_x, dt, animating);
}

void DownloadsScreen::draw_queue(double now, float x0, float chips_x, float dt, bool *animating)
{
    if (now - m_last >= kEvery)
        refresh();
    /* Where it stands: how many, and each service that failed. */
    bool loading = false, all_off = true, any_ready = false;
    const Source *failed = nullptr;   /* the first that failed */
    float cx = chips_x;
    for (const Source &src : m_queues) {
        loading |= src.queue.loading() && !src.queue.ready();
        all_off &= src.queue.state == load::Off;
        any_ready |= src.queue.ready();
        if (src.queue.state == load::Failed && !failed)
            failed = &src;
    }
    const int n = (int)m_items.size();
    if (n > 0) {
        const std::string count = n == 1 ? i18n::tr(i18n::QueueOne) : fmt::f(i18n::tr(i18n::QueueCount), n);
        cx += draw_chip(cx, 267, count, 0xff0a84ffu, 1.f, 22) + 14;
    }
    for (const Source &src : m_queues)
        if (src.queue.state == load::Failed)
            cx += draw_chip(cx, 267,
                            fmt::labelled(m_ctx.services->config().at(src.service, src.instance).name,
                                          fmt::error(src.queue.error, src.queue.http_status)),
                            kRed, 1.f, 22) +
                  14;
    gfx::text(x0 + kListW, 296, i18n::tr(i18n::AutoRefresh), {gfx::Regular, 22}, kText3, 2);

    const gfx::Rect area{x0, kListTop, kListW, 300};
    if (n == 0) {
        if (m_focused && !m_bar)
            m_drop.to(area, -1);
        else
            m_drop.hide();
        m_drop.draw(dt, 0.6f, animating, 24);
        if (all_off)
            draw_empty(area, m_ctx.off_text(i18n::BothArrOff));
        else if (loading)
            for (int i = 0; i < 4; i++)
                draw_skeleton({x0, kListTop + i * (kRowH + kRowGap), kListW, kRowH}, 24, now, animating);
        else if (failed && !any_ready)
            draw_empty(area, fmt::error(failed->queue.error, failed->queue.http_status), i18n::tr(i18n::Retry), true);
        else
            draw_empty(area, i18n::tr(i18n::EmptyQueue));
        return;
    }

    /* The list. */
    m_focus = std::min(m_focus, n - 1);
    const float list_h = n * (kRowH + kRowGap);
    const float scroll =
        list_scroll(m_scroll, m_focus * (kRowH + kRowGap), kRowH, gfx::H - 120 - kListTop, list_h, dt, animating);
    gfx::push_scissor({0, kListTop - 20, gfx::W, gfx::H - 110 - kListTop + 20});
    for (int i = 0; i < n; i++) {
        const float y = kListTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH < kListTop - 20 || y > gfx::H)
            continue;
        glass_flat({x0, y, kListW, kRowH}, 24, 0.9f);
    }
    if (m_focused && !m_bar) {
        /* In list coordinates, the list scrolled: the drop springs within it. */
        m_drop.to({x0, kListTop + m_focus * (kRowH + kRowGap) - scroll, kListW, kRowH}, m_focus, 0, -scroll);
        m_drop.draw(dt, 1.f, animating, 24);
    } else {
        m_drop.hide();
        m_drop.draw(dt, 1.f, animating);
    }
    for (int i = 0; i < n; i++) {
        const model::QueueItem &q = m_items[i];
        const float y = kListTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH < kListTop - 20 || y > gfx::H)
            continue;
        const bool f = m_focused && !m_bar && i == m_focus;
        const art::Ref poster =
            q.media_id > 0 ? art::Ref{q.from, arr::mediacover_path(q.media_id, "poster-250.jpg"), q.instance}
                           : art::Ref{};
        art::draw_poster({x0 + 18, y + 12, 72, 108}, poster, q.title, seed_of(q.title), 10, 1.f, 160);

        const Look lk = look(q.status);
        const std::string label = i18n::tr(lk.label);
        const float right = x0 + kListW - 28;
        float chip_w = chip_width(label, 22);
        draw_chip(right - chip_w, y + 18, label, lk.color, 1.f, 22);
        if (m_ctx.services->instances(q.from) > 1) {   /* which Radarr / Sonarr */
            const std::string &name = m_ctx.services->config().at(q.from, q.instance).name;
            const float w = chip_width(name, 22);
            draw_chip(right - chip_w - 14 - w, y + 18, name, 0xff8e8e93u, 1.f, 22);
            chip_w += 14 + w;
        }

        const float tx = x0 + 116, text_w = kListW - 116 - chip_w - 60;
        gfx::text(tx, y + 50, q.title + (q.from == config::Radarr && q.year > 0 ? " (" + std::to_string(q.year) + ")" : ""),
                  {f ? gfx::Bold : gfx::SemiBold, 32, text_w}, kText);
        std::string sub;
        if (q.from == config::Sonarr && q.episode > 0)
            sub = fmt::episode(q.season, q.episode) + (q.episode_title.empty() ? "" : " · " + q.episode_title);
        if (!q.quality.empty())
            sub += (sub.empty() ? "" : "  ·  ") + q.quality;
        if (!sub.empty())
            gfx::text(tx, y + 84, sub, {gfx::Regular, 24, text_w}, kText2);

        const float bar_w = kListW - 116 - 380;
        draw_progress({tx, y + 104, bar_w, 8}, (float)q.progress, lk.color);
        const std::string pct = fmt::f("%d %%", (int)(q.progress * 100 + 0.5));
        gfx::text(tx + bar_w + 18, y + 113, pct, {gfx::SemiBold, 22}, kText2);
        if (q.time_left >= 0 && (q.status == model::QueueItem::Downloading || q.status == model::QueueItem::Queued))
            gfx::text(right, y + 113, fmt::f(i18n::tr(i18n::TimeLeft), fmt::duration(q.time_left).c_str()),
                      {gfx::Medium, 24}, kText, 2);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
