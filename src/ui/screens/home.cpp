/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/home.h"

#include "api/arr.h"
#include "api/jellyfin.h"
#include "core/jelly5.h"
#include "core/schedule.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/format.h"
#include "ui/hints.h"
#include "ui/poster.h"
#include "ui/screens/library.h"
#include "ui/tabbar.h"

#include <algorithm>
#include <map>
#include <set>

namespace ui {

namespace {

constexpr float kCardW = 200, kCardH = 300, kCardGap = 32;
constexpr float kBannerY = 140, kBannerH = 80;
constexpr float kRowY[2] = {300, 790};   /* the rows' label baselines (page coordinates) */
constexpr float kCardsBelow = 26;        /* from a label's baseline to its cards' top */
constexpr int kSkeletons = 7;

/* An alert, by the source it came from (an index in m_sources), its check and level. */
std::string seen_key(int source, const model::Health &h)
{
    return std::to_string(source) + "|" + h.source + "|" + std::to_string((int)h.level);
}

uint32_t level_color(model::Health::Level l)
{
    return l == model::Health::Error ? kRed : l == model::Health::Warning ? kOrange : 0xff64d2ffu;
}

time_t local_midnight(time_t now)
{
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    return mktime(&tm);
}

} // namespace

HomeScreen::HomeScreen(Context &ctx) : m_ctx(ctx) {}

const std::string &HomeScreen::name_of(const Source &src) const
{
    return m_ctx.services->config().at(src.service, src.instance).name;
}

void HomeScreen::activate()
{
    if (!m_loaded)
        reload();
}

void HomeScreen::reload()
{
    m_loaded = true;
    art::retry_failed();
    api::Services *svc = m_ctx.services;
    if (!svc)
        return;
    std::weak_ptr<bool> alive = m_alive;
    const time_t now = time(nullptr), start = local_midnight(now), end = start + 15 * 86400;
    if (m_sources.empty())
        for (config::Service s : {config::Sonarr, config::Radarr})
            for (int n = 0; n < svc->instances(s); n++)
                m_sources.push_back({s, n, {}, {}, {}});
    for (int i = 0; i < (int)m_sources.size(); i++) {
        Source &src = m_sources[i];
        const config::Service s = src.service;
        const int inst = src.instance;
        src.calendar.state = src.health.state = src.disks.state = load::Loading;
        svc->calendar(
            s, start, end,
            [this, alive, i](api::Result<std::vector<model::CalendarItem>> r) {
                if (alive.expired())
                    return;
                m_sources[i].calendar.take(std::move(r), platform::now());
                rebuild_upcoming();
            },
            inst);
        svc->health(
            s,
            [this, alive, i](api::Result<std::vector<model::Health>> r) {
                if (alive.expired())
                    return;
                Source &src = m_sources[i];
                src.health.take(std::move(r), platform::now());
                if (!src.health.ready())
                    return;
                /* Remember when each alert first showed up; forget the ones that went away. */
                const time_t now = time(nullptr);
                const std::string prefix = std::to_string(i) + "|";
                std::map<std::string, time_t> kept;
                for (auto &kv : m_seen)
                    if (kv.first.rfind(prefix, 0) != 0)
                        kept.insert(kv);
                for (const auto &h : src.health.value) {
                    const std::string k = seen_key(i, h);
                    auto f = m_seen.find(k);
                    kept[k] = f != m_seen.end() ? f->second : now;
                    if (f == m_seen.end())   /* new to this session: the journal's "seen" */
                        m_ctx.health_log.push_back({now, src.service, src.instance, false, h});
                }
                for (const auto &kv : m_seen)   /* gone since the last reading */
                    if (kv.first.rfind(prefix, 0) == 0 && !kept.count(kv.first)) {
                        auto g = m_health_seen.find(kv.first);
                        if (g != m_health_seen.end())
                            m_ctx.health_log.push_back({now, src.service, src.instance, true, g->second});
                    }
                for (const auto &h : src.health.value)
                    m_health_seen[seen_key(i, h)] = h;
                m_seen.swap(kept);
            },
            inst);
        svc->diskspace(
            s,
            [this, alive, i](api::Result<std::vector<model::DiskSpace>> r) {
                if (!alive.expired())
                    m_sources[i].disks.take(std::move(r), platform::now());
            },
            inst);
    }
    m_recent.state = load::Loading;
    svc->recent([this, alive](api::Result<std::vector<model::RecentItem>> r) {
        if (alive.expired())
            return;
        m_recent.take(std::move(r), platform::now());
        rebuild_recent();
    });
}

void HomeScreen::rebuild_upcoming()
{
    std::vector<model::CalendarItem> all;
    for (const auto &src : m_sources)
        if (src.calendar.ready())
            all.insert(all.end(), src.calendar.value.begin(), src.calendar.value.end());
    schedule::drop_duplicates(&all);   /* followed by two instances: once */
    std::stable_sort(all.begin(), all.end(),
                     [](const model::CalendarItem &a, const model::CalendarItem &b) { return a.when < b.when; });
    const time_t now = time(nullptr), from = local_midnight(now), to = from + 15 * 86400;
    Row &row = m_rows[0];
    const std::string focused = row.focus < (int)row.cards.size() ? row.cards[row.focus].key : "";
    row.cards.clear();
    for (const auto &c : all) {
        if (c.when < from || c.when >= to)   /* the next 14 days only, whatever came back */
            continue;
        Card k;
        k.when = c.when;
        k.key = std::string(c.from == config::Sonarr ? "s" : "r") + std::to_string(c.instance) + "." +
                std::to_string(c.media_id) + ":" +
                std::to_string(c.season) + "x" + std::to_string(c.episode) + ":" + std::to_string((long long)c.when);
        k.poster = {c.from, arr::mediacover_path(c.media_id, "poster-500.jpg"), c.instance};
        k.backdrop = {c.from, arr::mediacover_path(c.media_id, "fanart-360.jpg"), c.instance};
        k.title = c.title;
        k.seed = seed_of(c.title);
        const std::string when = c.from == config::Sonarr ? fmt::day(c.when, now) + " · " + fmt::clock(c.when)
                                                          : fmt::day(c.when, now);
        if (c.from == config::Sonarr) {
            k.line1 = fmt::episode(c.season, c.episode) + (c.episode_title.empty() ? "" : " · " + c.episode_title);
        } else {
            k.line1 = i18n::tr(c.release == model::CalendarItem::Cinema    ? i18n::ReleaseCinema
                               : c.release == model::CalendarItem::Digital ? i18n::ReleaseDigital
                                                                           : i18n::ReleasePhysical);
        }
        k.line2 = when;
        row.cards.push_back(k);
    }
    row.focus = 0;
    for (int i = 0; i < (int)row.cards.size(); i++)
        if (row.cards[i].key == focused)
            row.focus = i;
}

void HomeScreen::rebuild_recent()
{
    Row &row = m_rows[1];
    row.cards.clear();
    row.focus = 0;
    if (!m_recent.ready())
        return;
    const time_t now = time(nullptr);
    for (const auto &r : m_recent.value) {
        Card k;
        k.key = r.id;
        k.poster = {config::Jellyfin, jellyfin::image_path(r.image_id, "Primary", 400, r.image_tag)};
        k.backdrop = {config::Jellyfin, jellyfin::image_path(r.backdrop_id, "Backdrop", 480)};
        k.title = r.title;
        k.seed = seed_of(r.title);
        if (r.type == "Episode")
            k.line1 = r.new_episodes > 1 ? fmt::f(i18n::tr(i18n::NewEpisodes), r.new_episodes)
                                         : fmt::episode(r.season, r.episode) + " · " + r.episode_title;
        else
            k.line1 = r.year > 0 ? std::to_string(r.year) : "";
        k.line2 = r.added ? fmt::ago(r.added, now) : "";
        k.jellyfin_id = jelly5::item_for(r);
        row.cards.push_back(k);
    }
}

void HomeScreen::open_alerts()
{
    /* Errors first, then warnings, then notices; each with its service, level and since when. */
    std::vector<std::pair<int, ListSheet::Item>> rows;
    for (int i = 0; i < (int)m_sources.size(); i++) {
        if (!m_sources[i].health.ready())
            continue;
        for (const auto &h : m_sources[i].health.value) {
            ListSheet::Item it;
            it.chips.push_back({name_of(m_sources[i]), 0xff8e8e93u});
            it.chips.push_back({fmt::health_level(h.level), level_color(h.level)});
            it.text = fmt::health(h);
            auto f = m_seen.find(seen_key(i, h));
            if (f != m_seen.end()) {
                /* The servers do not say since when: this is when Régie5 first saw it. */
                const time_t t = f->second, now = time(nullptr);
                struct tm a, b;
                localtime_r(&t, &a);
                localtime_r(&now, &b);
                it.detail = a.tm_yday == b.tm_yday && a.tm_year == b.tm_year
                                ? fmt::f(i18n::tr(i18n::HealthSeenSince), fmt::clock(t).c_str())
                                : fmt::f(i18n::tr(i18n::HealthSeenSinceDay), fmt::day(t, now).c_str(),
                                         fmt::clock(t).c_str());
            }
            const int rank = h.level == model::Health::Error ? 0 : h.level == model::Health::Warning ? 1 : 2;
            rows.push_back({rank, std::move(it)});
        }
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    std::vector<ListSheet::Item> items;
    for (auto &r : rows)
        items.push_back(std::move(r.second));
    const std::string title = std::string(i18n::tr(i18n::HealthTitle)) +
                              (items.empty() ? "" : " (" + std::to_string(items.size()) + ")");
    m_alerts.open(title, std::move(items), i18n::tr(i18n::HealthNoAlert));
}

const HomeScreen::Card *HomeScreen::menu_card() const
{
    if (m_row != 2 || !m_ctx.jelly5.available(m_ctx.services))
        return nullptr;
    const Row &row = m_rows[1];
    if (row.focus >= (int)row.cards.size() || row.cards[row.focus].jellyfin_id.empty())
        return nullptr;
    return &row.cards[row.focus];
}

void HomeScreen::open_menu()
{
    const Card *c = menu_card();
    if (!c)
        return;
    const std::string id = c->jellyfin_id, title = c->title;
    m_menu.open(title, "", {m_ctx.jelly5.label(m_ctx.services), i18n::tr(i18n::Cancel)}, 1, [this, id, title](int choice) {
        if (choice == 0)
            m_ctx.jelly5.open(m_ctx.services, id, title);
    });
}

std::vector<PadHint> HomeScreen::hints() const
{
    if (!m_focused || !menu_card())
        return {};
    return {{PadButton::Options, i18n::tr(i18n::HintOptions)}};
}

Action HomeScreen::input(const input::State &in)
{
    if (m_alerts.is_open()) {
        m_alerts.input(in);
        return Action::None;
    }
    if (m_menu.is_open()) {
        m_menu.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        reload();
        return Action::None;
    }
    if (p & input::Up) {
        if (m_row == 0)
            return Action::ToTabs;
        m_row--;
    } else if (p & input::Down) {
        if (m_row == 2)
            return Action::Bump;
        m_row++;
    } else if (m_row > 0 && (p & (input::Left | input::Right))) {
        Row &row = m_rows[m_row - 1];
        const int n = (int)row.cards.size();
        if (n == 0)
            return Action::Bump;
        if (p & input::Left) {
            if (row.focus == 0)
                return Action::Bump;
            row.focus--;
        } else {
            if (row.focus >= n - 1)
                return Action::Bump;
            row.focus++;
        }
    } else if (m_row == 0 && (p & input::Cross)) {
        open_alerts();
    } else if (p & input::Options) {
        open_menu();
    } else if (m_row == 1 && (p & input::Cross) && m_rows[0].focus < (int)m_rows[0].cards.size()) {
        /* An upcoming episode or film: its week in the Library's guide. */
        m_ctx.go = {TabLibrary, LibraryScreen::Guide, m_rows[0].cards[m_rows[0].focus].when};
    }
    return Action::None;
}

bool HomeScreen::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (!m_focused || m_row == 0)
        return false;
    const Row &row = m_rows[m_row - 1];
    if (row.focus >= (int)row.cards.size())
        return false;
    const Card &c = row.cards[row.focus];
    *key = c.backdrop.path;
    *ref = c.backdrop;
    *seed = (int)(c.seed % 997);
    return true;
}

void HomeScreen::draw_banner(float x0, float y, float dt, bool *animating)
{
    const gfx::Rect r{x0, y, gfx::W - 2 * kPad, kBannerH};
    glass_panel(r, kBannerH / 2, 1.f, false);
    if (m_focused && m_row == 0) {   /* the drop under the banner's words */
        m_drop.to(r, 0);
        m_drop.draw(dt, 1.f, animating);
    }
    /* Disks on the right, each one once: Sonarr and Radarr often see the same
     * filesystem under several paths (same size, same free space). */
    std::vector<model::DiskSpace> disks;
    std::set<std::pair<int64_t, int64_t>> seen;
    for (const Source &src : m_sources)
        if (src.disks.ready())
            for (const auto &d : src.disks.value)
                if (d.total > 0 && seen.insert({d.total >> 24, d.free >> 24}).second)
                    disks.push_back(d);
    std::sort(disks.begin(), disks.end(), [](const model::DiskSpace &a, const model::DiskSpace &b) {
        return (double)a.free / a.total < (double)b.free / b.total;   /* the fullest first */
    });
    const float cy = r.y + r.h / 2;
    float rx = r.x + r.w - 28;
    for (size_t i = 0; i < disks.size() && i < 2; i++) {
        const model::DiskSpace &d = disks[i];
        const float used = 1.f - (float)((double)d.free / d.total);
        const std::string text = fmt::f(i18n::tr(i18n::DiskFree), fmt::size(d.free).c_str(), fmt::size(d.total).c_str());
        const gfx::TextStyle st{gfx::Medium, 22};
        const float tw = gfx::text_width(text, st);
        const uint32_t c = used > 0.95f ? kRed : used > 0.85f ? kOrange : kGreen;
        draw_progress({rx - 90, cy - 4, 90, 8}, used, c);
        gfx::text(rx - 104, cy + 8, text, st, kText2, 2);
        rx -= 104 + tw + 40;
    }

    /* Alerts on the left, in what room is left. */
    int alerts = 0, errors = 0, first_rank = 9;
    std::string first;      /* the most serious one */
    std::string failures;   /* the services whose health could not be read */
    bool loading = false, any = false, all_off = true;
    for (const Source &src : m_sources) {
        const auto &health = src.health;
        loading |= health.loading();
        all_off &= health.state == load::Off;
        if (health.state == load::Failed)
            failures += (failures.empty() ? "" : " · ") +
                        fmt::labelled(name_of(src), fmt::error(health.error, health.http_status));
        if (!health.ready())
            continue;
        any = true;
        for (const auto &h : health.value) {
            alerts++;
            errors += h.level == model::Health::Error;
            const int rank = h.level == model::Health::Error ? 0 : h.level == model::Health::Warning ? 1 : 2;
            if (rank < first_rank) {
                first_rank = rank;
                first = fmt::labelled(name_of(src), fmt::health(h));
            }
        }
    }
    float x = r.x + 24;
    if (all_off) {   /* nothing to watch */
        gfx::text(x, cy + 9, m_ctx.off_text(i18n::BothArrOff), {gfx::Regular, 24, rx - x - 20}, kText2);
    } else if (any || !loading) {
        /* "No alerts" only when every service watched answered. */
        const bool unknown = !failures.empty();
        const std::string label = alerts == 0 && unknown ? i18n::tr(i18n::HealthUnknown)
                                  : alerts == 0          ? i18n::tr(i18n::HealthNoAlert)
                                  : alerts == 1          ? i18n::tr(i18n::HealthOneAlert)
                                                         : fmt::f(i18n::tr(i18n::HealthAlerts), alerts);
        const uint32_t dot = errors || unknown ? kRed : alerts == 0 ? kGreen : kOrange;
        if (first.empty())
            first = failures;
        x += draw_chip(x, cy - 19, label, dot, 1.f, 22) + 18;
        /* ✕ opens the list: said on the banner itself, where the alerts are. */
        const std::string details = i18n::tr(i18n::HealthDetails);
        const float hint_w = alerts > 0 ? pad_hint_width(PadButton::Cross, details, 30) + 24 : 0;
        if (!first.empty() && rx - x - hint_w > 120)
            gfx::text(x, cy + 9, first, {gfx::Regular, 24, rx - x - hint_w - 20}, kText2);
        if (alerts > 0)
            draw_pad_hint(rx - hint_w, cy, PadButton::Cross, details, 30);
    }
}

void HomeScreen::draw_row(int index, Row &row, float x0, float y, double now, float dt, bool *animating)
{
    const bool focused_row = m_focused && m_row == index + 1;
    const char *title = i18n::tr(index == 0 ? i18n::SectionUpcoming : i18n::SectionRecent);
    float lx = x0 + gfx::text(x0, y, title, {gfx::SemiBold, 34}, kText) + 20;
    const float top = y + kCardsBelow;
    const gfx::Rect area{x0, top, gfx::W - 2 * kPad, kCardH};

    /* Where the row's data stands. */
    bool loading = false, all_off = true, all_failed = true;
    std::string failures;
    if (index == 0) {
        for (const Source &src : m_sources) {
            const auto &c = src.calendar;
            loading |= c.loading() || c.state == load::Idle;
            all_off &= c.state == load::Off;
            all_failed &= c.state == load::Failed || c.state == load::Off;
            if (c.state == load::Failed) {
                if (!failures.empty())
                    failures += " · ";
                failures += fmt::labelled(name_of(src), fmt::error(c.error, c.http_status));
            }
        }
        all_failed &= !all_off;
    } else {
        loading = m_recent.loading() || m_recent.state == load::Idle;
        all_off = m_recent.state == load::Off;
        all_failed = m_recent.state == load::Failed;
        if (all_failed)
            failures = fmt::labelled(i18n::media_server(), fmt::error(m_recent.error, m_recent.http_status));
    }

    if (row.cards.empty()) {
        if (focused_row) {   /* over the message, as its pane: a rounded box, not a pill */
            m_drop.to(area, 100 + index);
            m_drop.draw(dt, 0.6f, animating, 24);
        }
        if (loading && !all_failed) {
            for (int i = 0; i < kSkeletons; i++)
                draw_skeleton({x0 + i * (kCardW + kCardGap), top, kCardW, kCardH}, 14, now, animating);
        } else if (all_off) {
            draw_empty(area, m_ctx.off_text(index == 0 ? i18n::BothArrOff : i18n::JellyfinOff));
        } else if (!failures.empty()) {   /* "nothing planned" would not be true */
            draw_empty(area, failures, i18n::tr(i18n::Retry), true);
        } else {
            draw_empty(area, i18n::tr(index == 0 ? i18n::EmptyUpcoming : i18n::EmptyRecent));
        }
        return;
    }
    if (!failures.empty())   /* part of it failed: say so beside the title */
        draw_chip(lx, y - 30, failures, kRed, 1.f, 20);

    const int n = (int)row.cards.size();
    row.focus = std::min(row.focus, n - 1);
    const float row_w = n * kCardW + (n - 1) * kCardGap;
    const float scroll = row_scroll(row.scroll, row_w, gfx::W - 2 * kPad,
                                    row.focus * (kCardW + kCardGap) + kCardW / 2, dt, animating);
    for (int i = 0; i < n; i++) {
        const Card &c = row.cards[i];
        const float x = x0 + i * (kCardW + kCardGap) - scroll;
        const bool f = focused_row && i == row.focus;
        const float lift = row.lifts.step(c.key, f, dt, animating);
        if (x + kCardW < -80 || x > gfx::W + 80)
            continue;
        draw_poster_card({x, top, kCardW, kCardH}, c.poster, c.title, c.seed, lift, 1.f);
        const float ty = top + kCardH + 38 + 12 * lift;
        gfx::text(x, ty, c.title, {f ? gfx::SemiBold : gfx::Medium, 24, kCardW + 10}, f ? kText : kText2);
        if (!c.line1.empty())
            gfx::text(x, ty + 30, c.line1, {gfx::Regular, 21, kCardW + 10}, kText3);
        if (!c.line2.empty())
            gfx::text(x, ty + 58, c.line2, {gfx::Regular, 21, kCardW + 10}, kText3);
    }
}

void HomeScreen::draw(double now, float dt, float ox, bool *animating)
{
    if (!m_loaded)
        reload();
    /* The page slides up when the second row has the focus. */
    m_page.to(m_focused && m_row == 2 ? 430.f : 0.f);
    if (m_page.step(dt, 10.f))
        *animating = true;
    const float x0 = kPad + ox, dy = -m_page.value;

    /* The drop marks the banner and an empty row; cards lift instead. */
    if (!m_focused || (m_row > 0 && !m_rows[m_row - 1].cards.empty()))
        m_drop.hide();
    draw_banner(x0, kBannerY + dy, dt, animating);
    for (int i = 0; i < 2; i++)
        draw_row(i, m_rows[i], x0, kRowY[i] + dy, now, dt, animating);
    if (art::busy())
        *animating = true;
}

} // namespace ui
