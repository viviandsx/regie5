/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/mock_transport.h"

#include "api/json.h"
#include "cJSON.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>
#include <vector>

namespace api {

namespace {

/* text split at sep (no iostreams: the PS5 runtime has no locales for them). */
std::vector<std::string> split_at(const std::string &text, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find(sep, start);
        out.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return out;
}

/* Runs on worker threads, whose stacks may be small (the PS5's): the file
 * goes straight into the string, no buffer on the stack. */
bool read_file(const std::string &path, std::string *out)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        out->resize((size_t)size);
        out->resize(std::fread(&(*out)[0], 1, (size_t)size, f));
    }
    std::fclose(f);
    return true;
}

/* "http://sonarr.mock/api/v3/queue?page=1" -> service "sonarr", path "/api/v3/queue", query "page=1". */
bool split(const std::string &url, std::string *service, std::string *path, std::string *query)
{
    const size_t s = url.find("://");
    if (s == std::string::npos)
        return false;
    const size_t host_end = url.find('/', s + 3);
    const std::string host = url.substr(s + 3, host_end == std::string::npos ? std::string::npos : host_end - s - 3);
    const size_t dot = host.find(".mock");
    if (dot == std::string::npos)
        return false;
    *service = host.substr(0, dot);
    const std::string rest = host_end == std::string::npos ? "/" : url.substr(host_end);
    const size_t q = rest.find('?');
    *path = rest.substr(0, q);
    *query = q == std::string::npos ? "" : rest.substr(q + 1);
    return true;
}

std::string query_value(const std::string &query, const std::string &key)
{
    for (const std::string &pair : split_at(query, '&')) {
        const size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key)
            return pair.substr(eq + 1);
    }
    return {};
}

/* Arrays emptied, counts set to 0, nested objects (Seerr's pageInfo) alike. */
void empty_lists(cJSON *o)
{
    static const char *const kCounts[] = {"totalRecords", "TotalRecordCount", "results", "pages"};
    cJSON *child = o->child;
    while (child) {
        cJSON *next = child->next;
        if (cJSON_IsArray(child)) {
            cJSON_ReplaceItemInObjectCaseSensitive(o, child->string, cJSON_CreateArray());
        } else if (cJSON_IsObject(child)) {
            empty_lists(child);
        } else if (cJSON_IsNumber(child) && child->string) {
            for (const char *k : kCounts)
                if (std::string(child->string) == k)
                    cJSON_SetNumberValue(child, 0);
        }
        child = next;
    }
}

/* "%C3%A9t%C3%A9" -> "été" (a query as the URL carries it). */
std::string unescape(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2])) {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else {
            out += s[i] == '+' ? ' ' : s[i];
        }
    }
    return out;
}

std::string lowered(std::string s)
{
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

/* Seerr's search: only the titles whose name holds the query (any case). */
std::string search_matches(const std::string &body, const std::string &query)
{
    cJSON *root = cJSON_ParseWithLength(body.data(), body.size());
    cJSON *results = root ? cJSON_GetObjectItemCaseSensitive(root, "results") : nullptr;
    if (!cJSON_IsArray(results)) {
        cJSON_Delete(root);
        return body;
    }
    const std::string q = lowered(query);
    cJSON *kept = cJSON_CreateArray();
    for (cJSON *it = results->child; it; it = it->next) {
        std::string name;
        for (const char *k : {"title", "name"})
            if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(it, k); cJSON_IsString(v))
                name = v->valuestring;
        if (lowered(name).find(q) != std::string::npos)
            cJSON_AddItemToArray(kept, cJSON_Duplicate(it, true));
    }
    const int n = cJSON_GetArraySize(kept);
    cJSON_ReplaceItemInObjectCaseSensitive(root, "results", kept);
    if (cJSON *total = cJSON_GetObjectItemCaseSensitive(root, "totalResults"))
        cJSON_SetNumberValue(total, n);
    char *text = cJSON_PrintUnformatted(root);
    std::string out = text ? text : body;
    cJSON_free(text);
    cJSON_Delete(root);
    return out;
}

/* Howard Hinnant's civil_from_days (public domain): a day number back to a date. */
void civil_from_days(int64_t z, int *y, int *m, int *d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* "/" -> root, "/status/sessions" -> status_sessions, "/:/prefs" -> prefs;
 * a library's list by what is asked: its latest (sorted by date), a count
 * (a page of 0), or its titles with their ids; several items at once ->
 * library_metadata_ids. */
std::string plex_file_for(const std::string &path, const std::string &query)
{
    if (path.rfind("/photo/", 0) == 0)
        return {};   /* pictures: none in fixtures/ */
    std::string p = path == "/" ? "root" : path == "/:/prefs" ? "prefs" : path;
    while (!p.empty() && p[0] == '/')
        p.erase(0, 1);
    std::string name;
    for (char c : p)
        name += c == '/' ? '_' : (char)std::tolower((unsigned char)c);
    if (name.rfind("library_metadata_", 0) == 0 && name.find(',') != std::string::npos)
        name = "library_metadata_ids";
    if (name.rfind("library_sections_", 0) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, "_all") == 0 &&
        name != "library_sections_all") {
        const bool episodes = query_value(query, "type") == "4";
        if (query_value(query, "X-Plex-Container-Size") == "0")
            name += episodes ? "_count_episode" : "_count";
        else if (!query_value(query, "sort").empty())
            name += "_recent";
    }
    return "plex/" + name + ".json";
}

} // namespace

std::string MockTransport::dated(const std::string &body, int64_t shift_days)
{
    if (shift_days == 0)
        return body;
    /* Every "yyyy-mm-dd" in the text, keys included. */
    std::string out = body;
    auto digits = [&](size_t at, size_t n) {
        for (size_t i = 0; i < n; i++)
            if (!std::isdigit((unsigned char)out[at + i]))
                return false;
        return true;
    };
    for (size_t i = 0; i + 10 <= out.size(); i++) {
        if (!(digits(i, 4) && out[i + 4] == '-' && digits(i + 5, 2) && out[i + 7] == '-' && digits(i + 8, 2)) ||
            (i > 0 && std::isdigit((unsigned char)out[i - 1])))
            continue;
        const int y = std::stoi(out.substr(i, 4)), m = std::stoi(out.substr(i + 5, 2)), d = std::stoi(out.substr(i + 8, 2));
        if (m < 1 || m > 12 || d < 1 || d > 31)
            continue;
        int ny, nm, nd;
        civil_from_days(json::days_from_civil(y, m, d) + shift_days, &ny, &nm, &nd);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", ny, nm, nd);
        out.replace(i, 10, buf);
        i += 9;
    }
    return out;
}

MockTransport::MockTransport(std::string root, int delay_ms, const std::string &fail, const std::string &empty,
                             const std::string &old)
    : m_root(std::move(root)), m_delay_ms(delay_ms)
{
    for (const std::string &name : split_at(old, ','))
        if (!name.empty())
            m_old[name] = true;
    std::string index;
    if (read_file(m_root + "/images.json", &index)) {
        if (cJSON *o = cJSON_ParseWithLength(index.data(), index.size())) {
            for (cJSON *it = o->child; it; it = it->next)
                if (it->string && cJSON_IsString(it))
                    m_pictures[it->string] = it->valuestring;
            cJSON_Delete(o);
        }
    }
    for (const std::string &name : split_at(empty, ','))
        if (!name.empty())
            m_empty[name] = true;
    for (const std::string &item : split_at(fail, ',')) {
        const size_t c = item.find(':');
        if (c != std::string::npos)
            m_fail[item.substr(0, c)] = item.substr(c + 1);
    }
}

std::string MockTransport::file_for(const std::string &url)
{
    std::string service, path, query;
    if (!split(url, &service, &path, &query))
        return {};
    if (service == "plex")   /* Plex's paths: fixtures/plex/ (scripts/make-plex-fixtures.py) */
        return plex_file_for(path, query);
    std::string p = path;
    /* An *arr behind a path of its own (urlBase, a reverse proxy): what
     * precedes /api/ is not part of the file's name. */
    const size_t api = p.find("/api/");
    if (api != std::string::npos && api > 0)
        p = p.substr(api);
    for (const char *prefix : {"/api/v3/", "/api/v1/", "/api/4/"})
        if (p.rfind(prefix, 0) == 0)
            p = p.substr(std::string(prefix).size());
    if (service == "scrutiny" && p.rfind("/api/", 0) == 0)   /* Scrutiny's API has no version in its path */
        p = p.substr(5);
    while (!p.empty() && p[0] == '/')
        p.erase(0, 1);
    std::string name;
    for (char c : p)
        name += c == '/' ? '_' : (char)std::tolower((unsigned char)c);
    if (service == "sonarr" && name == "episodefile") {   /* one file per series */
        const std::string id = query_value(query, "seriesId");
        if (!id.empty())
            name += "_" + id;
    }
    const bool media = service == "jellyfin" || service == "emby";   /* fixtures/emby/: Emby's own answers */
    if (media && name == "items" && !query_value(query, "ids").empty())
        name = "items_ids";   /* names for the journal (the recent row has items.json) */
    else if (media && name == "items" && !query_value(query, "userId").empty() &&
             query_value(query, "IsPlayed") == "true")
        name = "items_user_" + query_value(query, "userId") + "_played";   /* Emby: watched episodes, dated */
    else if (media && name == "items" && !query_value(query, "userId").empty())
        name = "items_user_" + query_value(query, "userId");   /* one user's, for the cleanup */
    else if (media && name == "items" && !query_value(query, "ParentId").empty()) {
        /* a library's titles, or a count (Limit=0) of its episodes or files */
        const std::string types = query_value(query, "IncludeItemTypes");
        name = "items_parent_" + query_value(query, "ParentId") +
               (types == "Episode"  ? "_episode"
                : types == "BoxSet" ? "_boxset"
                : !query_value(query, "IsFolder").empty() ? "_files"
                                                          : "");
    } else if (media && name == "items" && query_value(query, "IncludeItemTypes") == "Movie,Series")
        name = "items_index";   /* every film and series (Jelly5's links) */
    if (media && name.rfind("user_usage_stats_", 0) == 0 && !query_value(query, "days").empty())
        name += "_" + query_value(query, "days");   /* Playback Reporting: one answer per period */
    if (service == "seerr" && name == "issue") {   /* Seerr's default filter is "open" */
        const std::string f = query_value(query, "filter");
        name += "_" + (f.empty() ? std::string("open") : f);
    }
    if (service == "seerr" && name == "request") {
        const std::string f = query_value(query, "filter");
        name += "_" + (f.empty() ? std::string("all") : f);
    }
    if (name.empty())
        return {};
    return service + "/" + name + ".json";
}

std::string MockTransport::picture_for(const std::string &url)
{
    std::string service, path, query;
    if (!split(url, &service, &path, &query))
        return {};
    auto lower = [](std::string s) {
        for (char &c : s)
            c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::vector<std::string> parts = split_at(path, '/');
    for (size_t i = 0; i + 2 < parts.size(); i++) {
        if (parts[i] == "mediacover") {   /* .../mediacover/12/poster-500.jpg */
            const std::string &file = parts[i + 2];
            return service + "/mediacover/" + parts[i + 1] + "/" + file.substr(0, file.find_first_of("-."));
        }
        if (lower(parts[i]) == "items" && parts.size() > i + 3 && lower(parts[i + 2]) == "images")
            return service + "/items/" + parts[i + 1] + "/" + lower(parts[i + 3]);
    }
    if (path.find("/imageproxy/") != std::string::npos && !parts.empty())
        return service + "/" + parts.back();
    return {};
}

std::string MockTransport::emptied(const std::string &rel, const std::string &body)
{
    for (const char *kept : {"system_status", "settings_about", "system_info", "/movie_", "/tv_", "plex/root", "plex/identity",
                             "/plugins", "scrutiny/settings"})
        if (rel.find(kept) != std::string::npos)
            return body;
    cJSON *root = cJSON_ParseWithLength(body.data(), body.size());
    if (!root)
        return body;
    std::string out = "[]";
    if (rel == "scrutiny/summary.json") {   /* its disks are an object's members, not a list */
        if (cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data"))
            cJSON_ReplaceItemInObjectCaseSensitive(data, "summary", cJSON_CreateObject());
        char *text = cJSON_PrintUnformatted(root);
        out = text ? text : body;
        cJSON_free(text);
    } else if (cJSON_IsObject(root)) {
        empty_lists(root);
        char *text = cJSON_PrintUnformatted(root);
        out = text ? text : body;
        cJSON_free(text);
    }
    cJSON_Delete(root);
    return out;
}

http::Response MockTransport::perform(const http::Request &req, const std::atomic<bool> *cancel)
{
    http::Response r;
    std::string service, path, query;
    if (!split(req.url, &service, &path, &query)) {
        r.fail = http::Response::Connect;
        r.error = "mock: not a .mock address";
        return r;
    }
    for (int waited = 0; waited < m_delay_ms && !(cancel && *cancel); waited += 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto f = m_fail.find(service);
    if (f != m_fail.end()) {
        const std::string &kind = f->second;
        if (kind == "unreachable")
            r.fail = http::Response::Connect;
        else if (kind == "timeout")
            r.fail = http::Response::Timeout;
        else if (kind == "unauthorized")
            r.status = 401;
        else if (kind == "forbidden")
            r.status = 403;
        else if (kind == "notfound")
            r.status = 404;
        else if (kind == "servererror")
            r.status = 500;
        else if (kind == "badjson")
            r.status = 200, r.body = "<html>not json</html>", r.content_type = "text/html";
        else if (kind == "toolarge" && req.max_body)   /* a library past the limit (other calls answer) */
            r.fail = http::Response::TooLarge, r.error = "answer too large";
        if (kind != "toolarge" || req.max_body)
            return r;
    }

    /* Quick Connect's two POSTs only sign in: answered from the fixtures. */
    const bool sign_in = req.method == "POST" && (service == "jellyfin") &&
                         (path == "/QuickConnect/Initiate" || path == "/Users/AuthenticateWithQuickConnect");
    if (req.method != "GET" && !sign_in) {   /* an action: logged, never carried out */
        std::fprintf(stderr, "mock: %s %s (nothing done)\n", req.method.c_str(), path.c_str());
        r.status = 204;
        return r;
    }
    if (!m_pictures.empty()) {
        const std::string key = picture_for(req.url);
        if (!key.empty()) {
            auto p = m_pictures.find(key);
            if (p == m_pictures.end() || !read_file(m_root + "/images/" + p->second, &r.body)) {
                r.status = 404;
                return r;
            }
            r.status = 200;
            r.content_type = "image/jpeg";
            return r;
        }
    }
    const std::string rel = file_for(req.url);
    std::string body;
    if (rel.empty() || !read_file(m_root + "/" + rel, &body)) {
        r.status = 404;
        return r;
    }
    r.status = 200;
    if (rel == "seerr/search.json")
        body = search_matches(body, unescape(query_value(query, "query")));
    if (rel.rfind("scrutiny/", 0) == 0 || rel.find("/user_usage_stats_") != std::string::npos) {
        const time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        body = dated(body, json::days_from_civil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) -
                               json::days_from_civil(kAnchorY, kAnchorM, kAnchorD));
    }
    r.body = m_empty.count(service) ? emptied(rel, body) : body;
    if (m_old.count(service)) {   /* the connection test's answer: an old version */
        for (const char *key : {"version", "Version"}) {
            const std::string k = std::string("\"") + key + "\": \"";
            const size_t at = r.body.find(k);
            if (at != std::string::npos && (rel.find("system_status") != std::string::npos ||
                                            rel.find("settings_about") != std::string::npos ||
                                            rel.find("system_info") != std::string::npos ||
                                            rel == "plex/root.json"))
                r.body.replace(at + k.size(), r.body.find('"', at + k.size()) - at - k.size(), "1.0.0");
        }
    }
    r.content_type = "application/json";
    if (req.max_body && r.body.size() > req.max_body) {   /* as a transport gives up */
        r = http::Response{};
        r.fail = http::Response::TooLarge;
        r.error = "answer too large";
    }
    return r;
}

} // namespace api
