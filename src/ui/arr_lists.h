/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every Radarr's films and every Sonarr's series, asked for once for all the
 * screens that show them (Library, its cleanup, the Jellyfin tab's library
 * sizes). A big library (5 000 films: about 5 MiB once read) is held once,
 * shared read-only, never once per screen.
 */
#pragma once

#include "api/services.h"
#include "ui/load.h"

#include <memory>
#include <vector>

namespace ui {

template <typename T> using Shared = std::shared_ptr<const std::vector<T>>;

/* Several instances' lists joined (merge_parts' rules); with one instance,
 * the same list, not a copy. */
template <typename T> void join_shared(const std::vector<Part<Shared<T>>> &parts, Load<Shared<T>> *out)
{
    merge_parts(parts, out, [](Shared<T> *v, const Shared<T> &p) {
        if (!p)
            return;
        if (!*v) {
            *v = p;
            return;
        }
        auto all = std::make_shared<std::vector<T>>(**v);
        all->insert(all->end(), p->begin(), p->end());
        *v = std::move(all);
    });
}

/* The list in a Load, or an empty one. */
template <typename T> const std::vector<T> &list_of(const Load<Shared<T>> &l)
{
    static const std::vector<T> kNone;
    return l.value ? *l.value : kNone;
}

class ArrLists {
public:
    explicit ArrLists(api::Services *services) : m_services(services) {}

    /* Asks every instance's list not asked yet; force (△): asked again,
     * unless on its way. A failure waits for a force (never asked every frame). */
    void want_movies(bool force);
    void want_series(bool force);
    /* By instance, in config.json's order (value null until ready). */
    const std::vector<Part<Shared<model::Movie>>> &movies() const { return m_movies; }
    const std::vector<Part<Shared<model::Series>>> &series() const { return m_series; }
    /* Goes up each time a list arrives (or fails): the screens join again. */
    unsigned version() const { return m_version; }
    /* config.json changed: everything forgotten; answers still on their way are dropped. */
    void reset();

private:
    api::Services *m_services;
    std::vector<Part<Shared<model::Movie>>> m_movies;
    std::vector<Part<Shared<model::Series>>> m_series;
    std::vector<char> m_movies_busy, m_series_busy;   /* by instance: asked, no answer yet */
    unsigned m_version = 0;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
