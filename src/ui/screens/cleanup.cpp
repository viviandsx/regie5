/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/cleanup.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kTop = 300, kRowH = 120, kRowGap = 12;
constexpr float kListW = gfx::W - 2 * kPad;
const int kMonths[4] = {3, 6, 12, 24};

std::string names(const std::vector<std::string> &v)
{
    std::string out;
    for (size_t i = 0; i < v.size() && i < 4; i++)
        out += (i ? ", " : "") + v[i];
    if (v.size() > 4)
        out += ", +" + std::to_string(v.size() - 4);
    return out;
}

} // namespace

CleanupView::CleanupView(Context &ctx) : m_ctx(ctx) {}

void CleanupView::load(bool force)
{
    api::Services *svc = m_ctx.services;
    if (!svc)
        return;
    std::weak_ptr<bool> alive = m_alive;
    auto stale = [force](const auto &l) { return !l.loading() && (force || l.state == load::Idle); };
    if (m_sources.empty())
        for (config::Service s : {config::Radarr, config::Sonarr})
            for (int i = 0; i < svc->instances(s); i++)
                m_sources.push_back({s, i, {}, {}});
    for (size_t k = 0; k < m_sources.size(); k++) {
        Source &src = m_sources[k];
        if (src.service == config::Radarr && stale(src.movies)) {
            src.movies.state = load::Loading;
            svc->movies(
                [this, alive, k](api::Result<std::vector<model::Movie>> r) {
                    if (alive.expired() || k >= m_sources.size())
                        return;
                    m_sources[k].movies.take(std::move(r), platform::now());
                    rebuild();
                },
                src.instance);
        } else if (src.service == config::Sonarr && stale(src.series)) {
            src.series.state = load::Loading;
            svc->series(
                [this, alive, k](api::Result<std::vector<model::Series>> r) {
                    if (alive.expired() || k >= m_sources.size())
                        return;
                    m_sources[k].series.take(std::move(r), platform::now());
                    rebuild();
                },
                src.instance);
        }
    }
    if (stale(m_users)) {
        m_users.state = load::Loading;
        svc->users([this, alive](api::Result<std::vector<model::JellyfinUser>> r) {
            if (alive.expired())
                return;
            m_users.take(std::move(r), platform::now());
            if (!m_users.ready())
                return;
            /* What each user watched: one call per user. */
            m_items.clear();
            for (const auto &u : m_users.value) {
                const std::string id = u.id;
                m_items[id].state = load::Loading;
                m_ctx.services->user_items(id, [this, alive, id](api::Result<std::vector<model::UserItem>> res) {
                    if (alive.expired())
                        return;
                    m_items[id].take(std::move(res), platform::now());
                    rebuild();
                });
            }
            rebuild();
        });
    }
}

void CleanupView::rebuild()
{
    m_list.clear();
    m_total = 0;
    if (m_show_kept) {   /* the kept titles, as they were saved */
        for (const auto &k : m_ctx.settings.kept) {
            cleanup::Candidate c;
            c.service = k.service, c.instance_name = k.instance, c.id = k.id, c.title = k.title;
            m_list.push_back(c);
        }
        m_focus = std::clamp(m_focus, 0, std::max(0, (int)m_list.size() - 1));
        return;
    }
    /* Only once everything is in: a partial answer would offer titles someone watched recently. */
    if (!m_users.ready())
        return;
    std::map<std::string, std::vector<model::UserItem>> items;
    for (const auto &kv : m_items) {
        if (!kv.second.ready())
            return;
        items[kv.first] = kv.second.value;
    }
    std::vector<cleanup::Library> libs;
    for (const Source &src : m_sources) {
        const bool ready = src.service == config::Radarr ? src.movies.ready() : src.series.ready();
        const bool off = src.service == config::Radarr ? src.movies.state == load::Off : src.series.state == load::Off;
        if (off)
            continue;
        if (!ready)
            return;
        cleanup::Library l;
        l.service = src.service;
        l.instance = src.instance;
        l.instance_name = m_ctx.services->config().at(src.service, src.instance).name;
        l.movies = src.movies.value;
        l.series = src.series.value;
        libs.push_back(std::move(l));
    }
    const cleanup::Criteria c{m_ctx.settings.cleanup_everyone, m_ctx.settings.cleanup_months};
    m_list = cleanup::candidates(libs, m_users.value, items, m_ctx.services->config().ignore_users, c, m_ctx.settings,
                                 time(nullptr));
    for (const auto &x : m_list)
        m_total += x.size;
    m_focus = std::clamp(m_focus, 0, std::max(0, (int)m_list.size() - 1));
}

std::vector<std::string> CleanupView::pills() const
{
    return {i18n::tr(m_ctx.settings.cleanup_everyone ? i18n::CleanupEveryone : i18n::CleanupAnyone),
            fmt::f(i18n::tr(i18n::CleanupMonths), m_ctx.settings.cleanup_months),
            fmt::f(i18n::tr(i18n::CleanupKeptPill), (int)m_ctx.settings.kept.size())};
}

void CleanupView::choose(int pill)
{
    if (pill == 2) {   /* the kept titles, and back */
        m_show_kept = !m_show_kept;
        m_focus = 0;
        m_scroll.snap(0);
        rebuild();
        return;
    }
    if (pill == 0) {
        const int active = m_users.ready()
                               ? (int)cleanup::active_users(m_users.value, m_ctx.services->config().ignore_users,
                                                            time(nullptr))
                                     .size()
                               : 0;
        m_sheet.open(i18n::tr(i18n::ChooseWatchedBy), "",
                     {fmt::f(i18n::tr(i18n::ChooseEveryone), active), i18n::tr(i18n::ChooseAnyone)},
                     m_ctx.settings.cleanup_everyone ? 0 : 1, [this](int c) {
                         if (c < 0)
                             return;
                         m_ctx.settings.cleanup_everyone = c == 0;
                         m_ctx.save_settings();
                         rebuild();
                     });
        return;
    }
    std::vector<std::string> opts;
    int cur = 1;
    for (int i = 0; i < 4; i++) {
        opts.push_back(fmt::f(i18n::tr(i18n::MonthsN), kMonths[i]));
        if (kMonths[i] == m_ctx.settings.cleanup_months)
            cur = i;
    }
    m_sheet.open(i18n::tr(i18n::ChooseMonths), "", opts, cur, [this](int c) {
        if (c < 0 || c > 3)
            return;
        m_ctx.settings.cleanup_months = kMonths[c];
        m_ctx.save_settings();
        rebuild();
    });
}

void CleanupView::keep(const cleanup::Candidate &c)
{
    m_ctx.settings.kept.push_back({c.service, c.instance_name, c.id, c.title});
    m_ctx.save_settings();
    notify(fmt::f(i18n::tr(i18n::CleanupKeptDone), c.title.c_str()), true, platform::now());
    rebuild();
}

void CleanupView::act()
{
    if (m_focus >= (int)m_list.size())
        return;
    const cleanup::Candidate c = m_list[m_focus];
    if (m_show_kept) {
        m_sheet.open(c.title, "", {i18n::tr(i18n::CleanupUnkeep), i18n::tr(i18n::Cancel)}, 1, [this, c](int choice) {
            if (choice != 0)
                return;
            auto &kept = m_ctx.settings.kept;
            kept.erase(std::remove_if(kept.begin(), kept.end(),
                                      [&c](const app_settings::Kept &k) {
                                          return k.service == c.service && k.instance == c.instance_name &&
                                                 k.id == c.id;
                                      }),
                       kept.end());
            m_ctx.save_settings();
            rebuild();
        });
        return;
    }
    const std::string body = fmt::size(c.size) + (c.watched_by.empty() ? "" : " · " + fmt::f(i18n::tr(i18n::CleanupWatchedBy), names(c.watched_by).c_str()));
    m_sheet.open(c.title + (c.year > 0 ? " (" + std::to_string(c.year) + ")" : ""), body,
                 {i18n::tr(i18n::CleanupKeep), i18n::tr(i18n::CleanupDeleteDots), i18n::tr(i18n::Cancel)}, 2,
                 [this, c](int choice) {
                     if (choice == 0)
                         keep(c);
                     else if (choice == 1)
                         confirm_delete(c);
                 });
}

/* Twice, Cancel first both times: what goes (title, size, where), and that the files go for good. */
void CleanupView::confirm_delete(const cleanup::Candidate &c)
{
    if (m_busy) {
        notify(i18n::tr(i18n::CleanupBusy), false, platform::now());
        return;
    }
    std::string what = c.title + (c.year > 0 ? " (" + std::to_string(c.year) + ")" : "") + " · " + fmt::size(c.size);
    if (m_ctx.services && m_ctx.services->instances(c.service) > 1)
        what += " · " + c.instance_name;
    m_sheet.open(fmt::f(i18n::tr(i18n::CleanupConfirm1), c.title.c_str()),
                 fmt::f(i18n::tr(i18n::CleanupConfirm1Body), what.c_str()),
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::CleanupContinue)}, 0, [this, c](int choice) {
                     if (choice != 1)
                         return;
                     m_sheet.open(i18n::tr(i18n::CleanupConfirm2),
                                  fmt::f(i18n::tr(i18n::CleanupConfirm2Body), c.title.c_str(), fmt::size(c.size).c_str()),
                                  {i18n::tr(i18n::Cancel), i18n::tr(i18n::CleanupDelete)}, 0,
                                  [this, c](int second) {
                                      if (second == 1)
                                          do_delete(c);
                                  },
                                  true);
                 },
                 true);
}

void CleanupView::do_delete(const cleanup::Candidate &c)
{
    if (m_busy || !m_ctx.services)
        return;
    m_busy = true;
    std::weak_ptr<bool> alive = m_alive;
    auto after = [this, alive, c](api::Result<bool> r) {
        if (alive.expired())
            return;
        if (!r.ok()) {
            m_busy = false;
            notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(r.error, r.http_status).c_str()), false,
                   platform::now());
            return;
        }
        /* Gone from Radarr / Sonarr. Seerr has no call to mark a title deleted (its
         * POST /media/{id}/{status} knows available, partial, processing, pending,
         * unknown, and answers 200 to anything else without a change): its own
         * "Media Availability Sync" marks it deleted, its requests kept, so that it
         * can be asked for again. The note says so; the lists are read again. */
        m_busy = false;
        const bool seerr = m_ctx.services->enabled(config::Seerr);
        notify(fmt::f(i18n::tr(seerr ? i18n::CleanupDoneSeerr : i18n::CleanupDone), c.title.c_str(),
                      fmt::size(c.size).c_str()),
               true, platform::now(), seerr ? 6.0 : 3.0);
        for (Source &src : m_sources)
            if (src.service == c.service && src.instance == c.instance) {
                src.movies.state = load::Idle;
                src.series.state = load::Idle;
            }
        load(false);
    };
    if (c.service == config::Radarr)
        m_ctx.services->delete_movie(c.instance, c.id, after);
    else
        m_ctx.services->delete_series(c.instance, c.id, after);
}

Action CleanupView::input(const input::State &in)
{
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    const int n = (int)m_list.size();
    if (p & input::Up) {
        if (m_focus == 0)
            return Action::ToTabs;
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    } else if (p & (input::Cross | input::Options)) {
        act();
    } else if (p & (input::L2 | input::R2)) {
        m_focus = std::clamp(m_focus + ((p & input::R2) ? 5 : -5), 0, std::max(0, n - 1));
    }
    return Action::None;
}

std::vector<PadHint> CleanupView::hints() const
{
    if (m_list.empty())
        return {};
    return {{PadButton::Options, i18n::tr(i18n::HintOptions)}};
}

bool CleanupView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (m_focus >= (int)m_list.size())
        return false;
    const cleanup::Candidate &c = m_list[m_focus];
    *ref = {c.service, arr::mediacover_path(c.id, "fanart-360.jpg"), c.instance};
    *key = ref->path + std::to_string((int)c.service) + "." + std::to_string(c.instance);
    *seed = (int)(seed_of(c.title) % 997);
    return true;
}

void CleanupView::draw(double now, float x0, bool focused, float dt, bool *animating)
{
    /* How much, or why nothing. */
    bool loading = !m_users.ready() && m_users.state != load::Failed && m_users.state != load::Off;
    for (const auto &kv : m_items)
        loading |= kv.second.loading();
    for (const auto &s : m_sources)
        loading |= s.movies.loading() || s.series.loading();
    const int n = (int)m_list.size();
    if (!m_show_kept && n > 0)
        gfx::text(x0, 262,
                  fmt::f(i18n::tr(n == 1 ? i18n::CleanupSummaryOne : i18n::CleanupSummary), n, fmt::size(m_total).c_str()),
                  {gfx::SemiBold, 26}, kText2);
    const gfx::Rect area{x0, kTop, kListW, 260};
    if (n == 0) {
        if (m_show_kept) {
            draw_empty(area, i18n::tr(i18n::CleanupKeptEmpty));
        } else if (m_users.state == load::Off) {
            draw_empty(area, m_ctx.off_text(i18n::JellyfinOff));
        } else if (m_users.state == load::Failed) {
            draw_empty(area, fmt::labelled("Jellyfin", fmt::error(m_users.error, m_users.http_status)),
                       i18n::tr(i18n::Retry), true);
        } else if (loading) {
            for (int i = 0; i < 4; i++)
                draw_skeleton({x0, kTop + i * (kRowH + kRowGap), kListW, kRowH}, 22, now, animating);
        } else if (m_users.ready() &&
                   cleanup::active_users(m_users.value, m_ctx.services->config().ignore_users, time(nullptr)).empty()) {
            draw_empty(area, i18n::tr(i18n::CleanupNoUsers));
        } else {
            draw_empty(area, i18n::tr(i18n::CleanupNone));
        }
        if (focused && !loading) {
            m_drop.to(area, -1);
            m_drop.draw(dt, 0.6f, animating, 24);
        }
        return;
    }

    const float view_h = gfx::H - 110 - kTop;
    const float scroll = list_scroll(m_scroll, m_focus * (kRowH + kRowGap), kRowH, view_h, n * (kRowH + kRowGap), dt,
                                     animating);
    gfx::push_scissor({0, kTop - 20, gfx::W, view_h + 20});
    for (int i = 0; i < n; i++) {
        const float y = kTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH >= kTop - 20 && y <= gfx::H)
            glass_flat({x0, y, kListW, kRowH}, 22, 0.9f);
    }
    if (focused) {
        m_drop.to({x0, kTop + m_focus * (kRowH + kRowGap) - scroll, kListW, kRowH}, m_focus, 0, -scroll);
        m_drop.draw(dt, 1.f, animating, 22);
    } else {
        m_drop.hide();
    }
    const time_t now_t = time(nullptr);
    const bool several = m_ctx.services && (m_ctx.services->instances(config::Radarr) > 1 ||
                                            m_ctx.services->instances(config::Sonarr) > 1);
    for (int i = 0; i < n; i++) {
        const float y = kTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH < kTop - 20 || y > gfx::H)
            continue;
        const cleanup::Candidate &c = m_list[i];
        const bool f = focused && i == m_focus;
        art::draw_poster({x0 + 16, y + 12, 64, 96}, {c.service, arr::mediacover_path(c.id, "poster-250.jpg"), c.instance},
                         c.title, seed_of(c.title), 8, 1.f, 120);
        const float tx = x0 + 104, right = x0 + kListW - 32;
        std::string title = c.title + (c.year > 0 ? " (" + std::to_string(c.year) + ")" : "");
        float tw = right - tx - 220;
        gfx::text(tx, y + 46, title, {f ? gfx::Bold : gfx::SemiBold, 30, tw}, kText);
        std::string line = i18n::tr(c.service == config::Sonarr ? i18n::KindSeries : i18n::KindMovie);
        if (several || m_show_kept)
            line += " · " + c.instance_name;
        if (!m_show_kept) {
            line += " · " + fmt::f(i18n::tr(i18n::CleanupWatchedBy), names(c.watched_by).c_str());
            line += " · " + (c.last_played > 0
                                 ? fmt::f(i18n::tr(i18n::CleanupLast), fmt::ago(c.last_played, now_t).c_str())
                                 : std::string(i18n::tr(i18n::CleanupUndated)));
        }
        gfx::text(tx, y + 84, line, {gfx::Regular, 22, right - tx}, kText2);
        if (c.size > 0)
            gfx::text(right, y + 50, fmt::size(c.size), {gfx::Bold, 32}, kText, 2);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
