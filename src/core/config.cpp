/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/config.h"

#include "cJSON.h"

#include <cerrno>
#include <cstdio>

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

const char *media_name(MediaServer m) { return m == MediaServer::Emby ? "Emby" : "Jellyfin"; }

namespace {

/* label: the default name ("Emby" for an Emby server). */
ServiceConfig parse_one(const cJSON *o, Service svc, int n, const char *label)
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
    /* On unless it says otherwise, as long as it has somewhere to go (and a
     * key: Glances needs none). */
    s.enabled = !cJSON_IsFalse(en) && !s.url.empty() && (svc == Glances || !s.api_key.empty());
    return s;
}

/* A service's object, or its list of instances, under key. */
void parse_service(const cJSON *root, const char *key, Service svc, const char *label, Config *out)
{
    out->services[svc] = ServiceConfig{};
    out->services[svc].name = label;
    out->extra[svc].clear();
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsObject(o)) {
        out->services[svc] = parse_one(o, svc, 0, label);
    } else if (cJSON_IsArray(o)) {   /* instances */
        int n = 0;
        const cJSON *it;
        cJSON_ArrayForEach(it, o)
        {
            if (!cJSON_IsObject(it))
                continue;
            if (n == 0)
                out->services[svc] = parse_one(it, svc, 0, label);
            else
                out->extra[svc].push_back(parse_one(it, svc, n, label));
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
    /* "emby" fills the media server's place when Jellyfin is not on. */
    if (cJSON_GetObjectItemCaseSensitive(root, "emby")) {
        Config emby;
        parse_service(root, "emby", Jellyfin, "Emby", &emby);
        if (any_on(emby, Jellyfin)) {
            if (any_on(*out, Jellyfin)) {
                out->both_media = true;
            } else {
                out->services[Jellyfin] = emby.services[Jellyfin];
                out->extra[Jellyfin] = emby.extra[Jellyfin];
                out->media = MediaServer::Emby;
            }
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

unsigned changed(const Config &a, const Config &b)
{
    unsigned mask = 0;
    for (int i = 0; i < ServiceCount; i++)
        if (!(a.services[i] == b.services[i]) || a.extra[i] != b.extra[i] ||
            (i == Jellyfin && a.media != b.media))
            mask |= 1u << i;
    return mask;
}

Config mock(const std::string &off, bool multi, bool emby)
{
    Config c;
    if (emby)
        c.media = MediaServer::Emby;
    const std::string list = "," + off + ",";
    for (int i = 0; i < ServiceCount; i++) {
        c.services[i].name = i == Jellyfin ? media_name(c.media) : service_name((Service)i);
        c.services[i].enabled = list.find(std::string(",") + service_key((Service)i) + ",") == std::string::npos;
        c.services[i].url = std::string("http://") + (i == Jellyfin && emby ? "emby" : service_key((Service)i)) + ".mock";
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
