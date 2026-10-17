/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/plex.h"

#include "api/json.h"

namespace plex {

namespace {

/* A header's value: no line break, nothing outside printable ASCII. */
std::string header_safe(const std::string &s)
{
    std::string out;
    for (char c : s)
        if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f)
            out += c;
    return out;
}

/* Plex writes some fields as a number or a string depending on the call
 * ("librarySectionID": 1 or "1", "allowSync": true or "1"): read leniently. */
bool flag(const cJSON *o, const char *key, bool def = false)
{
    const cJSON *v = json::get(o, key);
    if (cJSON_IsBool(v))
        return cJSON_IsTrue(v);
    if (cJSON_IsNumber(v))
        return v->valuedouble != 0;
    if (cJSON_IsString(v) && v->valuestring)
        return std::string(v->valuestring) == "1" || std::string(v->valuestring) == "true";
    return def;
}

/* The answer's MediaContainer, which every Plex answer is wrapped in. */
const cJSON *container(const json::Doc &doc) { return json::get(doc.root(), "MediaContainer"); }

} // namespace

std::vector<std::string> client_headers(const std::string &client_id, const std::string &device_name,
                                        const std::string &version)
{
    return {
        "X-Plex-Client-Identifier: " + header_safe(client_id),
        "X-Plex-Product: Regie5",
        "X-Plex-Version: " + header_safe(version),
        "X-Plex-Platform: " + header_safe(device_name == "PS5" ? "PlayStation" : "Linux"),
        "X-Plex-Device-Name: " + header_safe(device_name),
    };
}

std::string token_header(const std::string &token)
{
    return token.empty() ? std::string() : "X-Plex-Token: " + header_safe(token);
}

std::string identity_path() { return "/identity"; }
std::string info_path() { return "/"; }

bool parse_identity(const std::string &body, std::string *machine_id, std::string *version)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    *machine_id = json::str(c, "machineIdentifier");
    *version = json::str(c, "version");
    return !machine_id->empty();
}

bool parse_info(const std::string &body, std::string *version)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c) || !json::get(c, "machineIdentifier"))
        return false;   /* not a Plex server's root */
    *version = json::str(c, "version");
    return true;
}

bool parse_plex_pass(const std::string &body, bool *plex_pass)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    *plex_pass = flag(c, "myPlexSubscription");
    return true;
}

} // namespace plex
