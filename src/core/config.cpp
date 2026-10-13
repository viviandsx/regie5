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

namespace {

ServiceConfig parse_one(const cJSON *o, Service svc, int n)
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
                                                           : std::string(service_name(svc)) +
                                                                 (n > 0 ? " " + std::to_string(n + 1) : "");
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

} // namespace

bool parse(const std::string &text, Config *out)
{
    cJSON *root = cJSON_ParseWithLength(text.data(), text.size());
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    *out = Config{};
    for (int i = 0; i < ServiceCount; i++) {
        const Service svc = (Service)i;
        out->services[i].name = service_name(svc);
        const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, service_key(svc));
        if (cJSON_IsObject(o)) {
            out->services[i] = parse_one(o, svc, 0);
        } else if (cJSON_IsArray(o)) {   /* instances */
            int n = 0;
            const cJSON *it;
            cJSON_ArrayForEach(it, o)
            {
                if (!cJSON_IsObject(it))
                    continue;
                if (n == 0)
                    out->services[i] = parse_one(it, svc, 0);
                else
                    out->extra[i].push_back(parse_one(it, svc, n));
                n++;
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
        if (!(a.services[i] == b.services[i]) || a.extra[i] != b.extra[i])
            mask |= 1u << i;
    return mask;
}

Config mock(const std::string &off, bool multi)
{
    Config c;
    const std::string list = "," + off + ",";
    for (int i = 0; i < ServiceCount; i++) {
        c.services[i].name = service_name((Service)i);
        c.services[i].enabled = list.find(std::string(",") + service_key((Service)i) + ",") == std::string::npos;
        c.services[i].url = std::string("http://") + service_key((Service)i) + ".mock";
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
