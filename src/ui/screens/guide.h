/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bibliothèque › Guide: a week like a TV guide, a column a day (Monday to
 * Sunday), Sonarr's episodes and Radarr's releases, each with where it stands
 * (available, upcoming, missing). L2/R2 go a week back / forward. Read-only:
 * ✕ opens the Library's own sheet for the series or the film.
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "core/schedule.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/hints.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/style.h"

#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class GuideView {
public:
    explicit GuideView(Context &ctx);

    /* Asks for the week shown (force: again, △). */
    void load(bool force);
    /* Shows the week holding t (the Home row's ✕), focused on that day. */
    void show_week_of(time_t t);
    /* Action::ToTabs: up out of the grid (to the Library's pills). */
    Action input(const input::State &in);
    void draw(double now, float x0, bool focused, float dt, bool *animating);
    std::vector<PadHint> hints() const;
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;
    /* ✕ was pressed on an entry: the series or the film to open (once). */
    bool take_open(config::Service *s, int *instance, int *media_id);

    struct Entry {
        model::CalendarItem item;
        schedule::State state = schedule::Upcoming;
    };

private:
    void rebuild();
    const Entry *focused_entry() const;

    Context &m_ctx;
    time_t m_monday = 0;
    unsigned m_gen = 0;   /* a newer week wins over an older answer */
    std::vector<Part<std::vector<model::CalendarItem>>> m_cal;   /* every Sonarr, then every Radarr */
    std::vector<Entry> m_days[7];
    int m_day = 0;
    int m_index[7] = {};
    Anim m_scroll[7];
    Drop m_drop;
    Lifts m_lifts;
    bool m_open = false;
    config::Service m_open_service = config::Sonarr;
    int m_open_instance = 0, m_open_id = 0;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
