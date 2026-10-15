/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/app_settings.h"

#include "cJSON.h"

#include <cerrno>
#include <cstdio>

namespace app_settings {

bool Settings::is_kept(config::Service s, const std::string &instance, int id) const
{
    for (const auto &k : kept)
        if (k.service == s && k.instance == instance && k.id == id)
            return true;
    return false;
}

std::string path_beside(const std::string &config_path)
{
    const size_t slash = config_path.rfind('/');
    return (slash == std::string::npos ? std::string(".") : config_path.substr(0, slash)) + "/settings.json";
}

const char *refresh_key(Refresh r) { return r == Refresh::Hz60 ? "60" : r == Refresh::Hz120 ? "120" : "auto"; }

int refresh_asked(Refresh r) { return r == Refresh::Hz60 ? 60 : 120; }

std::string to_json(const Settings &s)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *display = cJSON_AddObjectToObject(root, "display");
    cJSON_AddStringToObject(display, "refresh", refresh_key(s.refresh));
    cJSON *c = cJSON_AddObjectToObject(root, "cleanup");
    cJSON_AddBoolToObject(c, "enabled", s.cleanup_enabled);
    cJSON_AddStringToObject(c, "watchedBy", s.cleanup_everyone ? "everyone" : "anyone");
    cJSON_AddNumberToObject(c, "months", s.cleanup_months);
    cJSON *kept = cJSON_AddArrayToObject(c, "kept");
    for (const auto &k : s.kept) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "service", config::service_key(k.service));
        cJSON_AddStringToObject(o, "instance", k.instance.c_str());
        cJSON_AddNumberToObject(o, "id", k.id);
        cJSON_AddStringToObject(o, "title", k.title.c_str());
        cJSON_AddItemToArray(kept, o);
    }
    if (s.welcome_later)
        cJSON_AddStringToObject(root, "welcome", "later");
    if (!s.seen_version.empty())
        cJSON_AddStringToObject(root, "seenVersion", s.seen_version.c_str());
    if (!s.device_id.empty())
        cJSON_AddStringToObject(root, "deviceId", s.device_id.c_str());
    char *text = cJSON_Print(root);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(root);
    return out + "\n";
}

bool from_json(const std::string &text, Settings *out)
{
    *out = Settings{};
    cJSON *root = cJSON_ParseWithLength(text.data(), text.size());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    const cJSON *refresh = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "display"), "refresh");
    if (cJSON_IsString(refresh))
        for (Refresh r : {Refresh::Auto, Refresh::Hz60, Refresh::Hz120})
            if (std::string(refresh->valuestring) == refresh_key(r))
                out->refresh = r;
    const cJSON *welcome = cJSON_GetObjectItemCaseSensitive(root, "welcome");
    out->welcome_later = cJSON_IsString(welcome) && std::string(welcome->valuestring) == "later";
    const cJSON *seen = cJSON_GetObjectItemCaseSensitive(root, "seenVersion");
    if (cJSON_IsString(seen) && std::string(seen->valuestring).size() < 32)
        out->seen_version = seen->valuestring;
    const cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "deviceId");
    if (cJSON_IsString(device) && std::string(device->valuestring).size() <= 64)
        out->device_id = device->valuestring;
    const cJSON *c = cJSON_GetObjectItemCaseSensitive(root, "cleanup");
    out->cleanup_enabled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c, "enabled"));
    const cJSON *by = cJSON_GetObjectItemCaseSensitive(c, "watchedBy");
    out->cleanup_everyone = !(cJSON_IsString(by) && std::string(by->valuestring) == "anyone");
    const cJSON *months = cJSON_GetObjectItemCaseSensitive(c, "months");
    if (cJSON_IsNumber(months) && months->valueint > 0 && months->valueint <= 120)
        out->cleanup_months = months->valueint;
    const cJSON *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(c, "kept"))
    {
        const cJSON *svc = cJSON_GetObjectItemCaseSensitive(it, "service");
        const cJSON *inst = cJSON_GetObjectItemCaseSensitive(it, "instance");
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(it, "id");
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(it, "title");
        if (!cJSON_IsString(svc) || !cJSON_IsNumber(id))
            continue;
        Kept k;
        k.service = std::string(svc->valuestring) == "sonarr" ? config::Sonarr : config::Radarr;
        k.instance = cJSON_IsString(inst) ? inst->valuestring : "";
        k.id = id->valueint;
        k.title = cJSON_IsString(title) ? title->valuestring : "";
        out->kept.push_back(k);
    }
    cJSON_Delete(root);
    return true;
}

bool load(const std::string &path, Settings *out)
{
    *out = Settings{};
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return errno == ENOENT;
    std::string text;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && text.size() < (1u << 20))
        text.append(buf, n);
    std::fclose(f);
    if (from_json(text, out))
        return true;
    *out = Settings{};
    return false;
}

bool save(const std::string &path, const Settings &s)
{
    if (path.empty())
        return false;
    const std::string tmp = path + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    const std::string text = to_json(s);
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok) {
        std::remove(tmp.c_str());
        return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

} // namespace app_settings
