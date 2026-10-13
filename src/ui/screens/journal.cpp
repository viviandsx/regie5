/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/journal.h"

#include "api/arr.h"
#include "api/jellyfin.h"
#include "api/seerr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>
#include <set>

namespace ui {

namespace {

constexpr int kDays = 14;
constexpr size_t kMaxEvents = 300;
constexpr float kRowH = 84, kDayH = 52, kRowGap = 8;
constexpr float kListW = gfx::W - 2 * kPad;
constexpr int kMaxItemIds = 120;   /* names asked for at once */

const i18n::S kFilterLabels[5] = {i18n::JFilterAll, i18n::JFilterImports, i18n::JFilterRequests,
                                  i18n::JFilterJellyfin, i18n::JFilterHealth};
const uint32_t kSourceColor[4] = {0xff30d158u, 0xffbf5af2u, 0xff0a84ffu, 0xffff9f0au};

/* Day header rows between the events: "Aujourd'hui", "Hier", "jeu. 9 oct.". */
struct Line {
    bool day = false;
    int event = -1;
    time_t when = 0;
};

std::vector<Line> lines_of(const std::vector<JournalView::Event> &events, const std::vector<int> &shown)
{
    std::vector<Line> out;
    std::string last;
    for (int i : shown) {
        const std::string d = fmt::day(events[i].when, time(nullptr));
        if (d != last) {
            out.push_back({true, -1, events[i].when});
            last = d;
        }
        out.push_back({false, i, events[i].when});
    }
    return out;
}

} // namespace

JournalView::JournalView(Context &ctx) : m_ctx(ctx) {}

std::vector<std::string> JournalView::filter_labels() const
{
    std::vector<std::string> v;
    for (i18n::S s : kFilterLabels)
        v.push_back(i18n::tr(s));
    return v;
}

void JournalView::set_filter(int f)
{
    if (f == m_filter || f < 0 || f > SourceCount)
        return;
    m_filter = f;
    m_focus = 0;
    m_scroll.snap(0);
    rebuild();
}

void JournalView::load(bool force)
{
    api::Services *svc = m_ctx.services;
    if (!svc)
        return;
    std::weak_ptr<bool> alive = m_alive;
    const time_t since = time(nullptr) - kDays * 86400;
    auto stale = [force](const auto &l) { return !l.loading() && (force || l.state == load::Idle); };
    if (m_imports.empty())
        for (config::Service s : {config::Sonarr, config::Radarr})
            for (int i = 0; i < svc->instances(s); i++)
                m_imports.push_back({s, i, {}});
    for (size_t k = 0; k < m_imports.size(); k++) {
        ImportSource &src = m_imports[k];
        if (!stale(src.load))
            continue;
        if (!src.load.ready())
            src.load.state = load::Loading;
        svc->imports(
            src.service, since,
            [this, alive, k](api::Result<std::vector<model::ImportEvent>> r) {
                if (alive.expired() || k >= m_imports.size())
                    return;
                m_imports[k].load.take(std::move(r), platform::now());
                rebuild();
            },
            src.instance);
    }
    if (stale(m_requests)) {
        if (!m_requests.ready())
            m_requests.state = load::Loading;
        svc->recent_requests([this, alive](api::Result<model::RequestPage> r) {
            if (alive.expired())
                return;
            m_requests.take(std::move(r), platform::now());
            rebuild();
        });
    }
    if (stale(m_users)) {
        m_users.state = load::Loading;
        svc->users([this, alive](api::Result<std::vector<model::JellyfinUser>> r) {
            if (alive.expired())
                return;
            m_users.take(std::move(r), platform::now());
            rebuild();
        });
    }
    if (stale(m_activity)) {
        if (!m_activity.ready())
            m_activity.state = load::Loading;
        svc->activity(since, [this, alive](api::Result<std::vector<model::ActivityEntry>> r) {
            if (alive.expired())
                return;
            m_activity.take(std::move(r), platform::now());
            m_items_asked = false;
            ask_items();
            rebuild();
        });
    }
}

/* The names of what the activity log points at, asked for once per reading. */
void JournalView::ask_items()
{
    if (m_items_asked || !m_activity.ready() || !m_ctx.services)
        return;
    m_items_asked = true;
    std::vector<std::string> ids;
    std::set<std::string> seen;
    for (const auto &e : m_activity.value)
        if (!e.item_id.empty() && !m_items.count(e.item_id) && seen.insert(e.item_id).second &&
            (int)ids.size() < kMaxItemIds)
            ids.push_back(e.item_id);
    if (ids.empty())
        return;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->items(ids, [this, alive](api::Result<std::vector<model::JellyfinItem>> r) {
        if (alive.expired() || !r.ok())
            return;
        for (auto &it : r.value)
            m_items[it.id] = std::move(it);
        rebuild();
    });
}

std::string JournalView::user_name(const std::string &id) const
{
    if (m_users.ready())
        for (const auto &u : m_users.value)
            if (u.id == id)
                return u.name;
    return "?";
}

/* A request's title, from Seerr's details (asked for once per media). */
std::string JournalView::request_title(const model::Request &r)
{
    auto it = m_details.find(r.id);
    if (it == m_details.end() && m_ctx.services && r.tmdb_id > 0) {
        Load<model::MediaDetail> &d = m_details[r.id];
        d.state = load::Loading;
        std::weak_ptr<bool> alive = m_alive;
        const int id = r.id;
        m_ctx.services->detail(r.tv, r.tmdb_id, [this, alive, id](api::Result<model::MediaDetail> res) {
            if (alive.expired())
                return;
            m_details[id].take(std::move(res), platform::now());
            rebuild();
        });
        return {};
    }
    return it != m_details.end() && it->second.ready() ? it->second.value.title : std::string();
}

void JournalView::rebuild()
{
    const time_t now = time(nullptr), since = now - kDays * 86400;
    std::vector<Event> ev;

    /* Imports. */
    for (const auto &src : m_imports)
        if (src.load.ready())
            for (const auto &e : src.load.value) {
                if (e.when < since)
                    continue;
                Event x;
                x.source = Imports;
                x.when = e.when;
                x.title = e.title;
                if (e.from == config::Sonarr) {
                    x.title += " · " + fmt::episode(e.season, e.episode);
                    x.line = i18n::tr(i18n::JImportEpisode);
                } else {
                    if (e.year > 0)
                        x.title += " (" + std::to_string(e.year) + ")";
                    x.line = i18n::tr(i18n::JImportMovie);
                }
                if (!e.quality.empty())
                    x.line += " · " + e.quality;
                x.picture = {e.from, arr::mediacover_path(e.media_id, "poster-250.jpg"), e.instance};
                if (m_ctx.services && m_ctx.services->instances(e.from) > 1)   /* which one */
                    x.line += " · " + m_ctx.services->config().at(e.from, e.instance).name;
                x.seed_text = e.title;
                ev.push_back(std::move(x));
            }

    /* Seerr: asked, then where it stands now (Seerr keeps no history). */
    if (m_requests.ready()) {
        const auto &reqs = m_requests.value.requests;
        for (int i = 0; i < (int)reqs.size(); i++) {
            const model::Request &r = reqs[i];
            const time_t last = std::max({r.created, r.updated, r.media_added});
            if (last < since)
                continue;   /* nothing of it in the window: no title to ask for */
            std::string title = request_title(r);
            if (title.empty())
                title = std::string(i18n::tr(r.tv ? i18n::KindSeries : i18n::KindMovie)) + " #" +
                        std::to_string(r.tmdb_id);
            auto add = [&](time_t when, const std::string &line) {
                if (when < since || when <= 0)
                    return;
                Event x;
                x.source = Requests;
                x.when = when;
                x.title = title;
                x.line = line;
                x.request = i;
                x.seed_text = title;
                ev.push_back(std::move(x));
            };
            add(r.created, fmt::f(i18n::tr(i18n::JRequested), r.requested_by.c_str()));
            if (r.media_status == 5 || r.status == model::Request::Completed)
                add(r.media_added > 0 ? r.media_added : r.updated, i18n::tr(i18n::JAvailable));
            else if (r.status == model::Request::Approved && r.updated > r.created)
                add(r.updated, r.modified_by.empty() ? std::string(i18n::tr(i18n::JApproved))
                                                     : fmt::f(i18n::tr(i18n::JApprovedBy), r.modified_by.c_str()));
            else if (r.status == model::Request::Declined)
                add(r.updated, r.modified_by.empty() ? std::string(i18n::tr(i18n::JDeclined))
                                                     : fmt::f(i18n::tr(i18n::JDeclinedBy), r.modified_by.c_str()));
        }
    }

    /* Jellyfin: watching, signing in, new accounts. Sessions starting and
     * ending (every device, all day long) are left out: noise. */
    if (m_activity.ready()) {
        /* Jellyfin logs every pause and resume: the same kind of event, by the
         * same user, on the same item, within 15 minutes of one kept, folds
         * into it (the log is newest first). */
        std::map<std::string, time_t> kept;
        for (const auto &a : m_activity.value) {
            if (a.when < since)
                continue;
            const std::string k = a.type + "|" + a.user_id + "|" + a.item_id;
            auto prev = kept.find(k);
            if (prev != kept.end() && std::llabs((long long)(prev->second - a.when)) < 15 * 60) {
                prev->second = a.when;   /* the run goes on further back */
                continue;
            }
            kept[k] = a.when;
            Event x;
            x.source = Jellyfin;
            x.when = a.when;
            const std::string who = user_name(a.user_id);
            auto item = m_items.find(a.item_id);
            std::string what = i18n::tr(i18n::JUnknownMedia);
            if (item != m_items.end()) {
                const model::JellyfinItem &m = item->second;
                what = m.type == "Episode" && !m.series_name.empty()
                           ? m.series_name + " · " + fmt::episode(m.season, m.episode)
                           : m.name + (m.year > 0 ? " (" + std::to_string(m.year) + ")" : "");
                x.picture = {config::Jellyfin, jellyfin::image_path(m.id, "Primary", 160)};
            }
            if (a.type == "VideoPlayback") {
                x.title = what;
                x.line = fmt::f(i18n::tr(i18n::JPlayStart), who.c_str());
            } else if (a.type == "VideoPlaybackStopped") {
                x.title = what;
                x.line = fmt::f(i18n::tr(i18n::JPlayStop), who.c_str());
            } else if (a.type == "AuthenticationSucceeded") {
                x.title = who;
                x.line = i18n::tr(i18n::JSignIn);
            } else if (a.type == "UserCreated") {
                x.title = who;
                x.line = i18n::tr(i18n::JUserCreated);
            } else {
                continue;   /* other kinds (and failed sign-ins: an address) are not shown */
            }
            x.seed_text = x.title;
            ev.push_back(std::move(x));
        }
    }

    /* Health, as seen during this session. */
    for (const auto &h : m_ctx.health_log) {
        Event x;
        x.source = Health;
        x.when = h.when;
        x.title = m_ctx.services ? m_ctx.services->config().at(h.from, h.instance).name : config::service_name(h.from);
        x.line = fmt::f(i18n::tr(h.gone ? i18n::JHealthGone : i18n::JHealthSeen), fmt::health(h.health).c_str());
        x.seed_text = x.title;
        ev.push_back(std::move(x));
    }
    m_health_seen = m_ctx.health_log.size();

    std::stable_sort(ev.begin(), ev.end(), [](const Event &a, const Event &b) { return a.when > b.when; });
    if (ev.size() > kMaxEvents)
        ev.resize(kMaxEvents);
    m_events = std::move(ev);
    m_shown.clear();
    for (int i = 0; i < (int)m_events.size(); i++)
        if (m_filter == 0 || (int)m_events[i].source == m_filter - 1)
            m_shown.push_back(i);
    m_focus = std::clamp(m_focus, 0, std::max(0, (int)m_shown.size() - 1));
}

Action JournalView::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    const int n = (int)m_shown.size();
    if (p & input::Up) {
        if (m_focus == 0)
            return Action::ToTabs;
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    } else if (p & (input::L2 | input::R2)) {   /* a page */
        m_focus = std::clamp(m_focus + ((p & input::R2) ? 8 : -8), 0, std::max(0, n - 1));
    }
    return Action::None;
}

bool JournalView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (m_focus >= (int)m_shown.size())
        return false;
    const Event &e = m_events[m_shown[m_focus]];
    if (e.picture.empty() || e.picture.service == config::Jellyfin)
        return false;
    *ref = e.picture;
    /* The fanart of a series or a film, from its poster's mediacover path. */
    const size_t slash = ref->path.rfind('/');
    if (slash != std::string::npos)
        ref->path = ref->path.substr(0, slash + 1) + "fanart-360.jpg";
    *key = ref->path + std::to_string((int)ref->service);
    *seed = (int)(seed_of(e.seed_text) % 997);
    return true;
}

void JournalView::draw(double now, float x0, float top, float chips_x, bool focused, float dt, bool *animating)
{
    if (m_health_seen != m_ctx.health_log.size())
        rebuild();
    /* Where it stands: each source that failed, quietly. */
    std::string failures;
    auto failed = [&](const char *who, api::Error e, int st) {
        failures += (failures.empty() ? "" : " · ") + fmt::labelled(who, fmt::error(e, st));
    };
    bool loading = false;
    for (const auto &src : m_imports) {
        loading |= src.load.loading() && !src.load.ready();
        if (src.load.state == load::Failed && m_ctx.services)
            failed(m_ctx.services->config().at(src.service, src.instance).name.c_str(), src.load.error,
                   src.load.http_status);
    }
    loading |= (m_requests.loading() && !m_requests.ready()) || (m_activity.loading() && !m_activity.ready());
    if (m_requests.state == load::Failed)
        failed("Seerr", m_requests.error, m_requests.http_status);
    if (m_activity.state == load::Failed)
        failed("Jellyfin", m_activity.error, m_activity.http_status);
    if (!failures.empty())
        draw_chip(chips_x, 267, failures, kRed, 1.f, 22);

    const gfx::Rect area{x0, top + 10, kListW, 260};
    if (m_shown.empty()) {
        if (loading) {
            for (int i = 0; i < 5; i++)
                draw_skeleton({x0, top + 10 + i * (kRowH + kRowGap), kListW, kRowH}, 20, now, animating);
        } else {
            if (focused) {
                m_drop.to(area, -1);
                m_drop.draw(dt, 0.6f, animating, 24);
            }
            if (m_events.empty() && !failures.empty())
                draw_empty(area, failures, i18n::tr(i18n::Retry), true);
            else
                draw_empty(area, i18n::tr(m_filter == 0 ? i18n::JournalEmpty : i18n::JournalEmptyFilter));
        }
        return;
    }

    /* The rows (day headers between), scrolled to keep the focused event in view. */
    const std::vector<Line> lines = lines_of(m_events, m_shown);
    std::vector<float> ys;
    float y = 0, focus_y = 0;
    for (const auto &l : lines) {
        ys.push_back(y);
        if (!l.day && l.event == m_shown[m_focus])
            focus_y = y;
        y += (l.day ? kDayH : kRowH) + kRowGap;
    }
    const float view_top = top + 10, view_h = gfx::H - 110 - view_top;
    const float scroll = list_scroll(m_scroll, focus_y, kRowH, view_h, y, dt, animating);
    gfx::push_scissor({0, view_top - 10, gfx::W, view_h + 10});
    for (size_t i = 0; i < lines.size(); i++) {
        const float ry = view_top + ys[i] - scroll;
        if (ry + kRowH < view_top - 10 || ry > gfx::H)
            continue;
        if (lines[i].day)
            continue;
        glass_flat({x0, ry, kListW, kRowH}, 18, 0.85f);
    }
    if (focused) {
        m_drop.to({x0, view_top + focus_y - scroll, kListW, kRowH}, m_shown[m_focus]);
        m_drop.draw(dt, 1.f, animating, 18);
    } else {
        m_drop.hide();
    }
    for (size_t i = 0; i < lines.size(); i++) {
        const float ry = view_top + ys[i] - scroll;
        if (ry + kRowH < view_top - 10 || ry > gfx::H)
            continue;
        if (lines[i].day) {
            gfx::text(x0 + 4, ry + 36, fmt::day(lines[i].when, time(nullptr)), {gfx::SemiBold, 26}, kText2);
            continue;
        }
        const Event &e = m_events[lines[i].event];
        const bool f = focused && lines[i].event == m_shown[m_focus];
        /* The time, the source's colour, a small picture, then what happened. */
        gfx::text(x0 + 24, ry + 50, fmt::clock(e.when), {gfx::Medium, 24}, kText3);
        gfx::fill({x0 + 104, ry + 18, 6, kRowH - 36}, kSourceColor[e.source], 3);
        const gfx::Rect thumb{x0 + 126, ry + 10, 43, kRowH - 20};
        if (!e.picture.empty())
            art::draw_poster(thumb, e.picture, e.seed_text, seed_of(e.seed_text), 6, 1.f, 80);
        else
            draw_generated_card(thumb, e.seed_text, seed_of(e.seed_text), 6, 1.f);
        const float tx = thumb.x + thumb.w + 20, tw = x0 + kListW - 30 - tx;
        gfx::text(tx, ry + 36, e.title, {f ? gfx::Bold : gfx::SemiBold, 26, tw}, kText);
        gfx::text(tx, ry + 66, e.line, {gfx::Regular, 22, tw}, kText2);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
