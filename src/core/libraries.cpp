/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/libraries.h"

#include "core/cleanup.h"

#include <algorithm>
#include <map>

namespace libraries {

std::string last_name(const std::string &path)
{
    size_t end = path.find_last_not_of("/\\");
    if (end == std::string::npos)
        return {};
    const size_t start = path.find_last_of("/\\", end);
    return path.substr(start == std::string::npos ? 0 : start + 1, end - (start == std::string::npos ? 0 : start + 1) + 1);
}

namespace {

/* Indexes by id, so that a big library is matched in one pass. */
template <typename T> struct Index {
    std::multimap<int, const T *> tmdb, tvdb;
    std::multimap<std::string, const T *> imdb;
};

template <typename T>
int64_t pick(const std::vector<const T *> &found, const model::JellyfinLibrary &lib)
{
    for (const T *t : found)
        if (!t->root.empty() && std::find(lib.folders.begin(), lib.folders.end(), t->root) != lib.folders.end())
            return t->size;
    int64_t smallest = found.front()->size;
    for (const T *t : found)
        smallest = std::min(smallest, t->size);
    return smallest;
}

template <typename T, typename Same>
std::vector<const T *> find(const Index<T> &ix, const model::UserItem &u, Same same)
{
    std::vector<const T *> out;
    auto add = [&](auto range) {
        for (auto it = range.first; it != range.second; ++it)
            if (same(*it->second, u) && std::find(out.begin(), out.end(), it->second) == out.end())
                out.push_back(it->second);
    };
    if (u.tmdb_id > 0)
        add(ix.tmdb.equal_range(u.tmdb_id));
    if (u.tvdb_id > 0)
        add(ix.tvdb.equal_range(u.tvdb_id));
    if (!u.imdb_id.empty())
        add(ix.imdb.equal_range(u.imdb_id));
    return out;
}

} // namespace

Size measure(const model::JellyfinLibrary &lib, const std::vector<model::UserItem> &items,
             const std::vector<model::Movie> &movies, const std::vector<model::Series> &series)
{
    Index<model::Movie> mi;
    for (const model::Movie &m : movies) {
        if (m.tmdb_id > 0)
            mi.tmdb.insert({m.tmdb_id, &m});
        if (!m.imdb_id.empty())
            mi.imdb.insert({m.imdb_id, &m});
    }
    Index<model::Series> si;
    for (const model::Series &s : series) {
        if (s.tvdb_id > 0)
            si.tvdb.insert({s.tvdb_id, &s});
        if (s.tmdb_id > 0)
            si.tmdb.insert({s.tmdb_id, &s});
        if (!s.imdb_id.empty())
            si.imdb.insert({s.imdb_id, &s});
    }
    Size out;
    for (const model::UserItem &u : items) {
        out.titles++;
        if (u.series) {
            const auto found = find(si, u, cleanup::same_series);
            if (!found.empty())
                out.bytes += pick(found, lib), out.measured++;
        } else {
            const auto found = find(mi, u, cleanup::same_movie);
            if (!found.empty())
                out.bytes += pick(found, lib), out.measured++;
        }
    }
    return out;
}

} // namespace libraries
