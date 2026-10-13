/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sonarr v4 and Radarr (API v3, X-Api-Key): the paths the app reads and the
 * parsers for their answers. Pure functions: tested on fixtures/.
 */
#pragma once

#include "core/model.h"

#include <string>
#include <vector>

namespace arr {

/* ---- paths (to append to the service's url) ---- */

std::string status_path();   /* /api/v3/system/status */
std::string health_path();
std::string diskspace_path();
/* start, end: UTC; Sonarr with its series, Radarr as it is. */
std::string calendar_path(config::Service s, time_t start, time_t end);
/* The queue with its series and episodes (Sonarr) or films (Radarr). */
std::string queue_path(config::Service s, int page_size = 100);
/* A poster or fanart on the server itself (never TMDB): name "poster-500.jpg",
 * "fanart-360.jpg" ... */
std::string mediacover_path(int media_id, const std::string &name);

/* Libraries (all read-only GETs). */
std::string series_path();                        /* every series, with statistics */
std::string movies_path();                        /* every film */
std::string missing_path(config::Service s, int page_size = 250);   /* wanted/missing, monitored only */
std::string quality_profiles_path();
std::string episode_files_path(int series_id);    /* to tell its files' qualities */

/* ---- parsers: false when the body is not what the endpoint returns ---- */

bool parse_status(const std::string &body, std::string *version);
bool parse_health(const std::string &body, config::Service from, std::vector<model::Health> *out);
bool parse_diskspace(const std::string &body, config::Service from, std::vector<model::DiskSpace> *out);
/* GET: the imports since a time (Sonarr with series and episode, Radarr with the film). */
std::string history_path(config::Service s, time_t since);
/* Only downloadFolderImported events; sourceTitle, data and downloadId are never read. */
bool parse_history_imports(const std::string &body, config::Service from, std::vector<model::ImportEvent> *out);
/* DELETE, with the files, and kept out of import lists (decided: always). */
std::string delete_movie_path(int movie_id);
std::string delete_series_path(int series_id);
bool parse_sonarr_calendar(const std::string &body, std::vector<model::CalendarItem> *out);
/* A film is in the calendar for each of its release dates within [start, end]. */
bool parse_radarr_calendar(const std::string &body, time_t start, time_t end, std::vector<model::CalendarItem> *out);
/* now: for the time left when the server gives an estimated completion time. */
bool parse_queue(const std::string &body, config::Service from, time_t now, std::vector<model::QueueItem> *out,
                 int *total = nullptr);

/* Libraries. Never read: paths, file names, scene names, release groups. */
bool parse_series(const std::string &body, std::vector<model::Series> *out);
bool parse_movies(const std::string &body, std::vector<model::Movie> *out);
bool parse_missing_episodes(const std::string &body, std::vector<model::MissingEpisode> *out, int *total);
/* Radarr's wanted/missing: films, as parse_movies reads them. */
bool parse_missing_movies(const std::string &body, std::vector<model::Movie> *out, int *total);
bool parse_quality_profiles(const std::string &body, std::vector<model::QualityProfile> *out);
bool parse_episode_files(const std::string &body, model::FileSummary *out);

/* What a health check is about, from its source and level (the level tells
 * apart "all" from "one of them" for the status checks). Unknown for a check
 * this version does not know. */
model::Health::Kind health_kind(const std::string &source, model::Health::Level level);

/* Whether a server's text may be shown as it is: nothing that looks like a
 * download client, an indexer or tracker, a path or an address. */
bool shareable(const std::string &text);

} // namespace arr
