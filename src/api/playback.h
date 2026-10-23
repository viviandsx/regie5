/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin's Playback Reporting plugin (optional): viewing statistics,
 * read-only, with the administrator's key Régie5 already has. Only its safe
 * GETs: never user_manage/…, load_backup, save_backup (GETs that write),
 * submit_custom_query (SQL), nor the breakdowns by client or device.
 * https://github.com/jellyfin/jellyfin-plugin-playbackreporting (Api/, Data/)
 */
#pragma once

#include "core/model.h"

#include <ctime>
#include <string>
#include <vector>

namespace playback {

std::string plugins_path();   /* Jellyfin's /Plugins: is it installed and on? */
/* Its version when it is installed and active ("19.0.0.0"), "" when not:
 * false only when the answer is not Jellyfin's list. */
bool parse_plugins(const std::string &body, std::string *version);

/* The days ending today (local date), as the plugin asks them: how many,
 * the last one, the console's offset from UTC in hours. */
std::string period_query(int days, time_t now, long utc_offset_seconds);
std::string users_path(const std::string &period);    /* time and plays per user */
std::string days_path(const std::string &period);     /* time per user and per day */
std::string movies_path(const std::string &period);   /* films by name */
std::string series_path(const std::string &period);   /* series by name */

bool parse_users(const std::string &body, std::vector<model::PlayStats::User> *out);
/* Every user's time added up, day by day (each day of the period, 0 too). */
bool parse_days(const std::string &body, std::vector<std::pair<std::string, int64_t>> *out);
bool parse_titles(const std::string &body, std::vector<model::PlayStats::Title> *out);

} // namespace playback
