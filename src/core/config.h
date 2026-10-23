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

/* Glances (the server's load) and Scrutiny (its disks' health) are optional
 * and read-only: an address, and a user and a password only if they ask for
 * them (HTTP Basic, a reverse proxy's). */
enum Service { Sonarr = 0, Radarr, Seerr, Jellyfin, Glances, Scrutiny, ServiceCount };

/* The media server in the Jellyfin slot: Jellyfin, Emby (config.json's
 * "emby" key, in place of "jellyfin") or Plex ("plex"). Emby speaks
 * Jellyfin's API, with a few differences that api/jellyfin handles; Plex has
 * an API of its own (api/plex), read into the same models. Every screen is
 * the same. */
enum class MediaServer { Jellyfin, Emby, Plex };

const char *service_name(Service s);   /* "Sonarr" ... (Jellyfin: "Jellyfin", see media_name) */
const char *service_key(Service s);    /* "sonarr" ...: its key in config.json */

struct ServiceConfig {
    std::string name;     /* the instance's name ("Radarr 4K"); the service's by default */
    bool enabled = false;
    std::string url;      /* "http://192.168.1.10:8989", no trailing slash */
    std::string api_key;
    std::string username, password;   /* Glances behind --password, Scrutiny behind a proxy (HTTP Basic) */
    /* Plex: the server's token ("token") is kept in api_key, the one secret
     * of a service. Without one, Plex lets in only the networks it was told
     * to (its "allowed without auth" setting). */
    /* "is4k": a Radarr / Sonarr that Seerr knows as its 4K server. Said in
     * config.json, never guessed from the name. */
    bool is4k = false;
    /* "insecure": an https:// address whose certificate is not checked (a
     * self-signed one, on the local network). Off by default. */
    bool insecure = false;
    /* Jellyfin without an API key: the session Quick Connect gave ("accessToken",
     * written by the app), the account's name ("userName") and whether it is an
     * administrator ("isAdmin"; some screens need one). An API key wins. */
    std::string token, user;
    bool admin = true;
    /* The media server's "statistics": false hides Statistiques even when
     * Playback Reporting is there. */
    bool stats = true;
    /* Signed in by Quick Connect (no API key: the account's session). */
    bool session() const { return api_key.empty() && !token.empty(); }
    bool operator==(const ServiceConfig &o) const
    {
        return name == o.name && enabled == o.enabled && url == o.url && api_key == o.api_key &&
               username == o.username && password == o.password && is4k == o.is4k &&
               insecure == o.insecure && token == o.token && admin == o.admin && stats == o.stats;
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
    /* "emby" or "plex" in place of "jellyfin". Two or three on: the first of
     * Jellyfin, Emby, Plex is used and both_media says so (Settings shows it). */
    MediaServer media = MediaServer::Jellyfin;
    bool both_media = false;

    const ServiceConfig &at(Service s, int instance = 0) const
    {
        return instance <= 0 || instance > (int)extra[s].size() ? services[s] : extra[s][instance - 1];
    }
    int count(Service s) const { return 1 + (int)extra[s].size(); }
    bool emby() const { return media == MediaServer::Emby; }
    bool plex() const { return media == MediaServer::Plex; }
};

/* "Jellyfin", "Emby" or "Plex": the name the interface gives the media server. */
const char *media_name(MediaServer m);

/* The optional services read with no key (Glances, Scrutiny). */
inline bool keyless(Service s) { return s == Glances || s == Scrutiny; }

/* SetAside: readable, but not loaded at this start (core/boot_guard: the
 * app crashed at start with it, twice in a row). */
enum class LoadError { None, Missing, Unreadable, BadJson, SetAside };

/* Parses config.json's text. A service that is absent, or has no url or key,
 * is off. Returns false on malformed JSON. */
bool parse(const std::string &text, Config *out);

/* Reads and parses the file at path. */
LoadError load(const std::string &path, Config *out);

/* config.json's text with a Quick Connect session written into its
 * "jellyfin" (the first instance of a list): accessToken, userName, isAdmin;
 * everything else kept. false when the text has no "jellyfin" to put it in. */
bool with_jellyfin_session(const std::string &text, const std::string &token, const std::string &user, bool admin,
                           std::string *out);
/* A text file whole (up to 1 MiB); false when it cannot be read. */
bool read_file(const std::string &path, std::string *out);
/* A text file written whole, then renamed over the old one (never half a file). */
bool write_file(const std::string &path, const std::string &text);

/* Which services differ between a and b: bit i for service i. */
unsigned changed(const Config &a, const Config &b);

/* The configuration --mock runs on: every service on (but those named in off,
 * separated by commas), at addresses only the mock transport answers. multi:
 * a second Radarr ("Radarr 4K", under a path, as behind a reverse proxy).
 * media: the media server (Emby, Plex: in Jellyfin's place). */
Config mock(const std::string &off = {}, bool multi = false, MediaServer media = MediaServer::Jellyfin);

} // namespace config
