/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/config.h"

#include "cJSON.h"

#include <cerrno>
#include <cstdio>
#include <tuple>

namespace config {

const char *service_name(Service s)
{
    static const char *const kNames[ServiceCount] = {"Sonarr", "Radarr", "Seerr", "Jellyfin", "Glances"};
    return s >= 0 && s < ServiceCount ? kNames[s] : "";
}

const char *service_key(Service s)
{
    static const char *const kKeys[ServiceCount] = {"sonarr", "radarr", "seerr", "jellyfin", "glances"};
    return s >= 0 && s < ServiceCount ? kKeys[s] : "";
}

const char *media_name(MediaServer m)
{
    return m == MediaServer::Emby ? "Emby" : m == MediaServer::Plex ? "Plex" : "Jellyfin";
}

namespace {

/* label: the default name ("Emby" for an Emby server). plex: a Plex server,
 * whose secret is its "token" and which may go without one. */
ServiceConfig parse_one(const cJSON *o, Service svc, int n, const char *label, bool plex = false)
{
    ServiceConfig s;
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(o, "name");
    const cJSON *en = cJSON_GetObjectItemCaseSensitive(o, "enabled");
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(o, "url");
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(o, "apiKey");
    const cJSON *user = cJSON_GetObjectItemCaseSensitive(o, "username");
    const cJSON *pass = cJSON_GetObjectItemCaseSensitive(o, "password");
    const cJSON *is4k = cJSON_GetObjectItemCaseSensitive(o, "is4k");
    s.name = cJSON_IsString(name) && name->valuestring[0] ? name->valuestring
                                                           : std::string(label) + (n > 0 ? " " + std::to_string(n + 1) : "");
    s.url = cJSON_IsString(url) ? url->valuestring : "";
    while (!s.url.empty() && s.url.back() == '/')
        s.url.pop_back();
    s.api_key = cJSON_IsString(key) ? key->valuestring : "";
    s.username = cJSON_IsString(user) ? user->valuestring : "";
    s.password = cJSON_IsString(pass) ? pass->valuestring : "";
    s.is4k = cJSON_IsTrue(is4k);
    s.insecure = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "insecure"));
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(o, "accessToken");
    const cJSON *user_name = cJSON_GetObjectItemCaseSensitive(o, "userName");
    if (plex) {   /* "token", or an "apiKey" by mistake; who it belongs to, written by the phone's sign-in */
        const cJSON *ptoken = cJSON_GetObjectItemCaseSensitive(o, "token");
        if (cJSON_IsString(ptoken) && ptoken->valuestring[0])
            s.api_key = ptoken->valuestring;
        s.user = cJSON_IsString(user_name) ? user_name->valuestring : "";
        s.admin = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(o, "owner"));
        s.enabled = !cJSON_IsFalse(en) && !s.url.empty();
        return s;
    }
    if (svc == Jellyfin && cJSON_IsString(token)) {
        s.token = token->valuestring;
        s.user = cJSON_IsString(user_name) ? user_name->valuestring : "";
        s.admin = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(o, "isAdmin"));
    }
    /* On unless it says otherwise, as long as it has somewhere to go (and a
     * key, or Jellyfin's session: Glances needs none). */
    s.enabled = !cJSON_IsFalse(en) && !s.url.empty() && (svc == Glances || !s.api_key.empty() || !s.token.empty());
    return s;
}

/* A service's object, or its list of instances, under key. */
void parse_service(const cJSON *root, const char *key, Service svc, const char *label, Config *out, bool plex = false)
{
    out->services[svc] = ServiceConfig{};
    out->services[svc].name = label;
    out->extra[svc].clear();
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsObject(o)) {
        out->services[svc] = parse_one(o, svc, 0, label, plex);
    } else if (cJSON_IsArray(o)) {   /* instances */
        int n = 0;
        const cJSON *it;
        cJSON_ArrayForEach(it, o)
        {
            if (!cJSON_IsObject(it))
                continue;
            if (n == 0)
                out->services[svc] = parse_one(it, svc, 0, label, plex);
            else
                out->extra[svc].push_back(parse_one(it, svc, n, label, plex));
            n++;
        }
    }
}

bool any_on(const Config &c, Service s)
{
    for (int i = 0; i < c.count(s); i++)
        if (c.at(s, i).enabled)
            return true;
    return false;
}

} // namespace

bool parse(const std::string &text, Config *out)
{
    cJSON *root = cJSON_ParseWithLength(text.data(), text.size());
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    *out = Config{};
    for (int i = 0; i < ServiceCount; i++)
        parse_service(root, service_key((Service)i), (Service)i, service_name((Service)i), out);
    /* "emby", then "plex", fill the media server's place when nothing before
     * them is on. */
    for (const auto &[key, label, kind] : {std::tuple<const char *, const char *, MediaServer>{"emby", "Emby", MediaServer::Emby},
                                           {"plex", "Plex", MediaServer::Plex}}) {
        if (!cJSON_GetObjectItemCaseSensitive(root, key))
            continue;
        Config other;
        parse_service(root, key, Jellyfin, label, &other, kind == MediaServer::Plex);
        if (!any_on(other, Jellyfin))
            continue;
        if (any_on(*out, Jellyfin)) {
            out->both_media = true;
        } else {
            out->services[Jellyfin] = other.services[Jellyfin];
            out->extra[Jellyfin] = other.extra[Jellyfin];
            out->media = kind;
        }
    }
    const cJSON *cleanup = cJSON_GetObjectItemCaseSensitive(root, "cleanup");
    const cJSON *ignore = cJSON_GetObjectItemCaseSensitive(cleanup, "ignoreUsers");
    const cJSON *u;
    cJSON_ArrayForEach(u, ignore) if (cJSON_IsString(u)) out->ignore_users.push_back(u->valuestring);
    cJSON_Delete(root);
    return true;
}

LoadError load(const std::string &path, Config *out)
{
    *out = Config{};
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return errno == ENOENT ? LoadError::Missing : LoadError::Unreadable;
    std::string text;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && text.size() < (1u << 20))
        text.append(buf, n);
    std::fclose(f);
    return parse(text, out) ? LoadError::None : LoadError::BadJson;
}

bool with_jellyfin_session(const std::string &text, const std::string &token, const std::string &user, bool admin,
                           std::string *out)
{
    cJSON *root = cJSON_ParseWithLength(text.data(), text.size());
    cJSON *j = root ? cJSON_GetObjectItemCaseSensitive(root, "jellyfin") : nullptr;
    if (cJSON_IsArray(j))
        j = cJSON_GetArrayItem(j, 0);
    if (!cJSON_IsObject(j)) {
        cJSON_Delete(root);
        return false;
    }
    for (const char *k : {"accessToken", "userName", "isAdmin"})
        cJSON_DeleteItemFromObjectCaseSensitive(j, k);
    cJSON_AddStringToObject(j, "accessToken", token.c_str());
    cJSON_AddStringToObject(j, "userName", user.c_str());
    cJSON_AddBoolToObject(j, "isAdmin", admin);
    char *printed = cJSON_Print(root);
    *out = printed ? std::string(printed) + "\n" : std::string();
    cJSON_free(printed);
    cJSON_Delete(root);
    return !out->empty();
}

bool read_file(const std::string &path, std::string *out)
{
    out->clear();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && out->size() < (1u << 20))
        out->append(buf, n);
    std::fclose(f);
    return true;
}

bool write_file(const std::string &path, const std::string &text)
{
    const std::string tmp = path + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !written) {
        std::remove(tmp.c_str());
        return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

unsigned changed(const Config &a, const Config &b)
{
    unsigned mask = 0;
    for (int i = 0; i < ServiceCount; i++)
        if (!(a.services[i] == b.services[i]) || a.extra[i] != b.extra[i] ||
            (i == Jellyfin && a.media != b.media))
            mask |= 1u << i;
    return mask;
}

Config mock(const std::string &off, bool multi, MediaServer media)
{
    Config c;
    c.media = media;
    const bool emby = media == MediaServer::Emby, plex = media == MediaServer::Plex;
    const std::string list = "," + off + ",";
    for (int i = 0; i < ServiceCount; i++) {
        c.services[i].name = i == Jellyfin ? media_name(c.media) : service_name((Service)i);
        c.services[i].enabled = list.find(std::string(",") + service_key((Service)i) + ",") == std::string::npos;
        c.services[i].url = std::string("http://") +
                            (i == Jellyfin && emby ? "emby" : i == Jellyfin && plex ? "plex" : service_key((Service)i)) + ".mock";
        c.services[i].api_key = "mock";
    }
    if (multi) {
        ServiceConfig r4k = c.services[Radarr];
        r4k.name = "Radarr 4K";
        r4k.url = "http://radarr.mock/radarr4k";
        r4k.is4k = true;
        c.extra[Radarr].push_back(r4k);
    }
    c.ignore_users = {"Hugo"};   /* a test account, to show the setting */
    return c;
}

} // namespace config
