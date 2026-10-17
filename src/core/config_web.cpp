/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/config_web.h"

#include "core/config.h"

#include "cJSON.h"

#include <cctype>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <memory>

namespace config_web {

namespace {

struct JsonDeleter {
    void operator()(cJSON *j) const { cJSON_Delete(j); }
};
using Json = std::unique_ptr<cJSON, JsonDeleter>;

std::string print(const cJSON *j, bool pretty)
{
    char *s = pretty ? cJSON_Print(j) : cJSON_PrintUnformatted(j);
    std::string out = s ? s : "";
    cJSON_free(s);
    return out;
}

const char *str(const cJSON *o, const char *name)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    return cJSON_IsString(v) ? v->valuestring : "";
}

/* A service's instances in the file, as a list whatever its form. */
std::vector<const cJSON *> instances(const cJSON *root, const char *key)
{
    std::vector<const cJSON *> out;
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsObject(o)) {
        out.push_back(o);
    } else if (cJSON_IsArray(o)) {
        const cJSON *it;
        cJSON_ArrayForEach(it, o) if (cJSON_IsObject(it)) out.push_back(it);
    }
    return out;
}

bool is_glances(int s) { return s == config::Glances; }

/* The keys of config.json the page shows, in its order: "emby" and "plex"
 * stand in the media server's place when "jellyfin" is not on
 * (config::parse). */
const struct {
    const char *key;
    config::Service service;
} kKeys[] = {{"sonarr", config::Sonarr}, {"radarr", config::Radarr},     {"seerr", config::Seerr},
             {"jellyfin", config::Jellyfin}, {"emby", config::Jellyfin}, {"plex", config::Jellyfin},
             {"glances", config::Glances}};

bool is_plex(const char *key) { return std::string(key) == "plex"; }

/* The one secret of an instance: Plex calls its own a token. */
const char *secret_name(const char *key) { return is_plex(key) ? "token" : "apiKey"; }

} // namespace

std::string public_view(const std::string &config_text)
{
    Json root(cJSON_ParseWithLength(config_text.data(), config_text.size()));
    Json view(cJSON_CreateObject());
    cJSON *services = cJSON_AddObjectToObject(view.get(), "services");
    for (const auto &k : kKeys) {
        const char *key = k.key;
        const int s = k.service;
        cJSON *list = cJSON_AddArrayToObject(services, key);
        if (!root || !cJSON_IsObject(root.get()))
            continue;
        for (const cJSON *in : instances(root.get(), key)) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "name", str(in, "name"));
            cJSON_AddStringToObject(o, "url", str(in, "url"));
            cJSON_AddBoolToObject(o, "enabled", !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(in, "enabled")));
            cJSON_AddBoolToObject(o, "insecure", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(in, "insecure")));
            if (is_glances(s)) {
                cJSON_AddStringToObject(o, "username", str(in, "username"));
                cJSON_AddBoolToObject(o, "hasPassword", *str(in, "password") != '\0');
            } else {
                cJSON_AddBoolToObject(o, "hasKey", *str(in, secret_name(key)) != '\0' ||
                                                       (is_plex(key) && *str(in, "apiKey") != '\0'));
                if (is_plex(key)) {   /* who the token belongs to: shown, never the token */
                    cJSON_AddStringToObject(o, "userName", str(in, "userName"));
                    cJSON_AddBoolToObject(o, "owner", !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(in, "owner")));
                }
                if (s == config::Sonarr || s == config::Radarr)
                    cJSON_AddBoolToObject(o, "is4k", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(in, "is4k")));
            }
            cJSON_AddItemToArray(list, o);
        }
    }
    return print(view.get(), false);
}

bool apply(const std::string &current_text, const std::string &posted, std::string *out, std::string *error)
{
    Json cur(cJSON_ParseWithLength(current_text.data(), current_text.size()));
    if (!cur || !cJSON_IsObject(cur.get()))
        cur.reset(cJSON_CreateObject());   /* no file yet, or not JSON: started afresh */
    Json in(cJSON_ParseWithLength(posted.data(), posted.size()));
    const cJSON *services = in ? cJSON_GetObjectItemCaseSensitive(in.get(), "services") : nullptr;
    if (!cJSON_IsObject(services)) {
        *error = "not a configuration";
        return false;
    }
    for (const auto &k : kKeys) {
        const char *key = k.key;
        const int s = k.service;
        const cJSON *list = cJSON_GetObjectItemCaseSensitive(services, key);
        if (!list)
            continue;   /* not sent: left as it is */
        if (!cJSON_IsArray(list)) {
            *error = std::string(key) + ": not a list";
            return false;
        }
        const std::vector<const cJSON *> before = instances(cur.get(), key);
        Json made(cJSON_CreateArray());
        const cJSON *it;
        cJSON_ArrayForEach(it, list)
        {
            if (!cJSON_IsObject(it)) {
                *error = std::string(key) + ": not an instance";
                return false;
            }
            const std::string url = str(it, "url");
            if (url.empty())
                continue;   /* an empty row: dropped */
            if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
                *error = std::string(key) + ": the address must start with http:// or https://";
                return false;
            }
            const cJSON *from_j = cJSON_GetObjectItemCaseSensitive(it, "from");
            const int from = cJSON_IsNumber(from_j) ? (int)from_j->valuedouble : -1;
            const cJSON *old = from >= 0 && from < (int)before.size() ? before[(size_t)from] : nullptr;
            /* The old instance as the base: fields the page does not know stay. */
            cJSON *o = old ? cJSON_Duplicate(old, true) : cJSON_CreateObject();
            auto set_str = [o](const char *name, const std::string &v) {
                cJSON_DeleteItemFromObjectCaseSensitive(o, name);
                cJSON_AddStringToObject(o, name, v.c_str());
            };
            auto set_bool = [o](const char *name, bool v) {
                cJSON_DeleteItemFromObjectCaseSensitive(o, name);
                cJSON_AddBoolToObject(o, name, v);
            };
            const std::string name = str(it, "name");
            if (!name.empty())
                set_str("name", name);
            else
                cJSON_DeleteItemFromObjectCaseSensitive(o, "name");
            set_bool("enabled", !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(it, "enabled")));
            if (old && str(old, "url") != url)   /* another server: the old one's session means nothing there */
                for (const char *k : {"accessToken", "userName", "isAdmin"})
                    cJSON_DeleteItemFromObjectCaseSensitive(o, k);
            if (is_plex(key) && old && str(old, "url") != url)   /* nor its Plex token */
                for (const char *k : {"token", "apiKey", "owner"})
                    cJSON_DeleteItemFromObjectCaseSensitive(o, k);
            set_str("url", url);
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(it, "insecure")))
                set_bool("insecure", true);
            else
                cJSON_DeleteItemFromObjectCaseSensitive(o, "insecure");   /* off: the default, not written */
            if (is_glances(s)) {
                set_str("username", str(it, "username"));
                const std::string pass = str(it, "password");
                if (!pass.empty() || !old)
                    set_str("password", pass);
                if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(it, "clearPassword")))
                    set_str("password", "");
            } else if (is_plex(key)) {   /* a token pasted (or none: Plex's "allowed without auth") */
                const std::string k = str(it, "apiKey");
                if (!k.empty()) {
                    set_str("token", k);
                    cJSON_DeleteItemFromObjectCaseSensitive(o, "apiKey");
                    for (const char *w : {"userName", "owner"})   /* whose it is, unknown for a pasted one */
                        cJSON_DeleteItemFromObjectCaseSensitive(o, w);
                }
            } else {
                const std::string k = str(it, "apiKey");
                if (!k.empty())
                    set_str("apiKey", k);
                else if (!old || *str(old, "apiKey") == '\0')
                    set_str("apiKey", "");
                if (s == config::Sonarr || s == config::Radarr)
                    set_bool("is4k", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(it, "is4k")));
            }
            cJSON_AddItemToArray(made.get(), o);
        }
        cJSON_DeleteItemFromObjectCaseSensitive(cur.get(), key);
        const int n = cJSON_GetArraySize(made.get());
        if (n == 1)   /* one instance: written as one object, as most files have it */
            cJSON_AddItemToObject(cur.get(), key, cJSON_DetachItemFromArray(made.get(), 0));
        else if (n > 1)
            cJSON_AddItemToObject(cur.get(), key, made.release());
    }
    *out = print(cur.get(), true) + "\n";
    config::Config check;
    if (!config::parse(*out, &check)) {
        *error = "the result is not valid";
        return false;
    }
    return true;
}

bool unreadable(const std::string &config_text)
{
    for (char c : config_text)
        if (!std::isspace((unsigned char)c)) {
            config::Config c2;
            return !config::parse(config_text, &c2);
        }
    return false;   /* empty: nothing worth keeping */
}

bool is_local_address(const std::string &ip)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    char tail = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) == 4 && a < 256 && b < 256 && c < 256 &&
        d < 256)
        return a == 10 || a == 127 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) ||
               (a == 169 && b == 254);
    if (ip == "::1")
        return true;
    const std::string lower = [&] {
        std::string l = ip;
        for (char &ch : l)
            ch = (char)std::tolower((unsigned char)ch);
        return l;
    }();
    if (lower.rfind("::ffff:", 0) == 0)
        return is_local_address(lower.substr(7));
    return lower.rfind("fd", 0) == 0 || lower.rfind("fc", 0) == 0 || lower.rfind("fe80:", 0) == 0;
}

} // namespace config_web
