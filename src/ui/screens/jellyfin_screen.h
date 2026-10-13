/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin: who is watching what, where, and how (Direct Play or transcoding,
 * and why), with a way to stop a playback; a library scan; the libraries
 * (counts, size) and the users (last active); the scheduled tasks and how
 * they last went. Stopping and scanning ask first.
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/screens/jellyfin_stats.h"
#include "ui/screens/session_diag.h"
#include "ui/widgets.h"

#include <memory>
#include <string>
#include <map>
#include <vector>

namespace ui {

class JellyfinScreen : public Screen {
public:
    explicit JellyfinScreen(Context &ctx);

    void activate() override;
    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override { m_focus = 0; }
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_sheet.is_open() || m_diag.is_open(); }
    void draw_overlay(float dt, bool *animating) override;

private:
    /* What can have the focus, top to bottom. */
    struct Slot {
        enum Kind { Session, Scan, Library, User, Task } kind;
        int index;   /* into sessions, libraries, users or tasks */
        float y, h;  /* page coordinates */
    };
    void refresh_sessions();
    void refresh_tasks();
    void refresh_load();
    const model::Session *session(const std::string &id) const;
    void layout();
    void confirm_stop(const model::Session &s);
    void confirm_scan();
    void draw_libraries(float x0, float dy, const Slot &fs, double now, bool *animating);
    void draw_users(float x0, float dy, const Slot &fs, double now, bool *animating);

    Context &m_ctx;
    Load<std::vector<model::Session>> m_sessions;
    Load<std::vector<model::Task>> m_tasks;
    std::vector<Slot> m_slots;
    float m_libs_y = 0, m_users_y = 0, m_tasks_y = 0;   /* the sections' titles (page coordinates) */
    JellyfinStats m_stats{m_ctx};
    std::vector<JellyfinStats::LibraryLine> m_lib_lines;   /* this frame's */
    int m_focus = 0;
    bool m_busy = false;
    double m_last_sessions = -100, m_last_tasks = -100;
    Anim m_scroll;
    Drop m_drop;
    Sheet m_sheet;
    SessionDiag m_diag;
    Load<model::EncodingInfo> m_encoding;
    Load<model::ServerLoad> m_load;   /* Glances, when set up */
    double m_last_load = -100;
    std::map<std::string, diagnosis::Pace> m_pace;   /* per transcoding session */
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
