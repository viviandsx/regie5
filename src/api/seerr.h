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

bool parse_about(const std::string &body, std::string *version);
bool parse_requests(const std::string &body, model::RequestPage *out);
bool parse_detail(const std::string &body, bool tv, model::MediaDetail *out);
bool parse_issues(const std::string &body, model::IssuePage *out);
bool parse_issue(const std::string &body, model::Issue *out);

} // namespace seerr
