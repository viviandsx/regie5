/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/seerr.h"

#include "api/json.h"

#include <cstdlib>

namespace seerr {

std::string about_path() { return "/api/v1/settings/about"; }

std::string requests_path(Filter f, int take, int skip)
{
    const char *filter = f == Filter::Pending ? "pending" : f == Filter::Approved ? "approved" : "all";
    return "/api/v1/request?take=" + std::to_string(take) + "&skip=" + std::to_string(skip) + "&filter=" + filter +
           "&sort=added&sortDirection=desc";
}

std::string recent_requests_path(int take)
{
    return "/api/v1/request?take=" + std::to_string(take) + "&skip=0&filter=all&sort=modified&sortDirection=desc";
}

std::string detail_path(bool tv, int tmdb_id, const std::string &lang)
{
    return std::string(tv ? "/api/v1/tv/" : "/api/v1/movie/") + std::to_string(tmdb_id) + "?language=" + lang;
}


std::string approve_path(int request_id) { return "/api/v1/request/" + std::to_string(request_id) + "/approve"; }
std::string decline_path(int request_id) { return "/api/v1/request/" + std::to_string(request_id) + "/decline"; }

std::string image_path(const std::string &tmdb_path, const char *size)
{
    if (tmdb_path.empty() || tmdb_path[0] != '/')
        return {};
    return std::string("/imageproxy/tmdb/t/p/") + size + tmdb_path;
}

bool parse_about(const std::string &body, std::string *version)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *version = json::str(d.root(), "version");
    return !version->empty();
}

bool parse_requests(const std::string &body, model::RequestPage *out)
{
    json::Doc d(body);
    const cJSON *results = json::get(d.root(), "results");
    if (!cJSON_IsArray(results))
        return false;
    *out = {};
    const cJSON *info = json::get(d.root(), "pageInfo");
    out->total = json::integer(info, "results");
    out->pages = json::integer(info, "pages");
    out->page = json::integer(info, "page");
    const cJSON *it;
    cJSON_ArrayForEach(it, results)
    {
        model::Request r;
        r.id = json::integer(it, "id");
        const int st = json::integer(it, "status", 1);
        r.status = st >= 1 && st <= 5 ? (model::Request::Status)st : model::Request::Pending;
        r.tv = json::str(it, "type") == "tv";
        r.is4k = json::boolean(it, "is4k");
        r.created = json::parse_time(json::str(it, "createdAt"));
        r.updated = json::parse_time(json::str(it, "updatedAt"));
        const cJSON *media = json::get(it, "media");
        r.tmdb_id = json::integer(media, "tmdbId");
        /* A 4K request follows the media's 4K side (its own status, maybe its
         * own Jellyfin item). */
        r.media_status = json::integer(media, r.is4k ? "status4k" : "status");
        r.media_added = json::parse_time(json::str(media, "mediaAddedAt"));
        r.jellyfin_id = json::str(media, r.is4k ? "jellyfinMediaId4k" : "jellyfinMediaId");
        if (r.is4k && r.jellyfin_id.empty())
            r.jellyfin_id = json::str(media, "jellyfinMediaId");
        const cJSON *mod = json::get(it, "modifiedBy");
        r.modified_by = json::str(mod, "displayName");
        if (r.modified_by.empty())
            r.modified_by = json::str(mod, "jellyfinUsername", json::str(mod, "username"));
        const cJSON *by = json::get(it, "requestedBy");
        r.requested_by = json::str(by, "displayName");
        if (r.requested_by.empty())
            r.requested_by = json::str(by, "jellyfinUsername", json::str(by, "username"));
        const cJSON *seasons = json::get(it, "seasons");
        const cJSON *s;
        cJSON_ArrayForEach(s, seasons) r.seasons.push_back(json::integer(s, "seasonNumber"));
        out->requests.push_back(r);
    }
    return true;
}

bool parse_detail(const std::string &body, bool tv, model::MediaDetail *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *out = {};
    out->tv = tv;
    out->tmdb_id = json::integer(d.root(), "id");
    out->title = json::str(d.root(), tv ? "name" : "title");
    const std::string date = json::str(d.root(), tv ? "firstAirDate" : "releaseDate");
    out->year = date.size() >= 4 ? std::atoi(date.substr(0, 4).c_str()) : 0;
    out->poster_path = json::str(d.root(), "posterPath");
    out->backdrop_path = json::str(d.root(), "backdropPath");
    const cJSON *info = json::get(d.root(), "mediaInfo");
    out->media_id = json::integer(info, "id");
    out->media_status = json::integer(info, "status");
    out->media_status4k = json::integer(info, "status4k");
    return !out->title.empty();
}

} // namespace seerr
