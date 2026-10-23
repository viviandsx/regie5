/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin › Statistiques (Playback Reporting, optional): over 7 days, 30
 * days or 12 months, the time watched and the plays, a bar a day (a month),
 * the time per user, the films and series most watched (their posters found
 * by name in Radarr and Sonarr, else a generated card). Read-only, nothing
 * to focus but the pills. A plugin just installed has nothing yet: said so.
 */
#pragma once

#include "core/model.h"
#include "core/stats.h"
#include "ui/art.h"
#include "ui/context.h"
#include "ui/load.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class StatsView {
public:
    explicit StatsView(Context &ctx) : m_ctx(ctx) {}

    /* Asks the period shown, unless it is there and recent (force: again). */
    void load(bool force, double now);
    stats::Period period() const { return m_period; }
    void set_period(int p, double now);
    std::vector<std::string> period_labels() const;
    /* top: under the pills. */
    void draw(double now, float x0, float top, float dt, bool *animating);
    /* The most watched film's backdrop, when Radarr has it. */
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;

private:
    struct Poster {
        config::Service from;
        int instance, id;
    };
    void index_posters();
    const Poster *poster_of(bool movie, const std::string &name) const;
    void draw_bars(const model::PlayStats &s, float x, float y, float w, float h);
    void draw_users(const model::PlayStats &s, float x, float y, float w);
    void draw_titles(const std::vector<model::PlayStats::Title> &t, bool movies, float x, float y, float w);

    Context &m_ctx;
    stats::Period m_period = stats::Week;
    Load<model::PlayStats> m_stats[stats::PeriodCount];
    std::map<std::string, Poster> m_movies, m_series;   /* by lower-case title */
    unsigned m_lists_version = ~0u;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
