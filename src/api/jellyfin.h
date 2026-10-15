/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jellyfin 12 with an API key. The key goes only in
 * Authorization: MediaBrowser Token="..." (X-Emby-Token and ?api_key= are gone).
 * Emby, from which Jellyfin was forked, answers the same paths: its key goes
 * in X-Emby-Token, and the few places where it differs say so (emby).
 */
#pragma once

#include "core/model.h"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace jellyfin {

/* The Authorization header's value for a key. */
std::string auth_value(const std::string &api_key);

std::string info_path();       /* GET /System/Info: needs the key; answers the version */
std::string sessions_path();   /* GET, sessions active in the last 16 minutes */
std::string tasks_path();      /* GET /ScheduledTasks */
/* GET: the latest films and episodes (no user needed, unlike /Items/Latest). */
std::string recent_path(int limit = 40);
/* POST, no body. */
std::string stop_path(const std::string &session_id);
std::string library_refresh_path();
/* An item's image, at a width (Primary, Backdrop, Thumb). */
std::string image_path(const std::string &item_id, const char *type, int width, const std::string &tag = {});

/* GET: the activity log since a time, entries with a user only. */
std::string activity_path(time_t since, int limit = 300);
std::string users_path();
std::string encoding_path();
/* GET: one user's films and series, with what they watched (the cleanup).
 * Emby gives the last played date only when asked for it. */
std::string user_items_path(const std::string &user_id, bool emby = false);
/* GET /Users/{id}: one user. Emby's activity log names users by a number of
 * its own, which only this answers (never in /Users). */
std::string user_path(const std::string &user_id);
/* GET (Emby): one user's watched episodes, dated; parsed into series id ->
 * the latest of them (Emby dates no series). */
std::string played_episodes_path(const std::string &user_id);
bool parse_played_episodes(const std::string &body, std::map<std::string, time_t> *latest);
bool parse_user(const std::string &body, model::JellyfinUser *out);
/* GET: every film and series, with their TMDB / TVDB / IMDb ids and no
 * user's data (Jelly5's links). Parsed by parse_user_items. */
std::string index_path();
bool parse_user_items(const std::string &body, std::vector<model::UserItem> *out);
/* GET /Library/VirtualFolders (admin): name, kind, folders' last names
 * (Locations are read for that alone, never kept whole). */
std::string libraries_path();
bool parse_libraries(const std::string &body, std::vector<model::JellyfinLibrary> *out);
/* GET: a library's films and series with their ids (parse_user_items). */
std::string library_items_path(const std::string &library_id);
/* GET: how many items of a kind a library holds ("Episode"; empty: every
 * item that is not a folder), without the items (Limit=0). */
std::string library_count_path(const std::string &library_id, const std::string &type);
bool parse_count(const std::string &body, int *count);
/* GET /System/Configuration/encoding (admin). Emby says only whether
 * hardware acceleration is off (HardwareAccelerationMode 0). */
bool parse_encoding(const std::string &body, model::EncodingInfo *out);
/* GET: items by id (names for the journal), ids comma-separated. */
std::string items_path(const std::vector<std::string> &ids);

bool parse_info(const std::string &body, std::string *version);
/* /System/Info's Id: the server Jelly5 checks it is signed in to. */
bool parse_server_id(const std::string &body, std::string *id);
/* Type, date, user and item only: Name, ShortOverview and Overview are never
 * read (they hold IP addresses and device names). Emby's kinds of entry
 * ("playback.start"...) are given Jellyfin's names. */
bool parse_activity(const std::string &body, std::vector<model::ActivityEntry> *out);
bool parse_users(const std::string &body, std::vector<model::JellyfinUser> *out);
bool parse_items(const std::string &body, std::vector<model::JellyfinItem> *out);
/* Only the sessions playing something. Emby says which steps of a transcode
 * run on the hardware (VideoPipelineInfo) instead of naming the method. */
bool parse_sessions(const std::string &body, std::vector<model::Session> *out);
/* Without the hidden ones. */
bool parse_tasks(const std::string &body, std::vector<model::Task> *out);
/* Episodes of one series fold into one entry (the newest), counting them. */
bool parse_recent(const std::string &body, std::vector<model::RecentItem> *out, size_t max_items = 20);

/* Quick Connect (Jellyfin 10.9 and later; never on Emby): signing in without
 * an API key, a code approved in Jellyfin on a phone.
 *   GET /QuickConnect/Enabled          -> true / false
 *   POST /QuickConnect/Initiate        -> {Code, Secret}
 *   GET /QuickConnect/Connect?secret=  -> {Authenticated}
 *   POST /Users/AuthenticateWithQuickConnect {"Secret"} -> {AccessToken, User} */
std::string auth_client(const std::string &device_id, const std::string &device_name, const std::string &version,
                        const std::string &token);
std::string quick_connect_enabled_path();
std::string quick_connect_initiate_path();
std::string quick_connect_poll_path(const std::string &secret);
std::string quick_connect_auth_path();
std::string quick_connect_auth_body(const std::string &secret);
bool parse_quick_connect_enabled(const std::string &body, bool *on);
bool parse_quick_connect(const std::string &body, model::QuickConnect *out);
bool parse_quick_connect_poll(const std::string &body, bool *approved);
bool parse_session(const std::string &body, model::JellyfinSession *out);

} // namespace jellyfin
