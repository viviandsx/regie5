/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Activité › Journal: one timeline of the last 14 days, newest first, grouped
 * by day: imports (Sonarr/Radarr history, imports only, never what was
 * grabbed), Seerr requests (asked, approved, declined, available, with who),
 * Jellyfin's activity (watching, signing in, accounts) and the health alerts
 * seen or gone during this session. Filtered by source.
 *
 * Every sentence is the app's own: no text is copied from a server's log
 * (Jellyfin's holds addresses and device names), and no release name is read.
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class JournalView {
public:
    enum Source { Imports = 0, Requests, Jellyfin, Health, SourceCount };

    explicit JournalView(Context &ctx);

    void load(bool force);
    /* The filters' labels (Tout, Imports, Demandes, Jellyfin, Santé) and the one chosen. */
    std::vector<std::string> filter_labels() const;
    int filter() const { return m_filter; }
    void set_filter(int f);
    /* Action::ToTabs: up out of the list (to the pills). */
    Action input(const input::State &in);
    /* top: the list's top; chips_x: where on the pills' row the failures go. */
    void draw(double now, float x0, float top, float chips_x, bool focused, float dt, bool *animating);
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;

    struct Event {
        Source source = Imports;
        time_t when = 0;
        std::string title, line;
        art::Ref picture;        /* a poster, or none */
        std::string seed_text;   /* for the generated card */
        int request = -1;        /* Seerr: the request (its title comes later) */
    };

private:
    void rebuild();
    std::string request_title(const model::Request &r);
    std::string user_name(const std::string &id) const;
    void ask_items();

    Context &m_ctx;
    struct ImportSource {
        config::Service service;
        int instance;
        Load<std::vector<model::ImportEvent>> load;
    };
    std::vector<ImportSource> m_imports;   /* every Sonarr and Radarr instance */
    Load<model::RequestPage> m_requests;
    Load<std::vector<model::ActivityEntry>> m_activity;
    Load<std::vector<model::JellyfinUser>> m_users;
    std::map<std::string, model::JellyfinItem> m_items;   /* named by the activity log */
    bool m_items_asked = false;
    std::map<int, Load<model::MediaDetail>> m_details;   /* Seerr titles, by request */
    std::vector<Event> m_events;                          /* all, newest first */
    std::vector<int> m_shown;                             /* those of the filter */
    int m_filter = 0;                                     /* 0 all, else Source + 1 */
    int m_focus = 0;
    Anim m_scroll;
    Drop m_drop;
    size_t m_health_seen = 0;   /* health_log entries already in */
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
