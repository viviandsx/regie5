/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/arr_lists.h"

#include "platform/platform.h"

namespace ui {

namespace {

/* What a call gives back, made shared (moved, not copied). */
template <typename T> api::Result<Shared<T>> shared(api::Result<std::vector<T>> r)
{
    api::Result<Shared<T>> s;
    s.error = r.error;
    s.http_status = r.http_status;
    if (r.ok())
        s.value = std::make_shared<const std::vector<T>>(std::move(r.value));
    return s;
}

} // namespace

template <typename T, typename Ask>
static void want(std::vector<Part<Shared<T>>> &parts, std::vector<char> &busy, config::Service svc, int instances,
                 bool force, const std::shared_ptr<bool> &alive_token, unsigned *version, Ask ask)
{
    if (parts.empty())
        for (int i = 0; i < instances; i++)
            parts.push_back({svc, i, {}});
    busy.resize(parts.size());
    std::weak_ptr<bool> alive = alive_token;
    bool asked = false;
    for (size_t k = 0; k < parts.size(); k++) {
        Load<Shared<T>> &l = parts[k].load;
        if (busy[k] || !(force || l.state == load::Idle))   /* on its way (a ready one too) */
            continue;
        busy[k] = 1;
        if (!l.ready())   /* a ready list stays shown while it is asked again */
            l.state = load::Loading;
        asked = true;
        ask(parts[k].instance, [&parts, &busy, alive, k, version](api::Result<std::vector<T>> r) {
            if (alive.expired() || k >= parts.size())
                return;
            busy[k] = 0;
            parts[k].load.take(shared(std::move(r)), platform::now());
            ++*version;
        });
    }
    if (asked)
        ++*version;
}

void ArrLists::want_movies(bool force)
{
    if (!m_services)
        return;
    want(m_movies, m_movies_busy, config::Radarr, m_services->instances(config::Radarr), force, m_alive, &m_version,
         [this](int instance, std::function<void(api::Result<std::vector<model::Movie>>)> done) {
             m_services->movies(std::move(done), instance);
         });
}

void ArrLists::want_series(bool force)
{
    if (!m_services)
        return;
    want(m_series, m_series_busy, config::Sonarr, m_services->instances(config::Sonarr), force, m_alive, &m_version,
         [this](int instance, std::function<void(api::Result<std::vector<model::Series>>)> done) {
             m_services->series(std::move(done), instance);
         });
}

void ArrLists::reset()
{
    m_alive = std::make_shared<bool>(true);   /* the old answers find their token gone */
    m_movies.clear();
    m_series.clear();
    m_movies_busy.clear();
    m_series_busy.clear();
    ++m_version;
}

} // namespace ui
