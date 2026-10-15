/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Seerr (API v1, X-Api-Key): requests, title details and the approve /
 * decline actions. Pictures go through Seerr's own image proxy, so the
 * console never reaches TMDB.
 */
#pragma once

#include "core/model.h"

#include <string>
#include <vector>

namespace seerr {

enum class Filter { Pending, Approved, All };

/* GET: needs a valid key (and answers the version): the connection test. */
std::string about_path();
std::string requests_path(Filter f, int take = 50, int skip = 0);
/* lang: "fr", "en" (titles in that language when TMDB has them). */
/* GET: the requests last changed first (the journal). */
std::string recent_requests_path(int take = 50);
std::string detail_path(bool tv, int tmdb_id, const std::string &lang);
/* POST, no body. */
std::string approve_path(int request_id);
std::string decline_path(int request_id);
/* A TMDB picture through Seerr: size "w342", "w780" ...; empty without a path. */
std::string image_path(const std::string &tmdb_path, const char *size);

/* Issues (Seerr's "Problèmes"). GET: one page, the last changed first. */
enum class IssueFilter { Open, Resolved, All };
std::string issues_path(IssueFilter f, int take = 50, int skip = 0);
/* GET: one issue with its comments' authors (the list leaves them out). */
std::string issue_path(int issue_id);
/* POST, body comment_body(): adds a comment. */
std::string issue_comment_path(int issue_id);
std::string comment_body(const std::string &message);
/* POST, no body: marks the issue resolved. */
std::string issue_resolve_path(int issue_id);

/* Discover, search and new requests (0.5.0). lang: TMDB's language for the
 * names and overviews ("fr", "en" ...). */
enum class Shelf { Trending, PopularMovies, PopularTv, UpcomingMovies, UpcomingTv };
constexpr int kShelves = 5;
std::string discover_path(Shelf s, const std::string &lang, int page = 1);
/* The query escaped for the URL (Seerr wants spaces as %20). */
std::string search_path(const std::string &query, const std::string &lang, int page = 1);
std::string public_settings_path();
/* Radarr (films) or Sonarr (series) servers; one's profiles and folders. */
std::string servers_path(bool tv);
std::string server_path(bool tv, int server_id);
/* POST, body request_body(): a new request (it changes things in Seerr). */
std::string create_request_path();
struct NewRequest {
    int tmdb_id = 0;
    bool tv = false;
    int tvdb_id = 0;             /* series: sent when known */
    std::vector<int> seasons;    /* series: these (never empty) */
    bool is4k = false;
    int server_id = -1, profile_id = -1;   /* -1: Seerr's defaults */
    std::string root_folder;               /* empty: Seerr's default */
};
std::string request_body(const NewRequest &r);

bool parse_about(const std::string &body, std::string *version);
bool parse_requests(const std::string &body, model::RequestPage *out);
bool parse_detail(const std::string &body, bool tv, model::MediaDetail *out);
bool parse_issues(const std::string &body, model::IssuePage *out);
bool parse_issue(const std::string &body, model::Issue *out);
/* Search and discover pages: films and series (people and collections left out). */
bool parse_titles(const std::string &body, std::vector<model::SeerrTitle> *out);
bool parse_title_detail(const std::string &body, bool tv, model::SeerrDetail *out);
bool parse_public(const std::string &body, model::SeerrPublic *out);
bool parse_servers(const std::string &body, std::vector<model::SeerrServer> *out);
bool parse_server(const std::string &body, model::SeerrServer *out);
/* The new request (201), or nothing made (202: nothing left to ask for). An
 * empty body (--mock's 204) reads as sent, with no id. */
bool parse_created(const std::string &body, model::SeerrCreated *out);

} // namespace seerr
