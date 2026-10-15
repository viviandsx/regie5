/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/seerr.h"

#include "api/json.h"

#include <cctype>
#include <algorithm>
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

std::string issues_path(IssueFilter f, int take, int skip)
{
    const char *filter = f == IssueFilter::Open ? "open" : f == IssueFilter::Resolved ? "resolved" : "all";
    return "/api/v1/issue?take=" + std::to_string(take) + "&skip=" + std::to_string(skip) + "&sort=modified&filter=" +
           filter;
}

std::string issue_path(int issue_id) { return "/api/v1/issue/" + std::to_string(issue_id); }
std::string issue_comment_path(int issue_id) { return issue_path(issue_id) + "/comment"; }
std::string issue_resolve_path(int issue_id) { return issue_path(issue_id) + "/resolved"; }

std::string comment_body(const std::string &message)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "message", message.c_str());
    char *text = cJSON_PrintUnformatted(o);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(o);
    return out;
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

namespace {

/* A user as the app shows them: the display name, else the Jellyfin or local
 * name. Nothing else of the user is read (no e-mail address). */
std::string user_name(const cJSON *u)
{
    std::string n = json::str(u, "displayName");
    if (n.empty())
        n = json::str(u, "jellyfinUsername", json::str(u, "username"));
    return n;
}

void read_issue(const cJSON *it, model::Issue *out)
{
    model::Issue &i = *out;
    i = {};
    i.id = json::integer(it, "id");
    const int type = json::integer(it, "issueType", 4);
    i.type = type >= 1 && type <= 4 ? (model::Issue::Type)type : model::Issue::Other;
    i.resolved = json::integer(it, "status", 1) == 2;
    i.season = json::integer(it, "problemSeason");
    i.episode = json::integer(it, "problemEpisode");
    i.created = json::parse_time(json::str(it, "createdAt"));
    i.updated = json::parse_time(json::str(it, "updatedAt"));
    i.reported_by = user_name(json::get(it, "createdBy"));
    i.modified_by = user_name(json::get(it, "modifiedBy"));
    const cJSON *media = json::get(it, "media");
    i.tv = json::str(media, "mediaType") == "tv";
    i.tmdb_id = json::integer(media, "tmdbId");
    i.jellyfin_id = json::str(media, "jellyfinMediaId");
    const cJSON *c;
    cJSON_ArrayForEach(c, json::get(it, "comments"))
    {
        model::IssueComment ic;
        ic.author = user_name(json::get(c, "user"));
        ic.message = json::str(c, "message");
        ic.created = json::parse_time(json::str(c, "createdAt"));
        i.comments.push_back(ic);
    }
    /* Seerr does not promise their order: the oldest (the report) first. */
    std::stable_sort(i.comments.begin(), i.comments.end(),
                     [](const model::IssueComment &a, const model::IssueComment &b) { return a.created < b.created; });
}

} // namespace

bool parse_issues(const std::string &body, model::IssuePage *out)
{
    json::Doc d(body);
    const cJSON *results = json::get(d.root(), "results");
    if (!cJSON_IsArray(results))
        return false;
    *out = {};
    out->total = json::integer(json::get(d.root(), "pageInfo"), "results");
    const cJSON *it;
    cJSON_ArrayForEach(it, results)
    {
        model::Issue i;
        read_issue(it, &i);
        out->issues.push_back(std::move(i));
    }
    return true;
}

bool parse_issue(const std::string &body, model::Issue *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()) || !json::get(d.root(), "id"))
        return false;
    read_issue(d.root(), out);
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

namespace seerr {

namespace {

/* Every byte but the unreserved ones as %XX: Seerr hands the query to TMDB as
 * it comes, and refuses a '+' for a space. */
std::string escape(const std::string &s)
{
    static const char *const kHex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else
            out += '%', out += kHex[c >> 4], out += kHex[c & 15];
    }
    return out;
}

int year_of(const std::string &date) { return date.size() >= 4 ? std::atoi(date.substr(0, 4).c_str()) : 0; }

/* The fields a result and a page share (films and series name them apart). */
void read_title(const cJSON *o, bool tv, model::SeerrTitle *t)
{
    t->tmdb_id = json::integer(o, "id");
    t->tv = tv;
    t->title = json::str(o, tv ? "name" : "title");
    t->overview = json::str(o, "overview");
    t->poster_path = json::str(o, "posterPath");
    t->backdrop_path = json::str(o, "backdropPath");
    t->year = year_of(json::str(o, tv ? "firstAirDate" : "releaseDate"));
    const cJSON *media = json::get(o, "mediaInfo");
    if (cJSON_IsObject(media)) {
        t->status = json::integer(media, "status", model::SeerrUnknown);
        t->status4k = json::integer(media, "status4k", model::SeerrUnknown);
        t->jellyfin_id = json::str(media, "jellyfinMediaId");
    }
}

void read_server(const cJSON *o, model::SeerrServer *s)
{
    s->id = json::integer(o, "id");
    s->name = json::str(o, "name");
    s->is4k = json::boolean(o, "is4k");
    s->is_default = json::boolean(o, "isDefault");
    s->profile_id = json::integer(o, "activeProfileId");
    s->folder = json::str(o, "activeDirectory");
}

} // namespace

std::string discover_path(Shelf s, const std::string &lang, int page)
{
    const char *path = s == Shelf::PopularMovies    ? "/api/v1/discover/movies"
                       : s == Shelf::PopularTv      ? "/api/v1/discover/tv"
                       : s == Shelf::UpcomingMovies ? "/api/v1/discover/movies/upcoming"
                       : s == Shelf::UpcomingTv     ? "/api/v1/discover/tv/upcoming"
                                                    : "/api/v1/discover/trending";
    return std::string(path) + "?page=" + std::to_string(page) + "&language=" + escape(lang);
}

std::string search_path(const std::string &query, const std::string &lang, int page)
{
    return "/api/v1/search?query=" + escape(query) + "&page=" + std::to_string(page) + "&language=" + escape(lang);
}

std::string public_settings_path() { return "/api/v1/settings/public"; }
std::string servers_path(bool tv) { return tv ? "/api/v1/service/sonarr" : "/api/v1/service/radarr"; }
std::string server_path(bool tv, int server_id) { return servers_path(tv) + "/" + std::to_string(server_id); }
std::string create_request_path() { return "/api/v1/request"; }

std::string request_body(const NewRequest &r)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "mediaType", r.tv ? "tv" : "movie");
    cJSON_AddNumberToObject(o, "mediaId", r.tmdb_id);
    if (r.tv) {
        if (r.tvdb_id > 0)
            cJSON_AddNumberToObject(o, "tvdbId", r.tvdb_id);
        cJSON *seasons = cJSON_AddArrayToObject(o, "seasons");
        for (int n : r.seasons)
            cJSON_AddItemToArray(seasons, cJSON_CreateNumber(n));
    }
    cJSON_AddBoolToObject(o, "is4k", r.is4k);
    if (r.server_id >= 0)
        cJSON_AddNumberToObject(o, "serverId", r.server_id);
    if (r.profile_id >= 0)
        cJSON_AddNumberToObject(o, "profileId", r.profile_id);
    if (!r.root_folder.empty())
        cJSON_AddStringToObject(o, "rootFolder", r.root_folder.c_str());
    char *text = cJSON_PrintUnformatted(o);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(o);
    return out;
}

bool parse_titles(const std::string &body, std::vector<model::SeerrTitle> *out)
{
    json::Doc d(body);
    const cJSON *results = json::get(d.root(), "results");
    if (!cJSON_IsArray(results))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, results)
    {
        const std::string type = json::str(it, "mediaType");
        if (type != "movie" && type != "tv")
            continue;
        model::SeerrTitle t;
        read_title(it, type == "tv", &t);
        if (t.tmdb_id > 0 && !t.title.empty())
            out->push_back(std::move(t));
    }
    return true;
}

bool parse_title_detail(const std::string &body, bool tv, model::SeerrDetail *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *out = {};
    read_title(d.root(), tv, &out->title);
    const cJSON *v;
    cJSON_ArrayForEach(v, json::get(d.root(), "genres")) out->genres.push_back(json::str(v, "name"));
    if (tv) {
        const cJSON *rt = cJSON_GetArrayItem(json::get(d.root(), "episodeRunTime"), 0);
        out->runtime = cJSON_IsNumber(rt) ? rt->valueint : 0;
        out->tvdb_id = json::integer(json::get(d.root(), "externalIds"), "tvdbId");
        /* Where each season stands: on the media server, or in a request still
         * pending or approved; the 4K side apart. */
        const cJSON *media = json::get(d.root(), "mediaInfo");
        std::vector<model::SeerrSeason> known;
        const cJSON *s;
        cJSON_ArrayForEach(s, json::get(media, "seasons"))
        {
            model::SeerrSeason k;
            k.number = json::integer(s, "seasonNumber");
            k.status = json::integer(s, "status", model::SeerrUnknown);
            k.status4k = json::integer(s, "status4k", model::SeerrUnknown);
            known.push_back(k);
        }
        std::vector<std::pair<int, bool>> asked;   /* season, 4K */
        const cJSON *rq;
        cJSON_ArrayForEach(rq, json::get(media, "requests"))
        {
            const int st = json::integer(rq, "status");
            if (st != 1 && st != 2)
                continue;
            cJSON_ArrayForEach(s, json::get(rq, "seasons"))
                asked.push_back({json::integer(s, "seasonNumber"), json::boolean(rq, "is4k")});
        }
        cJSON_ArrayForEach(s, json::get(d.root(), "seasons"))
        {
            model::SeerrSeason season;
            season.number = json::integer(s, "seasonNumber");
            season.name = json::str(s, "name");
            season.episodes = json::integer(s, "episodeCount");
            for (const auto &k : known)
                if (k.number == season.number)
                    season.status = k.status, season.status4k = k.status4k;
            for (const auto &a : asked)
                if (a.first == season.number)
                    (a.second ? season.requested4k : season.requested) = true;
            out->seasons.push_back(season);
        }
    } else {
        out->runtime = json::integer(d.root(), "runtime");
    }
    return out->title.tmdb_id > 0 && !out->title.title.empty();
}

bool parse_public(const std::string &body, model::SeerrPublic *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    out->partial_requests = json::boolean(d.root(), "partialRequestsEnabled", true);
    out->special_episodes = json::boolean(d.root(), "enableSpecialEpisodes", false);
    return true;
}

bool parse_servers(const std::string &body, std::vector<model::SeerrServer> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::SeerrServer s;
        read_server(it, &s);
        out->push_back(s);
    }
    return true;
}

bool parse_server(const std::string &body, model::SeerrServer *out)
{
    json::Doc d(body);
    const cJSON *server = json::get(d.root(), "server");
    if (!cJSON_IsObject(server))
        return false;
    *out = {};
    read_server(server, out);
    const cJSON *v;
    cJSON_ArrayForEach(v, json::get(d.root(), "profiles"))
        out->profiles.push_back({json::integer(v, "id"), json::str(v, "name")});
    cJSON_ArrayForEach(v, json::get(d.root(), "rootFolders")) out->folders.push_back(json::str(v, "path"));
    return true;
}

bool parse_created(const std::string &body, model::SeerrCreated *out)
{
    *out = {};
    if (body.find_first_not_of(" \r\n\t") == std::string::npos)
        return true;
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    out->id = json::integer(d.root(), "id");
    out->status = json::integer(d.root(), "status");
    return true;
}

} // namespace seerr
