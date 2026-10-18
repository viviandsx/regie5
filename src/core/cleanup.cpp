/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/cleanup.h"

#include <algorithm>
#include <cctype>

namespace cleanup {

namespace {

std::string lower(const std::string &s)
{
    std::string out;
    for (char c : s)
        out += (char)std::tolower((unsigned char)c);
    return out;
}

/* What every user's Jellyfin says of one title. */
struct Seen {
    bool known = false;      /* Jellyfin has it (for anyone) */
    bool favourite = false;  /* anyone's favourite */
    std::vector<std::string> by;   /* active users who watched it all */
    time_t last = 0;         /* the latest play, over everyone (0: never dated) */
};

template <typename Match>
Seen seen(const std::vector<model::JellyfinUser> &users, const std::vector<model::JellyfinUser> &active,
          const std::map<std::string, std::vector<model::UserItem>> &items, Match match)
{
    Seen s;
    for (const auto &u : users) {
        auto it = items.find(u.id);
        if (it == items.end())
            continue;
        const bool counted =
            std::any_of(active.begin(), active.end(), [&u](const model::JellyfinUser &a) { return a.id == u.id; });
        for (const auto &i : it->second) {
            if (!match(i))
                continue;
            s.known = true;
            s.favourite |= i.favorite;
            s.last = std::max(s.last, i.last_played);
            const bool all = i.played || (i.series && i.unplayed == 0);
            if (all && counted && std::find(s.by.begin(), s.by.end(), u.name) == s.by.end())
                s.by.push_back(u.name);
        }
    }
    return s;
}

bool qualifies(const Seen &s, size_t active, const Criteria &c, time_t now)
{
    if (!s.known || s.favourite || s.by.empty())
        return false;
    if (c.everyone && s.by.size() < active)
        return false;
    /* Watched recently by anyone (the latest play over everyone): it stays. */
    const time_t limit = now - (time_t)c.months * 30 * 86400;
    return s.last == 0 || s.last < limit;
}

} // namespace

bool same_movie(const model::Movie &m, const model::UserItem &u)
{
    if (u.series)
        return false;
    if (m.tmdb_id > 0 && u.tmdb_id == m.tmdb_id)
        return true;
    return !m.imdb_id.empty() && u.imdb_id == m.imdb_id;
}

bool same_series(const model::Series &s, const model::UserItem &u)
{
    if (!u.series)
        return false;
    if (s.tvdb_id > 0 && u.tvdb_id == s.tvdb_id)
        return true;
    if (s.tmdb_id > 0 && u.tmdb_id == s.tmdb_id)
        return true;
    return !s.imdb_id.empty() && u.imdb_id == s.imdb_id;
}

std::vector<model::JellyfinUser> active_users(const std::vector<model::JellyfinUser> &users,
                                              const std::vector<std::string> &ignore, time_t now)
{
    std::vector<model::JellyfinUser> out;
    for (const auto &u : users) {
        if (u.disabled || u.last_activity < now - 182 * 86400)
            continue;
        const std::string name = lower(u.name);
        if (std::any_of(ignore.begin(), ignore.end(), [&name](const std::string &i) { return lower(i) == name; }))
            continue;
        out.push_back(u);
    }
    return out;
}

std::vector<Candidate> candidates(const std::vector<Library> &libraries,
                                  const std::vector<model::JellyfinUser> &users,
                                  const std::map<std::string, std::vector<model::UserItem>> &items,
                                  const std::vector<std::string> &ignore, const Criteria &c,
                                  const app_settings::Settings &kept, time_t now)
{
    std::vector<Candidate> out;
    const std::vector<model::JellyfinUser> active = active_users(users, ignore, now);
    if (active.empty())
        return out;   /* nobody to judge by */
    for (const Library &lib : libraries) {
        static const std::vector<model::Movie> kNoMovies;
        static const std::vector<model::Series> kNoSeries;
        for (const auto &m : lib.movies ? *lib.movies : kNoMovies) {
            if (m.size <= 0 || kept.is_kept(config::Radarr, lib.instance_name, m.id))
                continue;
            const Seen s = seen(users, active, items, [&m](const model::UserItem &u) { return same_movie(m, u); });
            if (!qualifies(s, active.size(), c, now))
                continue;
            out.push_back({config::Radarr, lib.instance, lib.instance_name, m.id, m.title, m.year, m.size, s.by,
                           s.last, m.tmdb_id});
        }
        for (const auto &x : lib.series ? *lib.series : kNoSeries) {
            if (x.size <= 0 || x.status != model::Series::Ended ||
                kept.is_kept(config::Sonarr, lib.instance_name, x.id))
                continue;
            const Seen s = seen(users, active, items, [&x](const model::UserItem &u) { return same_series(x, u); });
            if (!qualifies(s, active.size(), c, now))
                continue;
            out.push_back({config::Sonarr, lib.instance, lib.instance_name, x.id, x.title, x.year, x.size, s.by,
                           s.last, x.tmdb_id});
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const Candidate &a, const Candidate &b) { return a.size > b.size; });
    return out;
}

} // namespace cleanup
