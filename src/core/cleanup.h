/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bibliothèque › Ménage: which films and series could go to free space,
 * apart from any drawing (and so tested). Jellyfin says who watched what and
 * when, and who keeps what as a favourite; Radarr / Sonarr say how big it is.
 * They are matched by TMDB, TVDB and IMDb ids: a title Jellyfin does not know
 * is never offered (no history, no judgement).
 */
#pragma once

#include "core/app_settings.h"
#include "core/model.h"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace cleanup {

/* What one instance of Radarr or Sonarr holds. */
struct Library {
    config::Service service = config::Radarr;
    int instance = 0;
    std::string instance_name;
    std::vector<model::Movie> movies;   /* Radarr */
    std::vector<model::Series> series;  /* Sonarr */
};

struct Criteria {
    bool everyone = true;   /* watched by every active user, else by at least one */
    int months = 6;         /* last watched more than this long ago */
};

struct Candidate {
    config::Service service = config::Radarr;
    int instance = 0;
    std::string instance_name;
    int id = 0;   /* Radarr's movie, Sonarr's series */
    std::string title;
    int year = 0;
    int64_t size = 0;
    std::vector<std::string> watched_by;   /* active users who watched it */
    time_t last_played = 0;                 /* 0: watched, date unknown */
    int tmdb_id = 0;                        /* for Seerr afterwards */
};

/* Does this Jellyfin item stand for that film / series? Films by TMDB, then
 * IMDb; series by TVDB, then TMDB, then IMDb. */
bool same_movie(const model::Movie &m, const model::UserItem &u);
bool same_series(const model::Series &s, const model::UserItem &u);

/* Users counted for "everyone": not disabled, active in the last 6 months,
 * not named in ignore (any case). */
std::vector<model::JellyfinUser> active_users(const std::vector<model::JellyfinUser> &users,
                                              const std::vector<std::string> &ignore, time_t now);

/* The candidates, the biggest first. items: each user's films and series in
 * Jellyfin, by user id (all users: a favourite of anyone keeps a title). */
std::vector<Candidate> candidates(const std::vector<Library> &libraries,
                                  const std::vector<model::JellyfinUser> &users,
                                  const std::map<std::string, std::vector<model::UserItem>> &items,
                                  const std::vector<std::string> &ignore, const Criteria &c,
                                  const app_settings::Settings &kept, time_t now);

} // namespace cleanup
