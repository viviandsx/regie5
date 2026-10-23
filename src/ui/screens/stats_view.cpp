/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/stats_view.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "core/lang/language.h"
#include "platform/platform.h"
#include "ui/arr_lists.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cctype>

namespace ui {

namespace {

constexpr float kListW = gfx::W - 2 * kPad;
constexpr double kFresh = 300;   /* a period is asked again after 5 minutes (or △) */
constexpr float kPosterW = 112, kPosterH = 168, kPosterGap = 20;
constexpr int kTitles = 6, kUsers = 5;

std::string lowered(const std::string &s)
{
    std::string out = s;
    for (char &c : out)
        c = (char)std::tolower((unsigned char)c);
    return out;
}

/* Local noon of a date: its weekday, whatever the time zone. */
time_t noon(int y, int m, int d)
{
    struct tm t {};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

} // namespace

void StatsView::load(bool force, double now)
{
    Load<model::PlayStats> &l = m_stats[m_period];
    if (!m_ctx.services || l.loading() || (!force && l.state != load::Idle && now - l.at < kFresh))
        return;
    if (!l.ready())
        l.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    const stats::Period p = m_period;
    m_ctx.services->play_stats(stats::days(p), [this, alive, p](api::Result<model::PlayStats> r) {
        if (!alive.expired())
            m_stats[p].take(std::move(r), platform::now());
    });
    if (m_ctx.lists) {   /* the posters, by name */
        m_ctx.lists->want_movies(false);
        m_ctx.lists->want_series(false);
    }
}

void StatsView::set_period(int p, double now)
{
    m_period = (stats::Period)std::clamp(p, 0, (int)stats::PeriodCount - 1);
    load(false, now);
}

std::vector<std::string> StatsView::period_labels() const
{
    return {i18n::tr(i18n::StatsWeek), i18n::tr(i18n::StatsMonth), i18n::tr(i18n::StatsYear)};
}

void StatsView::index_posters()
{
    if (!m_ctx.lists || m_ctx.lists->version() == m_lists_version)
        return;
    m_lists_version = m_ctx.lists->version();
    m_movies.clear();
    m_series.clear();
    for (const auto &part : m_ctx.lists->movies())
        if (part.load.ready() && part.load.value)
            for (const auto &m : *part.load.value)
                m_movies.insert({lowered(m.title), {config::Radarr, m.instance, m.id}});
    for (const auto &part : m_ctx.lists->series())
        if (part.load.ready() && part.load.value)
            for (const auto &s : *part.load.value)
                m_series.insert({lowered(s.title), {config::Sonarr, s.instance, s.id}});
}

const StatsView::Poster *StatsView::poster_of(bool movie, const std::string &name) const
{
    const auto &index = movie ? m_movies : m_series;
    auto f = index.find(lowered(name));
    return f == index.end() ? nullptr : &f->second;
}

bool StatsView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    const Load<model::PlayStats> &l = m_stats[m_period];
    if (!l.ready() || l.value.movies.empty())
        return false;
    const Poster *p = poster_of(true, l.value.movies.front().name);
    if (!p)
        return false;
    *ref = {p->from, arr::mediacover_path(p->id, "fanart-360.jpg"), p->instance};
    *key = ref->path + "." + std::to_string(p->instance);
    *seed = (int)(seed_of(l.value.movies.front().name) % 997);
    return true;
}

void StatsView::draw_bars(const model::PlayStats &s, float x, float y, float w, float h)
{
    const std::vector<stats::Bar> bars = stats::bars(s, m_period);
    if (bars.empty())
        return;
    int64_t most = 1;
    size_t top = 0;
    for (size_t i = 0; i < bars.size(); i++)
        if (bars[i].seconds > most)
            most = bars[i].seconds, top = i;
    const float slot = w / bars.size();
    const float gap = std::min(14.f, slot * 0.3f);
    gfx::fill({x, y + h, w, 1}, alpha(0xffffffffu, 0.12f));
    for (size_t i = 0; i < bars.size(); i++) {
        const stats::Bar &b = bars[i];
        const float bh = b.seconds > 0 ? std::max(6.f, (float)b.seconds / most * (h - 30)) : 2.f;
        const float bx = x + i * slot;
        gfx::fill({bx + gap / 2, y + h - bh, slot - gap, bh}, alpha(0xff0a84ffu, b.seconds > 0 ? 0.85f : 0.3f),
                  std::min(6.f, (slot - gap) / 2));
        /* Each value over 7 days; over more, only the highest. */
        if (b.seconds > 0 && (bars.size() <= 7 || i == top))
            gfx::text(bx + slot / 2, y + h - bh - 10, fmt::watch_time(b.seconds), {gfx::Medium, 19}, kText2, 1);
        std::string label;
        if (m_period == stats::Week)
            label = fmt::weekday(noon(b.y, b.m, b.d));
        else if (m_period == stats::Year)
            label = i18n::language().months[std::clamp(b.m, 1, 12) - 1];
        else if (i % 5 == 4 || i + 1 == bars.size())
            label = std::to_string(b.d);
        if (!label.empty())
            gfx::text(bx + slot / 2, y + h + 30, label, {gfx::Regular, 19}, kText3, 1);
    }
}

void StatsView::draw_users(const model::PlayStats &s, float x, float y, float w)
{
    int64_t most = 1;
    for (const auto &u : s.users)
        most = std::max(most, u.seconds);
    const float name_w = 190, time_w = 120;
    for (size_t i = 0; i < s.users.size() && i < (size_t)kUsers; i++) {
        const auto &u = s.users[i];
        const float ry = y + i * 40;
        gfx::text(x, ry, u.name, {gfx::Medium, 24, name_w - 12}, kText);
        const float bw = w - name_w - time_w;
        gfx::fill({x + name_w, ry - 14, bw, 10}, alpha(0xffffffffu, 0.08f), 5);
        gfx::fill({x + name_w, ry - 14, std::max(10.f, bw * (float)u.seconds / most), 10}, 0xff0a84ffu, 5);
        gfx::text(x + w, ry, fmt::watch_time(u.seconds), {gfx::Medium, 22}, kText2, 2);
    }
}

void StatsView::draw_titles(const std::vector<model::PlayStats::Title> &t, bool movies, float x, float y, float w)
{
    const int n = std::min<int>((int)t.size(), std::min(kTitles, (int)((w + kPosterGap) / (kPosterW + kPosterGap))));
    for (int i = 0; i < n; i++) {
        const auto &title = t[i];
        const float px = x + i * (kPosterW + kPosterGap);
        const Poster *p = poster_of(movies, title.name);
        const art::Ref ref = p ? art::Ref{p->from, arr::mediacover_path(p->id, "poster-250.jpg"), p->instance} : art::Ref{};
        draw_poster_card({px, y, kPosterW, kPosterH}, ref, title.name, seed_of(title.name), 0.f, 1.f);
        gfx::text(px, y + kPosterH + 28, title.name, {gfx::Medium, 19, kPosterW + 8}, kText2);
        gfx::text(px, y + kPosterH + 52, fmt::count(i18n::StatsPlayOne, i18n::StatsPlayMany, title.plays),
                  {gfx::Regular, 18, kPosterW + 8}, kText3);
    }
}

void StatsView::draw(double now, float x0, float top, float dt, bool *animating)
{
    load(false, now);
    index_posters();
    const Load<model::PlayStats> &l = m_stats[m_period];
    const gfx::Rect area{x0, top + 10, kListW, 600};
    if (l.state == load::Off) {
        draw_empty(area, m_ctx.off_text(i18n::JellyfinOff));
        return;
    }
    if (l.state == load::Failed) {
        draw_empty(area, fmt::labelled(i18n::media_server(), fmt::error(l.error, l.http_status)), i18n::tr(i18n::Retry), true);
        return;
    }
    if (!l.ready()) {
        draw_skeleton({x0, top + 20, 420, 64}, 14, now, animating);
        draw_skeleton({x0, top + 130, 980, 200}, 18, now, animating);
        draw_skeleton({x0 + 1060, top + 130, kListW - 1060, 200}, 18, now, animating);
        draw_skeleton({x0, top + 400, kListW, kPosterH + 40}, 18, now, animating);
        return;
    }
    const model::PlayStats &s = l.value;
    if (stats::empty(s)) {   /* a plugin just installed, or a quiet week */
        draw_empty(area, i18n::tr(i18n::StatsEmpty), i18n::tr(i18n::StatsEmptyNote));
        return;
    }

    /* How much, in all. */
    const float tw = gfx::text(x0, top + 68, fmt::watch_time(stats::total_seconds(s)), {gfx::Bold, 64}, kText);
    gfx::text(x0 + tw + 20, top + 68,
              std::string(i18n::tr(i18n::StatsWatched)) + " · " +
                  fmt::count(i18n::StatsPlayOne, i18n::StatsPlayMany, stats::total_plays(s)),
              {gfx::Regular, 28}, kText2);

    /* By day (month) on the left, by user on the right. */
    const float cy = top + 140, users_x = x0 + 1060;
    gfx::text(x0, cy, i18n::tr(m_period == stats::Year ? i18n::StatsByMonth : i18n::StatsByDay), {gfx::SemiBold, 28}, kText);
    draw_bars(s, x0, cy + 22, 980, 170);
    gfx::text(users_x, cy, i18n::tr(i18n::StatsByUser), {gfx::SemiBold, 28}, kText);
    draw_users(s, users_x, cy + 56, kListW - 1060);

    /* The titles most watched: films, then series. */
    const float ty = top + 410, half = (kListW - 60) / 2;
    gfx::text(x0, ty, i18n::tr(i18n::StatsTopMovies), {gfx::SemiBold, 28}, kText);
    gfx::text(x0 + half + 60, ty, i18n::tr(i18n::StatsTopSeries), {gfx::SemiBold, 28}, kText);
    if (s.movies.empty())
        gfx::text(x0, ty + 60, "—", {gfx::Regular, 24}, kText3);
    if (s.series.empty())
        gfx::text(x0 + half + 60, ty + 60, "—", {gfx::Regular, 24}, kText3);
    draw_titles(s.movies, true, x0, ty + 24, half);
    draw_titles(s.series, false, x0 + half + 60, ty + 24, half);
    if (art::busy())
        *animating = true;
}

} // namespace ui
