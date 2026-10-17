/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plex Media Server, in the media server's place (docs/PLEX.md). Every
 * request says who Régie5 is (X-Plex-Client-Identifier, -Product,
 * -Version, -Platform, -Device-Name) and asks for JSON; the server's token
 * goes in X-Plex-Token, never in an address. Answers are read into the same
 * models as Jellyfin's, so that every screen stays the same.
 */
#pragma once

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

} // namespace plex
