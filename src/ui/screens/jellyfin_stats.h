/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin's libraries (how many films, series, episodes or items, and how
 * much they weigh, from Radarr / Sonarr: core/libraries) and its users (when
 * each was last active). Read-only. Never an address or a device name: the
 * users come from /Users, which carries neither.
 */
#pragma once

#include "core/libraries.h"
#include "core/model.h"
#include "ui/context.h"
#include "ui/load.h"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class JellyfinStats {
public:
    explicit JellyfinStats(Context &ctx) : m_ctx(ctx) {}

    /* Asks what is missing (force: everything again). Users are asked again
     * after a minute; libraries change slowly and wait for △. */
    void load(bool force, double now);

    struct LibraryLine {
        std::string name, kind;   /* "Films", "Séries", "Autres médias" */
        std::string counts;       /* "414 films", "50 séries · 814 épisodes"; empty while counting */
        std::string size;         /* "1,2 To"; empty while measuring or unknown */
        std::string size_note;    /* how the size was had, when not every title could be */
    };
    const Load<std::vector<model::JellyfinLibrary>> &libraries() const { return m_libs; }
    std::vector<LibraryLine> library_lines() const;
    const Load<std::vector<model::JellyfinUser>> &users() const { return m_users; }

private:
    void ask_library(size_t k);
    void ask_arr();
    void remeasure();
    bool arr_done() const;

    Context &m_ctx;
    Load<std::vector<model::JellyfinLibrary>> m_libs;
    struct Data {
        Load<std::vector<model::UserItem>> items;   /* films and series */
        Load<int> extra;                            /* episodes, or items of another kind */
        libraries::Size size;
        bool measured = false;
    };
    std::vector<Data> m_data;   /* by library */
    unsigned m_lists_seen = ~0u;   /* the version of Context::lists measured from */
    Load<std::vector<model::JellyfinUser>> m_users;
    double m_users_at = -100;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
