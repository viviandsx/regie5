/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/jellyfin_stats.h"

#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/format.h"

#include <algorithm>
#include <cctype>

namespace ui {

void JellyfinStats::load(bool force, double now)
{
    if (!m_ctx.services)
        return;
    if (m_ctx.lists && m_libs.ready() && m_ctx.lists->version() != m_lists_seen) {   /* a list arrived */
        m_lists_seen = m_ctx.lists->version();
        remeasure();
    }
    std::weak_ptr<bool> alive = m_alive;
    if ((force || now - m_users_at > 60) && !m_users.loading()) {
        m_users_at = now;
        if (!m_users.ready())
            m_users.state = load::Loading;
        m_ctx.services->users([this, alive](api::Result<std::vector<model::JellyfinUser>> r) {
            if (alive.expired())
                return;
            m_users.take(std::move(r), platform::now());
            /* The last active first; disabled accounts at the end. */
            std::stable_sort(m_users.value.begin(), m_users.value.end(),
                             [](const model::JellyfinUser &a, const model::JellyfinUser &b) {
                                 if (a.disabled != b.disabled)
                                     return b.disabled;
                                 return a.last_activity > b.last_activity;
                             });
        });
    }
    if ((force || m_libs.state == load::Idle) && !m_libs.loading()) {
        if (!m_libs.ready())
            m_libs.state = load::Loading;
        if (force && m_ctx.lists) {   /* the sizes are asked again too */
            m_ctx.lists->want_movies(true);
            m_ctx.lists->want_series(true);
        }
        m_ctx.services->libraries([this, alive](api::Result<std::vector<model::JellyfinLibrary>> r) {
            if (alive.expired())
                return;
            m_libs.take(std::move(r), platform::now());
            if (!m_libs.ready())
                return;
            m_data.assign(m_libs.value.size(), Data{});
            for (size_t k = 0; k < m_libs.value.size(); k++)
                ask_library(k);
            ask_arr();
        });
    }
}

void JellyfinStats::ask_library(size_t k)
{
    const model::JellyfinLibrary lib = m_libs.value[k];
    std::weak_ptr<bool> alive = m_alive;
    api::Services &svc = *m_ctx.services;
    const size_t n = m_data.size();
    /* Films and series are matched for the size; collections hold films
     * already counted in their own library, so only the collections are counted. */
    if (lib.kind == model::JellyfinLibrary::Movies || lib.kind == model::JellyfinLibrary::Shows) {
        m_data[k].items.state = load::Loading;
        svc.library_items(lib.id, [this, alive, k, n](api::Result<std::vector<model::UserItem>> r) {
            if (alive.expired() || m_data.size() != n)   /* the list was read again meanwhile */
                return;
            m_data[k].items.take(std::move(r), platform::now());
            remeasure();
        });
    }
    if (lib.kind != model::JellyfinLibrary::Movies) {
        m_data[k].extra.state = load::Loading;
        svc.library_count(lib.id,
                          lib.kind == model::JellyfinLibrary::Shows         ? "Episode"
                          : lib.kind == model::JellyfinLibrary::Collections ? "BoxSet"
                                                                            : "",
                          [this, alive, k, n](api::Result<int> r) {
                              if (!alive.expired() && m_data.size() == n)
                                  m_data[k].extra.take(std::move(r), platform::now());
                          });
    }
}

void JellyfinStats::ask_arr()
{
    /* Shared with the Library (Context::lists): asked once for both. */
    if (!m_ctx.lists)
        return;
    m_ctx.lists->want_movies(false);
    m_ctx.lists->want_series(false);
    m_lists_seen = m_ctx.lists->version();
    remeasure();
}

bool JellyfinStats::arr_done() const
{
    if (!m_ctx.lists)
        return true;
    for (const auto &p : m_ctx.lists->movies())
        if (p.load.loading() || p.load.state == load::Idle)
            return false;
    for (const auto &p : m_ctx.lists->series())
        if (p.load.loading() || p.load.state == load::Idle)
            return false;
    return true;
}

void JellyfinStats::remeasure()
{
    if (!arr_done() || !m_libs.ready())
        return;
    /* Every instance's titles, once (a few thousand: matched by index, not
     * per frame); with one instance each, the shared lists themselves. */
    Load<Shared<model::Movie>> movies_all;
    Load<Shared<model::Series>> series_all;
    if (m_ctx.lists) {
        join_shared(m_ctx.lists->movies(), &movies_all);
        join_shared(m_ctx.lists->series(), &series_all);
    }
    const bool any = movies_all.value || series_all.value;
    const std::vector<model::Movie> &movies = list_of(movies_all);
    const std::vector<model::Series> &series = list_of(series_all);
    for (size_t k = 0; k < m_data.size() && k < m_libs.value.size(); k++) {
        Data &d = m_data[k];
        if (d.measured || !d.items.ready())
            continue;
        d.measured = any;   /* no Radarr / Sonarr at all: no size */
        if (any)
            d.size = libraries::measure(m_libs.value[k], d.items.value, movies, series);
    }
}

std::vector<JellyfinStats::LibraryLine> JellyfinStats::library_lines() const
{
    std::vector<LibraryLine> out;
    if (!m_libs.ready())
        return out;
    for (size_t k = 0; k < m_libs.value.size(); k++) {
        const model::JellyfinLibrary &lib = m_libs.value[k];
        const Data *d = k < m_data.size() ? &m_data[k] : nullptr;
        LibraryLine line;
        line.name = lib.name;
        line.kind = i18n::tr(lib.kind == model::JellyfinLibrary::Movies        ? i18n::ViewMovies
                             : lib.kind == model::JellyfinLibrary::Shows       ? i18n::ViewSeries
                             : lib.kind == model::JellyfinLibrary::Collections ? i18n::LibKindCollections
                                                                               : i18n::LibKindOther);
        if (d) {
            const int titles = d->items.ready() ? (int)d->items.value.size() : -1;
            const int extra = d->extra.ready() ? d->extra.value : -1;
            if (lib.kind == model::JellyfinLibrary::Movies && titles >= 0)
                line.counts = fmt::count(i18n::CountMoviesOne, i18n::CountMovies, titles);
            else if (lib.kind == model::JellyfinLibrary::Shows && titles >= 0)
                line.counts = fmt::count(i18n::CountSeriesOne, i18n::CountSeries, titles) +
                              (extra >= 0 ? " · " + fmt::count(i18n::LibEpisodesOne, i18n::LibEpisodes, extra) : "");
            else if (lib.kind == model::JellyfinLibrary::Collections && extra >= 0)
                line.counts = fmt::count(i18n::LibCollectionsOne, i18n::LibCollections, extra);
            else if (lib.kind == model::JellyfinLibrary::Other && extra >= 0)
                line.counts = fmt::count(i18n::LibItemsOne, i18n::LibItems, extra);
            if (d->measured && d->size.measured > 0) {
                line.size = fmt::size(d->size.bytes);
                line.size_note = d->size.measured < d->size.titles
                                     ? fmt::f(i18n::tr(i18n::LibSizePartial), d->size.measured, d->size.titles)
                                     : i18n::tr(i18n::LibSizeFrom);
            }
        }
        /* "Films" named "Films": the kind would only say it again. */
        auto lower = [](std::string s) {
            for (char &ch : s)
                ch = (char)std::tolower((unsigned char)ch);
            return s;
        };
        if (lower(line.kind) == lower(line.name))
            line.kind.clear();
        out.push_back(std::move(line));
    }
    return out;
}

} // namespace ui
