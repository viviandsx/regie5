/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Configuring from a phone (phase 6.5): what the local page shows and what it
 * sends back, without the network (the server is platform/posix/config_server).
 *
 * The page sees each service as a list of instances (name, address, on/off,
 * 4K) and only whether a key is set: keys and passwords never leave the
 * console. What it sends back has the same shape; an empty key keeps the
 * instance's current one ("from" says which instance it was). Everything
 * else in config.json (cleanup, comments) is kept as it is.
 */
#pragma once

#include <string>

namespace config_web {

/* The current config.json (empty or unreadable: none) as the page's JSON,
 * keys left out. */
std::string public_view(const std::string &config_text);

/* The page's answer applied to the current file: *out is the new
 * config.json. false, with *error (for the phone, in English), when the
 * answer is not usable. */
bool apply(const std::string &current_text, const std::string &posted, std::string *out, std::string *error);

/* The page itself (one file: HTML, style and script), in the interface's language. */
std::string page_html();

/* Whether a peer address may use the page: loopback or a private network. */
bool is_local_address(const std::string &ip);

/* config.json's text holds something that is not a configuration Régie5 can
 * read (empty or blank: false, nothing to keep). */
bool unreadable(const std::string &config_text);

} // namespace config_web
