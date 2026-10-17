/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/jellyfin_screen.h"

#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/tabbar.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kTop = 300;   /* the first section title's baseline (page coordinates) */
constexpr float kSessionH = 156, kTaskH = 84, kGap = 14, kButtonH = 76;
constexpr float kListW = gfx::W - 2 * kPad;
constexpr float kViewTop = 240;   /* where the page shows from (under the title) */

std::string clock_of(int64_t ticks)
{
    const int64_t s = ticks / 10000000;
    if (s >= 3600)
        return fmt::f("%lld:%02lld:%02lld", (long long)(s / 3600), (long long)(s / 60 % 60), (long long)(s % 60));
    return fmt::f("%lld:%02lld", (long long)(s / 60), (long long)(s % 60));
}

std::string media_title(const model::Session &s) { return s.series_name.empty() ? s.item_name : s.series_name; }

} // namespace

JellyfinScreen::JellyfinScreen(Context &ctx) : m_ctx(ctx) {}

void JellyfinScreen::activate()
{
    const double now = platform::now();
    if (m_encoding.state == load::Idle && m_ctx.services) {
        m_encoding.state = load::Loading;
        std::weak_ptr<bool> alive = m_alive;
        m_ctx.services->encoding([this, alive](api::Result<model::EncodingInfo> r) {
            if (!alive.expired())
                m_encoding.take(std::move(r), platform::now());
        });
    }
    if (now - m_last_load > 2)
        refresh_load();
    if (now - m_last_sessions > 2)
        refresh_sessions();
    if (now - m_last_tasks > 5)
        refresh_tasks();
    m_stats.load(false, now);
}

void JellyfinScreen::refresh_sessions()
{
    if (!m_ctx.services || m_sessions.loading())
        return;
    m_last_sessions = platform::now();
    if (!m_sessions.ready())
        m_sessions.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->sessions([this, alive](api::Result<std::vector<model::Session>> r) {
        if (alive.expired())
            return;
        m_sessions.take(std::move(r), platform::now());
        m_ctx.badges[TabJellyfin] = m_sessions.ready() ? badges::sessions(m_sessions.value) : badges::Badge{};
        if (!m_sessions.ready())
            return;
        /* Each transcode's pace, against its lead at the last reading. */
        std::map<std::string, diagnosis::Pace> pace;
        for (const auto &s : m_sessions.value) {
            auto prev = m_pace.find(s.id);
            pace[s.id] = diagnosis::pace(s, prev != m_pace.end() && prev->second.state != diagnosis::Pace::Unknown
                                                ? prev->second.lead
                                                : -1,
                                         m_encoding.value);
        }
        m_pace.swap(pace);
        if (m_diag.is_open() && !session(m_diag.session_id()))
            m_diag.close();   /* that playback ended */
    });
}

void JellyfinScreen::refresh_load()
{
    if (!m_ctx.services || m_load.loading())
        return;
    m_last_load = platform::now();
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->server_load([this, alive](api::Result<model::ServerLoad> r) {
        if (!alive.expired())
            m_load.take(std::move(r), platform::now());
    });
}

const model::Session *JellyfinScreen::session(const std::string &id) const
{
    if (m_sessions.ready())
        for (const auto &s : m_sessions.value)
            if (s.id == id)
                return &s;
    return nullptr;
}

void JellyfinScreen::draw_overlay(float dt, bool *animating)
{
    m_sheet.draw(dt, animating);
    if (const model::Session *s = m_diag.is_open() ? session(m_diag.session_id()) : nullptr) {
        auto p = m_pace.find(s->id);
        m_diag.draw(*s, m_encoding.value, p != m_pace.end() ? p->second : diagnosis::Pace{},
                    m_ctx.services && m_ctx.services->enabled(config::Glances) ? &m_load : nullptr, dt, animating);
    }
}

void JellyfinScreen::refresh_tasks()
{
    if (!m_ctx.services || m_tasks.loading())
        return;
    m_last_tasks = platform::now();
    if (!m_tasks.ready())
        m_tasks.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->tasks([this, alive](api::Result<std::vector<model::Task>> r) {
        if (!alive.expired())
            m_tasks.take(std::move(r), platform::now());
    });
}

void JellyfinScreen::layout()
{
    m_slots.clear();
    float y = kTop + 26;
    const int ns = m_sessions.ready() ? (int)m_sessions.value.size() : 0;
    for (int i = 0; i < ns; i++) {
        m_slots.push_back({Slot::Session, i, y, kSessionH});
        y += kSessionH + kGap;
    }
    if (ns == 0)
        y += 120 + kGap;   /* the empty or loading state */
    y += 16;
    m_slots.push_back({Slot::Scan, 0, y, kButtonH});
    y += kButtonH + 64;
    /* A section: its title, then its rows (or the room of an empty / loading state). */
    auto section = [&](float *title_y, Slot::Kind kind, int n) {
        *title_y = y;
        y += 26;
        for (int i = 0; i < n; i++) {
            m_slots.push_back({kind, i, y, kTaskH});
            y += kTaskH + 8;
        }
        if (n == 0)
            y += 120 + 8;
        y += 56;
    };
    section(&m_libs_y, Slot::Library, (int)m_lib_lines.size());
    section(&m_users_y, Slot::User, m_stats.users().ready() ? (int)m_stats.users().value.size() : 0);
    m_tasks_y = y;
    y += 26;
    const int nt = m_tasks.ready() ? (int)m_tasks.value.size() : 0;
    for (int i = 0; i < nt; i++) {
        m_slots.push_back({Slot::Task, i, y, kTaskH});
        y += kTaskH + 8;
    }
    m_focus = std::clamp(m_focus, 0, (int)m_slots.size() - 1);
}

void JellyfinScreen::confirm_stop(const model::Session &s)
{
    const std::string id = s.id;
    std::string what = media_title(s);
    if (s.episode > 0)
        what += " " + fmt::episode(s.season, s.episode);
    std::string body = fmt::f(i18n::tr(i18n::ConfirmStopBody), what.c_str(), s.device.c_str(), s.user.c_str());
    if (i18n::plex() && m_encoding.ready() && !m_encoding.value.plex_pass)   /* Plex refuses it without Plex Pass, it seems */
        body += std::string(". ") + i18n::tr(i18n::PlexStopNeedsPass);
    m_sheet.open(i18n::tr(i18n::ConfirmStop), body, {i18n::tr(i18n::Cancel), i18n::tr(i18n::StopPlayback)}, 0,
                 [this, id](int choice) {
                     if (choice != 1 || m_busy || !m_ctx.services)
                         return;
                     m_busy = true;
                     std::weak_ptr<bool> alive = m_alive;
                     m_ctx.services->stop_session(id, [this, alive](api::Result<bool> r) {
                         if (alive.expired())
                             return;
                         m_busy = false;
                         if (r.ok())
                             notify(i18n::tr(i18n::DoneStopped), true, platform::now());
                         else
                             notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(r.error, r.http_status).c_str()),
                                    false, platform::now());
                         refresh_sessions();
                     });
                 },
                 true);
}

void JellyfinScreen::confirm_scan()
{
    m_sheet.open(i18n::tr(i18n::ConfirmScan), i18n::tr(i18n::ConfirmScanBody),
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::ScanAction)}, 0, [this](int choice) {
                     if (choice != 1 || m_busy || !m_ctx.services)
                         return;
                     m_busy = true;
                     std::weak_ptr<bool> alive = m_alive;
                     m_ctx.services->refresh_library([this, alive](api::Result<bool> r) {
                         if (alive.expired())
                             return;
                         m_busy = false;
                         if (r.ok())
                             notify(i18n::tr(i18n::DoneScan), true, platform::now());
                         else
                             notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(r.error, r.http_status).c_str()),
                                    false, platform::now());
                         refresh_tasks();
                     });
                 });
}

Action JellyfinScreen::input(const input::State &in)
{
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    if (m_diag.is_open()) {
        const std::string id = m_diag.session_id();
        if (m_diag.input(in) == SessionDiag::Stop)
            if (const model::Session *s = session(id))
                confirm_stop(*s);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        art::retry_failed();
        refresh_sessions();
        refresh_tasks();
        refresh_load();
        m_encoding.state = load::Idle;
        m_stats.load(true, platform::now());
        activate();
        return Action::None;
    }
    const int n = (int)m_slots.size();
    if (p & input::Up) {
        if (m_focus == 0)
            return Action::ToTabs;
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    } else if ((p & input::Cross) && m_focus < n) {
        const Slot &s = m_slots[m_focus];
        if (s.kind == Slot::Session && m_sessions.ready() && s.index < (int)m_sessions.value.size())
            m_diag.open(m_sessions.value[s.index].id);   /* "why does it stutter?", and stop from there */
        else if (s.kind == Slot::Scan && m_ctx.services && m_ctx.services->enabled(config::Jellyfin))
            confirm_scan();
    }
    return Action::None;
}

bool JellyfinScreen::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (!m_focused || m_focus >= (int)m_slots.size() || m_slots[m_focus].kind != Slot::Session ||
        !m_sessions.ready())
        return false;
    const model::Session &s = m_sessions.value[m_slots[m_focus].index];
    const std::string id = s.series_id.empty() ? s.item_id : s.series_id;
    *ref = {config::Jellyfin, m_ctx.services->media_image(id, "Backdrop", 480)};
    *key = "jf:" + id;
    *seed = (int)(seed_of(id) % 997);
    return true;
}

void JellyfinScreen::draw(double now, float dt, float ox, bool *animating)
{
    if (now - m_last_sessions >= 5.0)
        refresh_sessions();
    if (now - m_last_tasks >= 10.0)
        refresh_tasks();
    if (now - m_last_load >= 5.0 && m_ctx.services && m_ctx.services->enabled(config::Glances))
        refresh_load();
    m_stats.load(false, now);
    m_lib_lines = m_stats.library_lines();
    layout();
    const float x0 = kPad + ox;
    gfx::text(x0, 214, i18n::media_server(), {gfx::Bold, 60}, kText);
    /* The server's load, when Glances is set up (else nothing at all). */
    if (m_ctx.services && m_ctx.services->enabled(config::Glances)) {
        if (m_load.ready())
            draw_server_load(x0 + kListW, 172, m_load.value);
        else if (m_load.state == load::Failed)
            gfx::text(x0 + kListW, 200, i18n::tr(i18n::DiagLoadNA), {gfx::Regular, 22}, kText3, 2);
    }

    if (m_sessions.state == load::Off) {
        draw_empty({x0, kTop - 20, kListW, 200}, m_ctx.off_text(i18n::JellyfinOff));
        return;
    }

    /* The page scrolls to keep the focused slot in view. */
    const Slot &fs = m_slots[std::min(m_focus, (int)m_slots.size() - 1)];
    const float page_h = m_slots.back().y + m_slots.back().h + 60;
    const float scroll = list_scroll(m_scroll, fs.y - kViewTop, fs.h, gfx::H - 120 - kViewTop, page_h - kViewTop,
                                     dt, animating);
    const float dy = -scroll;
    gfx::push_scissor({0, kViewTop, gfx::W, gfx::H - 110 - kViewTop});

    /* Panes first, then the drop, then the words. */
    for (const Slot &s : m_slots)
        if (s.kind != Slot::Scan)
            glass_flat({x0, s.y + dy, kListW, s.h}, s.kind == Slot::Session ? 24 : 18, 0.9f);
    const float scan_w = gfx::text_width(i18n::tr(i18n::ScanLibraries), {gfx::SemiBold, 28}) + 90;
    const Slot *scan = nullptr;
    for (const Slot &s : m_slots)
        if (s.kind == Slot::Scan)
            scan = &s;
    if (scan)
        glass_panel({x0, scan->y + dy, scan_w, kButtonH}, kButtonH / 2, 1.f, false);
    if (m_focused) {
        const float w = fs.kind == Slot::Scan ? scan_w : kListW;
        m_drop.to({x0, fs.y + dy, w, fs.h}, m_focus, 0, dy);
        m_drop.draw(dt, 1.f, animating, fs.kind == Slot::Scan ? -1.f : fs.kind == Slot::Session ? 24.f : 18.f);
    } else {
        m_drop.hide();
        m_drop.draw(dt, 1.f, animating);
    }

    /* Now playing. */
    gfx::text(x0, kTop + dy, i18n::tr(i18n::SectionSessions), {gfx::SemiBold, 32}, kText);
    if (!m_sessions.ready() || m_sessions.value.empty()) {
        const gfx::Rect r{x0, kTop + 26 + dy, kListW, 120};
        if (m_sessions.state == load::Failed)
            draw_empty(r, fmt::labelled(i18n::media_server(), fmt::error(m_sessions.error, m_sessions.http_status)),
                       i18n::tr(i18n::Retry), true);
        else if (!m_sessions.ready())
            draw_skeleton(r, 24, now, animating);
        else
            draw_empty(r, i18n::tr(i18n::EmptySessions));
    }
    for (const Slot &slot : m_slots) {
        if (slot.kind != Slot::Session)
            continue;
        const model::Session &s = m_sessions.value[slot.index];
        const float y = slot.y + dy;
        if (y + slot.h < kViewTop || y > gfx::H)
            continue;
        const bool f = m_focused && &slot == &fs;
        const std::string pid = s.series_id.empty() ? s.item_id : s.series_id;
        const std::string title = media_title(s);
        art::draw_poster({x0 + 18, y + 14, 85, 128}, {config::Jellyfin, m_ctx.services->media_image(pid, "Primary", 200)},
                         title, seed_of(title), 10, 1.f, 200);
        const float tx = x0 + 128, right = x0 + kListW - 28;

        /* How it plays: chips on the right. */
        const char *method = i18n::tr(s.method == model::Session::Transcode      ? i18n::Transcode
                                      : s.method == model::Session::DirectStream ? i18n::DirectStream
                                                                                 : i18n::DirectPlay);
        const uint32_t mc = s.method == model::Session::Transcode ? kOrange
                            : s.method == model::Session::DirectStream ? 0xff0a84ffu
                                                                       : kGreen;
        float cx = right - chip_width(method, 22);
        draw_chip(cx, y + 18, method, mc, 1.f, 22);
        if (s.paused) {
            const char *paused = i18n::tr(i18n::PausedChip);
            cx -= chip_width(paused, 22) + 12;
            draw_chip(cx, y + 18, paused, 0xff8e8e93u, 1.f, 22);
        }
        auto pc = m_pace.find(s.id);
        if (pc != m_pace.end() && (pc->second.state == diagnosis::Pace::Behind || pc->second.state == diagnosis::Pace::Limit)) {
            const bool behind = pc->second.state == diagnosis::Pace::Behind;
            const char *t = i18n::tr(behind ? i18n::PaceBehindChip : i18n::PaceLimitChip);
            cx -= chip_width(t, 22) + 12;
            draw_chip(cx, y + 18, t, behind ? kRed : kOrange, 1.f, 22);
        }
        const float text_w = cx - tx - 30;
        gfx::text(tx, y + 50, title, {f ? gfx::Bold : gfx::SemiBold, 32, text_w}, kText);
        std::string line = s.episode > 0 ? fmt::episode(s.season, s.episode) + " · " + s.item_name
                                         : (s.series_name.empty() ? "" : s.item_name);
        std::string who = s.user + " · " + s.device + (s.client.empty() ? "" : " · " + s.client);
        gfx::text(tx, y + 84, line.empty() ? who : line, {gfx::Regular, 24, text_w}, kText2);
        if (!line.empty())
            gfx::text(tx, y + 114, who, {gfx::Regular, 22, kListW - 160}, kText3);
        const float bar_w = kListW - 128 - 260;
        const float prog = s.runtime_ticks > 0 ? (float)((double)s.position_ticks / s.runtime_ticks) : 0.f;
        draw_progress({tx, y + 132, bar_w, 8}, prog, mc);
        if (s.runtime_ticks > 0)
            gfx::text(right, y + 141, clock_of(s.position_ticks) + " / " + clock_of(s.runtime_ticks),
                      {gfx::Medium, 22}, kText2, 2);
        if (s.method == model::Session::Transcode && !s.transcode_reasons.empty())
            gfx::text(right, y + 86, fmt::transcode_reasons(s.transcode_reasons), {gfx::Regular, 22, 520}, kText3, 2);
    }

    /* The library scan. */
    if (scan) {
        const bool f = m_focused && scan == &fs;
        gfx::text(x0 + scan_w / 2, scan->y + dy + 48, i18n::tr(i18n::ScanLibraries),
                  {f ? gfx::Bold : gfx::SemiBold, 28}, f ? kText : kText2, 1);
    }

    /* Scheduled tasks. */
    draw_libraries(x0, dy, fs, now, animating);
    draw_users(x0, dy, fs, now, animating);
    const float tasks_title = m_tasks_y;
    gfx::text(x0, tasks_title + dy, i18n::tr(i18n::SectionTasks), {gfx::SemiBold, 32}, kText);
    if (!m_tasks.ready()) {
        const gfx::Rect r{x0, tasks_title + 26 + dy, kListW, 120};
        if (m_tasks.state == load::Failed)
            draw_empty(r, fmt::labelled(i18n::media_server(), fmt::error(m_tasks.error, m_tasks.http_status)),
                       i18n::tr(i18n::Retry), true);
        else
            draw_skeleton(r, 18, now, animating);
    } else if (m_tasks.value.empty()) {
        draw_empty({x0, tasks_title + 26 + dy, kListW, 120}, i18n::tr(i18n::TasksEmpty));
    }
    const time_t now_t = time(nullptr);
    for (const Slot &slot : m_slots) {
        if (slot.kind != Slot::Task)
            continue;
        const model::Task &t = m_tasks.value[slot.index];
        const float y = slot.y + dy;
        if (y + slot.h < kViewTop || y > gfx::H)
            continue;
        const bool f = m_focused && &slot == &fs;
        const float right = x0 + kListW - 28;
        std::string state;
        uint32_t dot = 0xff8e8e93u;
        if (t.state != model::Task::Idle) {
            state = fmt::f(i18n::tr(i18n::TaskRunning), (int)(t.progress + 0.5));
            dot = 0xff0a84ffu;
            draw_progress({right - 300, y + 58, 300, 6}, (float)(t.progress / 100.0), dot);
        } else if (t.every_days > 0) {   /* Plex's Butler: when it runs, nothing of the last time */
            state = i18n::tr(i18n::TaskScheduled);
        } else if (t.last == model::Task::Never) {
            state = i18n::tr(i18n::TaskNever);
        } else {
            const i18n::S k = t.last == model::Task::Completed ? i18n::ResultCompleted
                              : t.last == model::Task::Failed  ? i18n::ResultFailed
                              : t.last == model::Task::Cancelled ? i18n::ResultCancelled
                                                                 : i18n::ResultAborted;
            state = std::string(i18n::tr(k)) + (t.last_end ? " · " + fmt::ago(t.last_end, now_t) : "");
            dot = t.last == model::Task::Completed ? kGreen : t.last == model::Task::Failed ? kRed : kOrange;
        }
        const float sw = gfx::text_width(state, {gfx::Medium, 24});
        gfx::text(right, y + 46, state, {gfx::Medium, 24}, kText2, 2);
        gfx::fill({right - sw - 26, y + 34, 12, 12}, dot, 6);
        gfx::text(x0 + 30, y + 40, t.name, {f ? gfx::Bold : gfx::SemiBold, 28, kListW - sw - 120}, kText);
        const std::string under = t.every_days > 0 ? fmt::f(i18n::tr(t.every_days == 1 ? i18n::TaskEveryDay : i18n::TaskEveryDays), t.every_days)
                                                   : t.category;
        gfx::text(x0 + 30, y + 70, under, {gfx::Regular, 20, kListW - sw - 120}, kText3);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

void JellyfinScreen::draw_libraries(float x0, float dy, const Slot &fs, double now, bool *animating)
{
    gfx::text(x0, m_libs_y + dy, i18n::tr(i18n::SectionLibraries), {gfx::SemiBold, 32}, kText);
    const auto &libs = m_stats.libraries();
    if (m_lib_lines.empty()) {
        const gfx::Rect r{x0, m_libs_y + 26 + dy, kListW, 120};
        if (libs.state == load::Failed)
            draw_empty(r, fmt::labelled(i18n::media_server(), fmt::error(libs.error, libs.http_status)), i18n::tr(i18n::Retry),
                       true);
        else if (!libs.ready())
            draw_skeleton(r, 18, now, animating);
        else
            draw_empty(r, i18n::tr(i18n::LibrariesEmpty));
        return;
    }
    for (const Slot &slot : m_slots) {
        if (slot.kind != Slot::Library || slot.index >= (int)m_lib_lines.size())
            continue;
        const JellyfinStats::LibraryLine &l = m_lib_lines[slot.index];
        const float y = slot.y + dy;
        if (y + slot.h < kViewTop || y > gfx::H)
            continue;
        const bool f = m_focused && &slot == &fs;
        const float right = x0 + kListW - 28;
        /* Counts and size on the right; skeletons while they come. */
        float rw = 0;
        if (!l.counts.empty()) {
            const std::string text = l.size.empty() ? l.counts : l.counts + " · " + l.size;
            rw = gfx::text_width(text, {gfx::Medium, 24});
            gfx::text(right, y + 40, text, {gfx::Medium, 24}, kText2, 2);
        } else {
            rw = 260;
            draw_skeleton({right - rw, y + 22, rw, 24}, 8, now, animating);
        }
        if (!l.size_note.empty())
            gfx::text(right, y + 70, l.size_note, {gfx::Regular, 20}, kText3, 2);
        const float tw = kListW - std::max(rw, 420.f) - 90;
        gfx::text(x0 + 30, y + (l.kind.empty() ? 52 : 40), l.name, {f ? gfx::Bold : gfx::SemiBold, 28, tw}, kText);
        if (!l.kind.empty())
            gfx::text(x0 + 30, y + 70, l.kind, {gfx::Regular, 20, tw}, kText3);
    }
}

void JellyfinScreen::draw_users(float x0, float dy, const Slot &fs, double now, bool *animating)
{
    gfx::text(x0, m_users_y + dy, i18n::tr(i18n::SectionUsers), {gfx::SemiBold, 32}, kText);
    const auto &users = m_stats.users();
    if (!users.ready() || users.value.empty()) {
        const gfx::Rect r{x0, m_users_y + 26 + dy, kListW, 120};
        if (users.state == load::Failed)
            draw_empty(r, fmt::labelled(i18n::media_server(), fmt::error(users.error, users.http_status)), i18n::tr(i18n::Retry),
                       true);
        else if (!users.ready())
            draw_skeleton(r, 18, now, animating);
        else
            draw_empty(r, i18n::tr(i18n::UsersEmpty));
        return;
    }
    const time_t now_t = time(nullptr);
    for (const Slot &slot : m_slots) {
        if (slot.kind != Slot::User || slot.index >= (int)users.value.size())
            continue;
        const model::JellyfinUser &u = users.value[slot.index];
        const float y = slot.y + dy;
        if (y + slot.h < kViewTop || y > gfx::H)
            continue;
        const bool f = m_focused && &slot == &fs;
        const float right = x0 + kListW - 28;
        const std::string seen =
            u.last_activity ? fmt::f(i18n::tr(i18n::UserSeen), fmt::ago(u.last_activity, now_t).c_str())
                            : std::string(i18n::tr(i18n::UserNever));
        /* Recently active: a green dot (within a day), else grey. */
        const bool recent = u.last_activity && now_t - u.last_activity < 86400;
        const float sw = gfx::text_width(seen, {gfx::Medium, 24});
        gfx::text(right, y + 50, seen, {gfx::Medium, 24}, kText2, 2);
        gfx::fill({right - sw - 26, y + 38, 12, 12}, recent ? kGreen : 0xff8e8e93u, 6);
        const float nw = gfx::text(x0 + 30, y + 52, u.name, {f ? gfx::Bold : gfx::SemiBold, 28, kListW - sw - 400},
                                   u.disabled ? kText3 : kText);
        float cx = x0 + 30 + nw + 18;
        if (u.admin)
            cx += draw_chip(cx, y + 26, i18n::tr(i18n::plex() ? i18n::UserOwner : i18n::UserAdmin), 0xffbf5af2u, 1.f, 22) + 10;
        if (u.disabled)
            draw_chip(cx, y + 26, i18n::tr(i18n::UserDisabled), kRed, 1.f, 22);
    }
}

} // namespace ui
