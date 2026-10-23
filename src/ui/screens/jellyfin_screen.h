/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin: who is watching what, where, and how (Direct Play or transcoding,
 * and why), with a way to stop a playback; a library scan; the disks when
 * Scrutiny is set up (a sheet each); the libraries (counts, size) and the
 * users (last active); the scheduled tasks and how they last went. Stopping
 * and scanning ask first. With Playback Reporting, two pills: Serveur (all
 * that) and Statistiques (ui/screens/stats_view).
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/screens/disk_sheet.h"
#include "ui/screens/jellyfin_stats.h"
#include "ui/screens/stats_view.h"
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
    void enter_from_top() override;
    void go_to(int view, time_t when, const std::string &item) override;
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_sheet.is_open() || m_diag.is_open() || m_disk_sheet.is_open(); }
    void draw_overlay(float dt, bool *animating) override;

    enum View { Server = 0, Stats, ViewCount };

private:
    /* What can have the focus, top to bottom. */
    struct Slot {
        enum Kind { Session, Scan, Disk, Library, User, Task } kind;
        int index;   /* into sessions, disks, libraries, users or tasks */
        float y, h;  /* page coordinates */
    };
    void refresh_sessions();
    void refresh_tasks();
    void refresh_load();
    void refresh_disks();
    void open_disk(const std::string &key);
    /* Playback Reporting answered: the pills (Serveur · Statistiques) show. */
    bool pills() const { return m_plugin.ready() && !m_plugin.value.empty(); }
    const model::Session *session(const std::string &id) const;
    void layout();
    void confirm_stop(const model::Session &s);
    void confirm_scan();
    void draw_libraries(float x0, float dy, const Slot &fs, double now, bool *animating);
    void draw_users(float x0, float dy, const Slot &fs, double now, bool *animating);
    void draw_disks(float x0, float dy, const Slot &fs, double now, bool *animating);

    Context &m_ctx;
    Load<std::vector<model::Session>> m_sessions;
    Load<std::vector<model::Task>> m_tasks;
    std::vector<Slot> m_slots;
    float m_page_end = 0;   /* the last section's bottom (page coordinates) */
    float m_top = 300, m_view_top = 240;   /* the first title's baseline, where the page shows from (pills: lower) */
    float m_disks_y = 0, m_libs_y = 0, m_users_y = 0, m_tasks_y = 0;   /* the sections' titles (page coordinates) */
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
    Load<model::DiskReport> m_disks;   /* Scrutiny, when set up */
    double m_last_disks = -1000;
    DiskSheet m_disk_sheet;
    Load<model::DiskDetail> m_disk_detail;
    std::string m_pending_disk;   /* asked from Home before the disks came */
    std::string m_focus_disk;     /* the disk the focus goes to at the next layout */
    Load<std::string> m_plugin;   /* Playback Reporting's version, "" when absent */
    StatsView m_statsv{m_ctx};
    View m_view = Server;
    bool m_bar = false;      /* the focus is on the pills */
    int m_bar_focus = 0;     /* the views, then (Statistiques) its periods */
    Drop m_view_drop, m_period_drop;
    std::map<std::string, diagnosis::Pace> m_pace;   /* per transcoding session */
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
