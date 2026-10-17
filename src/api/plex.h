/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plex Media Server, in the media server's place (docs/PLEX.md). Every
 * request says who Régie5 is (X-Plex-Client-Identifier, -Product,
 * -Version, -Platform, -Device-Name) and asks for JSON; the server's token
 * goes in X-Plex-Token, never in an address. Answers are read into the same
 * models as Jellyfin's, so that every screen stays the same. Items are named
 * by their ratingKey.
 *
 * Never read: Player.address and remotePublicAddress (IP addresses), a
 * history entry's deviceID, Part.file and Location.path (only a library
 * folder's last name).
 */
#pragma once

#include "core/model.h"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace plex {

/* The headers that say who Régie5 is, one "Name: value" each. */
std::vector<std::string> client_headers(const std::string &client_id, const std::string &device_name,
                                        const std::string &version);
/* The token's header ("" without one: Plex's "allowed without auth"). */
std::string token_header(const std::string &token);

/* GET /identity, without a token: the server's id and version. */
std::string identity_path();
bool parse_identity(const std::string &body, std::string *machine_id, std::string *version);
/* GET /: the version (the connection test: it needs the token). */
std::string info_path();
bool parse_info(const std::string &body, std::string *version);
/* Whether the server's owner has Plex Pass (myPlexSubscription). */
bool parse_plex_pass(const std::string &body, bool *plex_pass);

/* An item's poster ("Primary") or backdrop ("Backdrop") through Plex's
 * picture transcoder, which sends it at the size asked for (docs/PLEX.md
 * § 7): never the original. id: a ratingKey. */
std::string image_path(const std::string &id, const char *type, int width);

/* GET (owner): what is playing. Each session's media is what is sent; the
 * source is in the item's metadata (apply_source). */
std::string sessions_path();
bool parse_sessions(const std::string &body, std::vector<model::Session> *out);
/* GET: items by ratingKey, comma-separated (a session's source, journal names). */
std::string metadata_path(const std::vector<std::string> &ids);
/* The file's streams and container, from the item's metadata: the media the
 * session plays (s->media_id), else the first. */
bool apply_source(const std::string &body, model::Session *s);
bool parse_items(const std::string &body, std::vector<model::JellyfinItem> *out);

/* POST (owner; Plex Pass, it seems): ends a playback, the reason shown to the viewer. */
std::string stop_path(const std::string &session_id, const std::string &reason);

/* GET: the libraries. */
std::string sections_path();
bool parse_sections(const std::string &body, std::vector<model::JellyfinLibrary> *out);
/* POST (owner): a library scanned for new files. */
std::string section_refresh_path(const std::string &section_id);
/* GET: a library's latest films (episodes: true, its latest episodes). */
std::string recent_path(const std::string &section_id, bool episodes, int limit);
bool parse_recent(const std::string &body, std::vector<model::RecentItem> *out);
/* Several libraries' latest, newest first; a series' episodes fold into one
 * entry (its newest), counting them, as with Jellyfin. */
std::vector<model::RecentItem> merge_recent(std::vector<model::RecentItem> all, size_t max_items = 20);
/* GET: a library's films and series with their TMDB / TVDB / IMDb ids. */
std::string section_items_path(const std::string &section_id);
bool parse_section_items(const std::string &body, std::vector<model::UserItem> *out);
/* GET: how many items of a kind a library holds ("Episode"; "": its films or
 * series), without the items. */
std::string count_path(const std::string &section_id, const std::string &type);
bool parse_count(const std::string &body, int *count);

/* GET (owner): the Butler's tasks (the enabled ones), and what runs now. */
std::string butler_path();
bool parse_butler(const std::string &body, std::vector<model::Task> *out);
std::string activities_path();
bool parse_activities(const std::string &body, std::vector<model::Task> *out);

/* GET (owner: every account's): finished playbacks since a time, newest
 * first, as "Watched" journal entries. */
std::string history_path(time_t since, int limit = 300);
bool parse_history(const std::string &body, std::vector<model::ActivityEntry> *out);
/* The latest playback of each account in a history answer. */
std::map<std::string, time_t> last_played_by_account(const std::vector<model::ActivityEntry> &history);

/* GET: the server's accounts (the owner is 1). */
std::string accounts_path();
std::string account_path(const std::string &id);
bool parse_accounts(const std::string &body, std::vector<model::JellyfinUser> *out);
bool parse_account(const std::string &body, model::JellyfinUser *out);

/* GET: the server's settings the diagnosis needs (hardware transcoding,
 * the transcoder's throttle). */
std::string prefs_path();
bool parse_prefs(const std::string &body, model::EncodingInfo *out);

} // namespace plex
