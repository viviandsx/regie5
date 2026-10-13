/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A series' or a film's page in the library, read-only: its status, whether it
 * is monitored, its quality profile, size and files' quality, its next airing
 * or releases, and a series' seasons with the episodes there and missing.
 * No search, no change: nothing here asks the server to do anything.
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/art.h"
#include "ui/drop.h"
#include "ui/load.h"

#include <memory>
#include <string>

namespace ui {

class LibraryDetail {
public:
    /* instance: which Sonarr / Radarr, shown when there are several (else empty). */
    void open_series(const model::Series &s, const std::string &profile, api::Services *services,
                     const std::string &instance = {});
    void open_movie(const model::Movie &m, const std::string &profile, const std::string &instance = {});
    bool is_open() const { return m_open; }
    void close() { m_open = false; }
    /* false once ○ closed it. */
    bool input(const input::State &in);
    void draw(double now, float dt, float ox, bool focused, bool *animating);
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;
    /* What is open (Options acts on it). */
    bool is_series() const { return m_tv; }
    const model::Series &series() const { return m_series; }
    const model::Movie &movie() const { return m_movie; }

private:
    bool m_open = false, m_tv = false;
    model::Series m_series;
    model::Movie m_movie;
    std::string m_profile;
    std::string m_instance;
    Load<model::FileSummary> m_files;
    int m_focus = 0;   /* the season with the focus */
    Anim m_scroll, m_show;
    Drop m_drop;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
