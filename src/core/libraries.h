/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * How much a Jellyfin library weighs. Jellyfin gives no size short of every
 * file's streams: its films and series are found in Radarr / Sonarr instead
 * (TMDB / TVDB / IMDb, as the cleanup does) and their sizes on disk added up.
 * A title in two instances (1080p and 4K) counts the one whose root folder
 * bears the name of one of the library's folders; else the smaller one.
 */
#pragma once

#include "core/model.h"

#include <cstdint>
#include <string>
#include <vector>

namespace libraries {

struct Size {
    int64_t bytes = 0;
    int measured = 0;   /* titles found in Radarr / Sonarr */
    int titles = 0;     /* the library's films and series */
};

/* "movies4k" from "/data/media/movies4k/" (either slash); empty from empty. */
std::string last_name(const std::string &path);

Size measure(const model::JellyfinLibrary &lib, const std::vector<model::UserItem> &items,
             const std::vector<model::Movie> &movies, const std::vector<model::Series> &series);

} // namespace libraries
