/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/jelly5.h"

#include "core/cleanup.h"

#include "cJSON.h"

namespace jelly5 {

const char *const kTitleId = "PPSA99505";
const char *const kHandoffPath = "/data/jelly5/open.json";

std::vector<std::string> launch_args(const OpenRequest &r)
{
    return {"--open-item", r.item_id, "--server", r.server_id};
}

std::string handoff_json(const OpenRequest &r, time_t now)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "serverId", r.server_id.c_str());
    cJSON_AddStringToObject(o, "itemId", r.item_id.c_str());
    cJSON_AddStringToObject(o, "action", "open");
    cJSON_AddNumberToObject(o, "createdAt", (double)now);
    char *text = cJSON_PrintUnformatted(o);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(o);
    return out;
}

std::string item_for(const model::Movie &m, const std::vector<model::UserItem> &index)
{
    for (const auto &u : index)
        if (cleanup::same_movie(m, u))
            return u.id;
    return {};
}

std::string item_for(const model::Series &s, const std::vector<model::UserItem> &index)
{
    for (const auto &u : index)
        if (cleanup::same_series(s, u))
            return u.id;
    return {};
}

std::string item_for(const model::RecentItem &r)
{
    if (r.type == "Episode" && r.new_episodes > 1 && !r.series_id.empty())
        return r.series_id;
    return r.id;
}

std::string item_for(const model::Request &r)
{
    /* MediaStatus: 4 partly available, 5 available. */
    return r.media_status == 4 || r.media_status == 5 ? r.jellyfin_id : std::string();
}

} // namespace jelly5
