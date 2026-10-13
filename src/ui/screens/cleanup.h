/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bibliothèque › Ménage (off until turned on in Settings): films and series
 * watched long ago, the biggest first, with the space they would free
 * (core/cleanup decides). Two actions, from ✕ or Options: keep it (never
 * offered again: settings.json), or delete it.
 *
 * Deleting is destructive: two confirmations showing the title, its size and
 * that the files go for good, Cancel first both times; one deletion at a
 * time, never several at once. Radarr / Sonarr delete the title and its files
 * and keep it out of their import lists; then Seerr's record of it is marked
 * deleted, so it may be asked for again.
 */
#pragma once

#include "core/cleanup.h"
#include "core/input.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/hints.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/widgets.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class CleanupView {
public:
    explicit CleanupView(Context &ctx);

    void load(bool force);
    /* Action::ToTabs: up out of the list (to the Library's pills). */
    Action input(const input::State &in);
    void draw(double now, float x0, bool focused, float dt, bool *animating);
    std::vector<PadHint> hints() const;
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;
    bool modal() const { return m_sheet.is_open(); }
    void draw_overlay(float dt, bool *animating) { m_sheet.draw(dt, animating); }

    /* The criteria, as pills on the right of the Library's header, and
     * choosing one (✕ on it). */
    std::vector<std::string> pills() const;
    void choose(int pill);

private:
    struct Source {
        config::Service service;
        int instance;
        Load<std::vector<model::Movie>> movies;
        Load<std::vector<model::Series>> series;
    };
    void rebuild();
    void act();   /* the focused row's actions */
    void keep(const cleanup::Candidate &c);
    void confirm_delete(const cleanup::Candidate &c);
    void do_delete(const cleanup::Candidate &c);

    Context &m_ctx;
    Load<std::vector<model::JellyfinUser>> m_users;
    std::map<std::string, Load<std::vector<model::UserItem>>> m_items;   /* by user id */
    std::vector<Source> m_sources;   /* every Radarr and Sonarr instance */
    std::vector<cleanup::Candidate> m_list;
    int64_t m_total = 0;
    bool m_show_kept = false;   /* the kept titles instead of the candidates */
    int m_focus = 0;
    Anim m_scroll;
    Drop m_drop;
    Sheet m_sheet;
    bool m_busy = false;   /* a deletion on its way: one at a time */
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
