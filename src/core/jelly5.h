/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * "Ouvrir dans Jelly5": what Regie5 hands Jelly5 so that it opens on a
 * title's page (docs/JELLY5-PR.md). Two ways, the same request: launch
 * arguments, and a handoff file Jelly5 reads, deletes and ignores after 60 s.
 * Which Jellyfin item a library title is, by the same ids as the cleanup.
 */
#pragma once

#include "core/model.h"

#include <ctime>
#include <string>
#include <vector>

namespace jelly5 {

/* Jelly5's Title ID, and where it looks for the handoff file on the PS5. */
extern const char *const kTitleId;
extern const char *const kHandoffPath;

struct OpenRequest {
    std::string server_id;   /* Jellyfin's /System/Info Id: Jelly5 checks it is its server */
    std::string item_id;
};

/* --open-item <ItemId> --server <ServerId> */
std::vector<std::string> launch_args(const OpenRequest &r);
/* {"serverId", "itemId", "action": "open", "createdAt"} */
std::string handoff_json(const OpenRequest &r, time_t now);

/* The Jellyfin item for a library title; empty when Jellyfin does not have it. */
std::string item_for(const model::Movie &m, const std::vector<model::UserItem> &index);
std::string item_for(const model::Series &s, const std::vector<model::UserItem> &index);

/* The Jellyfin item a recent addition opens: an episode alone, or its series
 * when several of its episodes were folded into the card. */
std::string item_for(const model::RecentItem &r);

/* A Seerr request whose media Jellyfin has (all or part of it, in the
 * request's quality): its item, else empty. */
std::string item_for(const model::Request &r);

} // namespace jelly5
