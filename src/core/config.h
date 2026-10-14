/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * config.json: for each service, whether it is on, its address and its API
 * key. The keys never leave this struct for anything but a request header:
 * nothing logs them.
 */
#pragma once

#include <string>
#include <vector>

namespace config {

/* Glances is optional (the server's load, read-only): an address, and a user
 * and a password only if it asks for them. */
enum Service { Sonarr = 0, Radarr, Seerr, Jellyfin, Glances, ServiceCount };

/* The media server in the Jellyfin slot: Jellyfin, or Emby (config.json's
 * "emby" key, in place of "jellyfin"). Both speak the same API, with a few
 * differences that api/jellyfin handles; every screen is the same. */
enum class MediaServer { Jellyfin, Emby };

const char *service_name(Service s);   /* "Sonarr" ... (Jellyfin: "Jellyfin", see media_name) */
const char *service_key(Service s);    /* "sonarr" ...: its key in config.json */

struct ServiceConfig {
    std::string name;     /* the instance's name ("Radarr 4K"); the service's by default */
    bool enabled = false;
    std::string url;      /* "http://192.168.1.10:8989", no trailing slash */
    std::string api_key;
    std::string username, password;   /* Glances behind --password (HTTP Basic) */
    /* "is4k": a Radarr / Sonarr that Seerr knows as its 4K server. Said in
     * config.json, never guessed from the name. */
    bool is4k = false;
    /* "insecure": an https:// address whose certificate is not checked (a
     * self-signed one, on the local network). Off by default. */
    bool insecure = false;
    bool operator==(const ServiceConfig &o) const
    {
        return name == o.name && enabled == o.enabled && url == o.url && api_key == o.api_key &&
               username == o.username && password == o.password && is4k == o.is4k &&
               insecure == o.insecure;
    }
};

/* A service is one object, or a list of named instances (Radarr and Radarr
 * 4K): services[s] is the first, extra[s] the others. */
struct Config {
    ServiceConfig services[ServiceCount];
    std::vector<ServiceConfig> extra[ServiceCount];
    /* Jellyfin accounts the cleanup does not count among "everyone" (names,
     * any case): test accounts, guests. "cleanup": {"ignoreUsers": [...]} */
    std::vector<std::string> ignore_users;
    /* "emby" in place of "jellyfin". Both on: Jellyfin is used and both_media
     * says so (Settings shows it). */
    MediaServer media = MediaServer::Jellyfin;
    bool both_media = false;

    const ServiceConfig &at(Service s, int instance = 0) const
    {
        return instance <= 0 || instance > (int)extra[s].size() ? services[s] : extra[s][instance - 1];
    }
    int count(Service s) const { return 1 + (int)extra[s].size(); }
    bool emby() const { return media == MediaServer::Emby; }
};

/* "Jellyfin" or "Emby": the name the interface gives the media server. */
const char *media_name(MediaServer m);

enum class LoadError { None, Missing, Unreadable, BadJson };

/* Parses config.json's text. A service that is absent, or has no url or key,
 * is off. Returns false on malformed JSON. */
bool parse(const std::string &text, Config *out);

/* Reads and parses the file at path. */
LoadError load(const std::string &path, Config *out);

/* Which services differ between a and b: bit i for service i. */
unsigned changed(const Config &a, const Config &b);

/* The configuration --mock runs on: every service on (but those named in off,
 * separated by commas), at addresses only the mock transport answers. multi:
 * a second Radarr ("Radarr 4K", under a path, as behind a reverse proxy).
 * emby: Emby in the media server's place. */
Config mock(const std::string &off = {}, bool multi = false, bool emby = false);

} // namespace config
