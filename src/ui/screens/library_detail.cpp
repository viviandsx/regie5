/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/library_detail.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kPosterX = kPad, kPosterY = 150, kPosterW = 330, kPosterH = 495;
constexpr float kTextX = kPad + kPosterW + 60, kTextW = gfx::W - kPad - kTextX;
constexpr float kSeasonH = 74, kSeasonGap = 8;

std::string series_status(model::Series::Status s)
{
    return i18n::tr(s == model::Series::Ended      ? i18n::SeEnded
                    : s == model::Series::Upcoming ? i18n::SeUpcoming
                    : s == model::Series::Deleted  ? i18n::SeDeleted
                                                   : i18n::SeContinuing);
}

std::string movie_status(model::Movie::Status s)
{
    return i18n::tr(s == model::Movie::Tba         ? i18n::MvTba
                    : s == model::Movie::Announced ? i18n::MvAnnounced
                    : s == model::Movie::InCinemas ? i18n::MvInCinemas
                    : s == model::Movie::Deleted   ? i18n::MvDeleted
                                                   : i18n::MvReleased);
}

std::string when(time_t t, time_t now) { return fmt::day(t, now) + " · " + fmt::clock(t); }

/* "12 mars 2025": a day far from today, without the weekday. */
std::string date(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    static const char *const kFr[12] = {"janv.", "févr.", "mars", "avr.", "mai", "juin",
                                        "juil.", "août", "sept.", "oct.", "nov.", "déc."};
    static const char *const kEn[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (i18n::lang() == i18n::Lang::French)
        return fmt::f("%d %s %d", tm.tm_mday, kFr[tm.tm_mon], tm.tm_year + 1900);
    return fmt::f("%s %d, %d", kEn[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
}

/* A line of the information block: a quiet label, then the value. */
float info(float x, float y, const char *label, const std::string &value, float w)
{
    gfx::text(x, y, label, {gfx::Medium, 24, 330}, kText3);
    gfx::text(x + 340, y, value, {gfx::Medium, 26, w - 340}, kText);
    return y + 42;
}

} // namespace

void LibraryDetail::open_series(const model::Series &s, const std::string &profile, api::Services *services,
                                const std::string &instance)
{
    m_open = true;
    m_tv = true;
    m_series = s;
    m_profile = profile;
    m_instance = instance;
    m_focus = 0;
    m_scroll.snap(0);
    m_show.snap(0);
    m_show.to(1);
    m_drop.hide();
    m_files = {};
    if (!services)
        return;
    m_files.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    const int id = s.id, inst = s.instance;
    services->episode_files(
        id,
        [this, alive, id, inst](api::Result<model::FileSummary> r) {
            if (!alive.expired() && m_open && m_tv && m_series.id == id && m_series.instance == inst)
                m_files.take(std::move(r), platform::now());
        },
        inst);
}

void LibraryDetail::open_movie(const model::Movie &m, const std::string &profile, const std::string &instance)
{
    m_open = true;
    m_tv = false;
    m_movie = m;
    m_profile = profile;
    m_instance = instance;
    m_show.snap(0);
    m_show.to(1);
    m_drop.hide();
}

bool LibraryDetail::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    if (p & input::Circle) {
        m_open = false;
        return false;
    }
    if (m_tv) {
        const int n = (int)m_series.seasons.size();
        if (p & input::Up)
            m_focus = std::max(0, m_focus - 1);
        else if (p & input::Down)
            m_focus = std::min(n - 1, m_focus + 1);
    }
    return true;
}

bool LibraryDetail::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (!m_open)
        return false;
    const config::Service s = m_tv ? config::Sonarr : config::Radarr;
    const int id = m_tv ? m_series.id : m_movie.id, inst = m_tv ? m_series.instance : m_movie.instance;
    *ref = {s, arr::mediacover_path(id, "fanart-360.jpg"), inst};
    *key = ref->path + std::to_string((int)s) + "." + std::to_string(inst);
    *seed = (int)(seed_of(m_tv ? m_series.title : m_movie.title) % 997);
    return true;
}

void LibraryDetail::draw(double now, float dt, float ox, bool focused, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 10.f))
        *animating = true;
    const float a = smoothstep(m_show.value), slide = (1 - a) * 40;
    gfx::push_opacity(a);
    const time_t now_t = time(nullptr);
    const config::Service svc = m_tv ? config::Sonarr : config::Radarr;
    const int id = m_tv ? m_series.id : m_movie.id, inst = m_tv ? m_series.instance : m_movie.instance;
    const std::string &title = m_tv ? m_series.title : m_movie.title;

    /* The poster. */
    const gfx::Rect poster{kPosterX + ox, kPosterY + slide, kPosterW, kPosterH};
    gfx::shadow(poster, 18, 40, 0.5f, 14);
    art::draw_poster(poster, {svc, arr::mediacover_path(id, "poster-500.jpg"), inst}, title, seed_of(title), 18, 1.f, 500);

    /* The title and what it is. */
    const float x = kTextX + ox;
    float y = 214 + slide;
    gfx::text(x, y, title, {gfx::Bold, 56, kTextW, 2, 64}, kText);
    y += gfx::text_width(title, {gfx::Bold, 56}) > kTextW ? 64 + 44 : 44;
    std::string sub;
    const int year = m_tv ? m_series.year : m_movie.year;
    if (year > 0)
        sub = std::to_string(year);
    const std::string &where = m_tv ? m_series.network : m_movie.studio;
    if (!where.empty())
        sub += (sub.empty() ? "" : " · ") + where;
    const int runtime = m_tv ? m_series.runtime : m_movie.runtime;
    if (runtime > 0)
        sub += (sub.empty() ? "" : " · ") + fmt::f(i18n::tr(i18n::Minutes), runtime);
    if (m_tv) {
        int seasons = 0;
        for (const auto &s : m_series.seasons)
            seasons += s.number > 0;
        sub += (sub.empty() ? "" : " · ") +
               (seasons == 1 ? std::string(i18n::tr(i18n::SeasonsOne)) : fmt::f(i18n::tr(i18n::SeasonsCount), seasons));
    }
    gfx::text(x, y, sub, {gfx::Regular, 28, kTextW}, kText2);
    y += 26;

    /* Chips: which instance (when several), status, monitored, missing. */
    float cx = x;
    if (!m_instance.empty())
        cx += draw_chip(cx, y, m_instance, 0xffbf5af2u) + 12;
    const bool monitored = m_tv ? m_series.monitored : m_movie.monitored;
    if (m_tv)
        cx += draw_chip(cx, y, series_status(m_series.status),
                        m_series.status == model::Series::Ended ? 0xff8e8e93u : 0xff0a84ffu) + 12;
    else
        cx += draw_chip(cx, y, movie_status(m_movie.status), 0xff0a84ffu) + 12;
    cx += draw_chip(cx, y, i18n::tr(monitored ? i18n::Monitored : i18n::Unmonitored),
                    monitored ? kGreen : 0xff8e8e93u) + 12;
    if (m_tv && m_series.missing() > 0)
        draw_chip(cx, y, m_series.missing() == 1 ? std::string(i18n::tr(i18n::MissingOne))
                                                 : fmt::f(i18n::tr(i18n::MissingCount), m_series.missing()),
                  kOrange);
    else if (!m_tv && !m_movie.has_file)
        draw_chip(cx, y, i18n::tr(i18n::NoFile), kOrange);
    y += 86;

    /* What it is made of. */
    if (!m_profile.empty())
        y = info(x, y, i18n::tr(i18n::InfoProfile), m_profile, kTextW);
    const int64_t size = m_tv ? m_series.size : m_movie.size;
    if (size > 0)
        y = info(x, y, i18n::tr(i18n::InfoSize), fmt::size(size), kTextW);
    if (m_tv) {
        y = info(x, y, i18n::tr(i18n::InfoEpisodes),
                 fmt::f(i18n::tr(i18n::EpisodesOf), m_series.files, m_series.episodes), kTextW);
        if (m_files.ready() && !m_files.value.qualities.empty()) {
            std::string q;
            for (const auto &p : m_files.value.qualities)
                q += (q.empty() ? "" : ", ") + p.first + " ×" + std::to_string(p.second);
            y = info(x, y, i18n::tr(i18n::InfoFiles), q, kTextW);
        }
        if (m_series.next_airing)
            y = info(x, y, i18n::tr(i18n::InfoNextAiring), when(m_series.next_airing, now_t), kTextW);
        else if (m_series.previous_airing)
            y = info(x, y, i18n::tr(i18n::InfoLastAiring), date(m_series.previous_airing), kTextW);
    } else {
        y = info(x, y, i18n::tr(i18n::InfoFile),
                 m_movie.has_file ? m_movie.file_quality : std::string(i18n::tr(i18n::NoFile)), kTextW);
        std::string rel;
        if (m_movie.in_cinemas)
            rel += std::string(i18n::tr(i18n::ReleaseCinema)) + " " + date(m_movie.in_cinemas);
        if (m_movie.digital)
            rel += (rel.empty() ? "" : " · ") + std::string(i18n::tr(i18n::ReleaseDigital)) + " " + date(m_movie.digital);
        if (m_movie.physical)
            rel += (rel.empty() ? "" : " · ") + std::string(i18n::tr(i18n::ReleasePhysical)) + " " +
                   date(m_movie.physical);
        if (!rel.empty())
            y = info(x, y, i18n::tr(i18n::InfoReleases), rel, kTextW);
    }
    const time_t added = m_tv ? m_series.added : m_movie.added;
    if (added)
        y = info(x, y, i18n::tr(i18n::InfoAdded), date(added), kTextW);

    const std::string &overview = m_tv ? m_series.overview : m_movie.overview;
    if (!overview.empty()) {
        y += 20;
        gfx::text(x, y, overview, {gfx::Regular, 26, kTextW, m_tv ? 3 : 5, 38}, kText2);
        y += (m_tv ? 3 : 5) * 38;
    }

    /* A series' seasons: one row each, the episodes there and missing. */
    if (m_tv && !m_series.seasons.empty()) {
        y += 10;
        gfx::text(x, y, i18n::tr(i18n::SeasonsTitle), {gfx::SemiBold, 30}, kText);
        const float top = y + 22, view_h = std::max(kSeasonH, gfx::H - 120 - top);
        const int n = (int)m_series.seasons.size();
        const float list_h = n * (kSeasonH + kSeasonGap) - kSeasonGap;
        const float scroll =
            list_scroll(m_scroll, m_focus * (kSeasonH + kSeasonGap), kSeasonH, view_h, list_h, dt, animating);
        gfx::push_scissor({x - 10, top - 6, kTextW + 20, view_h + 12});
        for (int i = 0; i < n; i++)
            glass_flat({x, top + i * (kSeasonH + kSeasonGap) - scroll, kTextW, kSeasonH}, 18, 0.85f);
        if (focused) {
            m_drop.to({x, top + m_focus * (kSeasonH + kSeasonGap) - scroll, kTextW, kSeasonH}, m_focus, 0, -scroll);
            m_drop.draw(dt, 1.f, animating, 18);
        } else {
            m_drop.hide();
        }
        for (int i = 0; i < n; i++) {
            const model::SeasonStats &s = m_series.seasons[i];
            const float ry = top + i * (kSeasonH + kSeasonGap) - scroll;
            if (ry + kSeasonH < top - 6 || ry > top + view_h + 6)
                continue;
            const std::string name =
                s.number == 0 ? std::string(i18n::tr(i18n::Specials)) : fmt::f(i18n::tr(i18n::SeasonOne), s.number);
            gfx::text(x + 28, ry + 47, name, {gfx::SemiBold, 28, 330}, s.monitored ? kText : kText3);
            const float bar_x = x + 380, bar_w = kTextW - 380 - 520;
            const float v = s.episodes > 0 ? (float)s.files / s.episodes : (s.total > 0 ? 0.f : 1.f);
            const int missing = s.episodes > s.files ? s.episodes - s.files : 0;
            draw_progress({bar_x, ry + kSeasonH / 2 - 4, bar_w, 8}, v, missing > 0 ? kOrange : kGreen);
            gfx::text(bar_x + bar_w + 24, ry + 46, fmt::f(i18n::tr(i18n::EpisodesOf), s.files, s.episodes),
                      {gfx::Medium, 24}, kText2);
            std::string chip;
            uint32_t dot = kGreen;
            if (!s.monitored)
                chip = i18n::tr(i18n::Unmonitored), dot = 0xff8e8e93u;
            else if (missing > 0)
                chip = missing == 1 ? std::string(i18n::tr(i18n::MissingOne))
                                    : fmt::f(i18n::tr(i18n::MissingCount), missing),
                dot = kOrange;
            else if (s.episodes > 0)
                chip = i18n::tr(i18n::Complete);
            if (!chip.empty())
                draw_chip(x + kTextW - 24 - chip_width(chip, 20), ry + kSeasonH / 2 - 17, chip, dot, 1.f, 20);
        }
        gfx::pop_scissor();
    }
    gfx::pop_opacity();
}

} // namespace ui
