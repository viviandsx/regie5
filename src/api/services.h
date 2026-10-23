/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The four services behind one object: builds each request (address, auth
 * header), runs it on the pool, parses on the worker and hands the result to
 * the main thread. Every call is asynchronous; callbacks run in Pool::pump().
 */
#pragma once

#include "api/http.h"
#include "api/seerr.h"
#include "core/config.h"
#include "core/model.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace api {

class MediaServer;

/* Why a call failed, in the terms the interface explains. */
enum class Error {
    None,
    Disabled,       /* off in config.json */
    Unreachable,    /* no connection (address, port, server down) */
    Timeout,        /* connected, no answer in time */
    Unauthorized,   /* 401/403: the key */
    NotFound,       /* 404: the address points somewhere else */
    Redirected,     /* 3xx: a proxy's login page, or http:// sent to https:// (never followed: the key would go along) */
    Http,           /* another HTTP status */
    BadResponse,    /* answered, but not what this service says */
    Unsupported,    /* an address this platform cannot reach yet (https on the PS5) */
    Certificate,    /* https: the server's certificate is refused */
    Revoked,        /* Jellyfin by Quick Connect: 401, the session has ended (revoked, device removed); Plex: its token refused */
    NeedsAdmin,     /* Jellyfin by Quick Connect: 403 for an account that is not an administrator; Plex: not the owner */
    NeedsPlexPass,  /* Plex: an action refused to an owner without Plex Pass (stopping a playback) */
    TooLarge,       /* a library list past list_limit(): given up before it fills the memory */
};

/* The most a library list (every film of a Radarr, every title of a media
 * server) may weigh as text: past it, Error::TooLarge rather than a crash.
 * Set by the platform from its memory (64 MiB by default). */
void set_list_limit(size_t bytes);
size_t list_limit();

/* From a transport response and whether its body parsed. */
Error classify(const http::Response &r, bool parsed);

template <typename T> struct Result {
    Error error = Error::None;
    int http_status = 0;
    T value{};
    bool ok() const { return error == Error::None; }
};

class Services {
public:
    Services(const config::Config &cfg, http::Pool &pool, std::string lang);
    ~Services();

    const config::Config &config() const { return m_cfg; }
    /* Who Régie5 is to Jellyfin when it signs in (Quick Connect): this
     * console's id (settings.json), its name, the app's version. */
    void set_client(std::string device_id, std::string device_name, std::string version)
    {
        m_device_id = std::move(device_id);
        m_device_name = std::move(device_name);
        m_version = std::move(version);
    }
    /* A new config.json (main thread). Requests already sent keep the
     * address and key they were made with. */
    void reconfigure(const config::Config &cfg) { m_cfg = cfg; }
    bool enabled(config::Service s, int instance = 0) const { return m_cfg.at(s, instance).enabled; }
    /* How many instances a service has (1 unless config.json lists more). */
    int instances(config::Service s) const { return m_cfg.count(s); }
    /* The services with a request waiting longer than min_seconds, by name
     * ("Sonarr", "Radarr 4K"), the longest wait first, and that wait. */
    std::vector<std::string> slow(double min_seconds, double *longest = nullptr) const;
    /* No request waiting or on its way. */
    bool idle() const { return m_pool.busy() == 0; }
    /* A path on a service, as a full URL (images included). The service's url
     * may hold a path of its own (a reverse proxy, the *arr's urlBase). */
    std::string url(config::Service s, const std::string &path, int instance = 0) const;
    /* The auth header for a service (for images fetched elsewhere); empty when none. */
    std::string auth_header(config::Service s, int instance = 0) const;

    /* The connection test: an authenticated read that answers the version
     * (tried even while the server is set aside, http::Pool). */
    void test(config::Service s, std::function<void(Result<std::string>)> done, int instance = 0);

    /* Sonarr / Radarr: one instance each call (the first by default); what
     * comes back carries its instance. */
    void health(config::Service s, std::function<void(Result<std::vector<model::Health>>)> done, int instance = 0);
    void diskspace(config::Service s, std::function<void(Result<std::vector<model::DiskSpace>>)> done,
                   int instance = 0);
    void calendar(config::Service s, time_t start, time_t end,
                  std::function<void(Result<std::vector<model::CalendarItem>>)> done, int instance = 0);
    void queue(config::Service s, std::function<void(Result<std::vector<model::QueueItem>>)> done, int instance = 0);

    /* Libraries (read-only). */
    void series(std::function<void(Result<std::vector<model::Series>>)> done, int instance = 0);
    void movies(std::function<void(Result<std::vector<model::Movie>>)> done, int instance = 0);
    struct MissingEpisodes {
        std::vector<model::MissingEpisode> items;
        int total = 0;
    };
    struct MissingMovies {
        std::vector<model::Movie> items;
        int total = 0;
    };
    void missing_episodes(std::function<void(Result<MissingEpisodes>)> done, int instance = 0);
    void missing_movies(std::function<void(Result<MissingMovies>)> done, int instance = 0);
    void quality_profiles(config::Service s, std::function<void(Result<std::vector<model::QualityProfile>>)> done,
                          int instance = 0);
    void episode_files(int series_id, std::function<void(Result<model::FileSummary>)> done, int instance = 0);

    /* The journal. */
    void imports(config::Service s, time_t since, std::function<void(Result<std::vector<model::ImportEvent>>)> done,
                 int instance = 0);
    void recent_requests(std::function<void(Result<model::RequestPage>)> done);
    void activity(time_t since, std::function<void(Result<std::vector<model::ActivityEntry>>)> done);
    void users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done);
    /* One user by id: Emby's activity log names users by a number /Users lacks. */
    void user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done);
    /* The cleanup. One user's films and series, with what they watched. */
    void user_items(const std::string &user_id, std::function<void(Result<std::vector<model::UserItem>>)> done);
    /* DELETE, files included, kept out of import lists. They change things on the server. */
    void delete_movie(int instance, int movie_id, std::function<void(Result<bool>)> done);
    void delete_series(int instance, int series_id, std::function<void(Result<bool>)> done);

    /* Jellyfin's libraries: the list, one's films and series, a count. */
    void libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done);
    void library_items(const std::string &library_id, std::function<void(Result<std::vector<model::UserItem>>)> done);
    void library_count(const std::string &library_id, const std::string &type, std::function<void(Result<int>)> done);

    /* The playback diagnosis. */
    void encoding(std::function<void(Result<model::EncodingInfo>)> done);
    void server_load(std::function<void(Result<model::ServerLoad>)> done);   /* Glances */
    /* Scrutiny (optional): every disk, and a disk's last week. */
    void disks(std::function<void(Result<model::DiskReport>)> done);
    void disk_detail(const std::string &key, std::function<void(Result<model::DiskDetail>)> done);
    /* Playback Reporting (optional, Jellyfin only): its version, "" when it is
     * not there (Disabled: not Jellyfin, or "statistics": false). */
    void stats_plugin(std::function<void(Result<std::string>)> done);
    /* The last days' statistics (7, 30, 365), ending today. */
    void play_stats(int days, std::function<void(Result<model::PlayStats>)> done);
    void items(const std::vector<std::string> &ids, std::function<void(Result<std::vector<model::JellyfinItem>>)> done);

    /* Seerr. */
    enum class RequestFilter { Pending, Approved, All };
    void requests(RequestFilter f, std::function<void(Result<model::RequestPage>)> done);
    void detail(bool tv, int tmdb_id, std::function<void(Result<model::MediaDetail>)> done);
    /* Actions: POST. They change things on the server. */
    void approve(int request_id, std::function<void(Result<bool>)> done);
    void decline(int request_id, std::function<void(Result<bool>)> done);
    /* Issues: the list (comments without their authors), one with them. */
    void issues(seerr::IssueFilter f, std::function<void(Result<model::IssuePage>)> done);
    void issue(int issue_id, std::function<void(Result<model::Issue>)> done);
    /* Actions: POST. They change things on the server. */
    void resolve_issue(int issue_id, std::function<void(Result<bool>)> done);
    void comment_issue(int issue_id, const std::string &message, std::function<void(Result<bool>)> done);
    /* Discover and search (in the interface's language), a title's page,
     * what a request depends on. */
    void discover(seerr::Shelf shelf, std::function<void(Result<std::vector<model::SeerrTitle>>)> done);
    void search(const std::string &query, std::function<void(Result<std::vector<model::SeerrTitle>>)> done);
    void title_detail(bool tv, int tmdb_id, std::function<void(Result<model::SeerrDetail>)> done);
    void seerr_public(std::function<void(Result<model::SeerrPublic>)> done);
    void seerr_servers(bool tv, std::function<void(Result<std::vector<model::SeerrServer>>)> done);
    void seerr_server(bool tv, int server_id, std::function<void(Result<model::SeerrServer>)> done);
    /* Action: POST. It changes things in Seerr (a new request, as the API
     * key's account). */
    void create_request(const seerr::NewRequest &r, std::function<void(Result<model::SeerrCreated>)> done);

    /* The media server's picture of an item: its poster ("Primary") or
     * backdrop ("Backdrop"), at a width (Jellyfin, Emby, Plex alike). */
    std::string media_image(const std::string &id, const char *type, int width, const std::string &tag = {}) const;
    /* A media server picture's path as it is fetched (Emby asks another size parameter). */
    std::string media_fetch_path(const std::string &path) const;

    /* Jellyfin (or Emby, or Plex). */
    void sessions(std::function<void(Result<std::vector<model::Session>>)> done);
    void tasks(std::function<void(Result<std::vector<model::Task>>)> done);
    void recent(std::function<void(Result<std::vector<model::RecentItem>>)> done);
    /* "Ouvrir dans Jelly5": every film and series with their ids, and the server's Id. */
    void jellyfin_index(std::function<void(Result<std::vector<model::UserItem>>)> done);
    void server_id(std::function<void(Result<std::string>)> done);
    /* Actions: POST. */
    void stop_session(const std::string &session_id, std::function<void(Result<bool>)> done);
    void refresh_library(std::function<void(Result<bool>)> done);
    /* Quick Connect: asked of Jellyfin's address even while it is off (no key
     * yet). Never on Emby. */
    void quick_connect_enabled(std::function<void(Result<bool>)> done);
    void quick_connect_start(std::function<void(Result<model::QuickConnect>)> done);
    void quick_connect_poll(const std::string &secret, std::function<void(Result<bool>)> done);
    void quick_connect_finish(const std::string &secret, std::function<void(Result<model::JellyfinSession>)> done);

private:
    friend class MediaServer;   /* api/media_server: Jellyfin, Emby, Plex */
    /* The media server config.json names (the three are kept: answers on
     * their way after config.json changed still find theirs). */
    MediaServer &media() const;
    /* Statistics can be asked: Jellyfin, on, and not "statistics": false. */
    bool stats_possible() const;
    http::Request make(config::Service s, const std::string &method, const std::string &path, int instance = 0) const;
    /* parse: on the worker; false when the body is not what was expected. */
    template <typename T>
    void call(config::Service s, const std::string &method, const std::string &path,
              std::function<bool(const std::string &, T *)> parse, std::function<void(Result<T>)> done,
              int instance = 0, const std::string &body = {}, bool need_on = true, size_t max_body = 0,
              bool fresh = false);
    config::Config m_cfg;
    http::Pool &m_pool;
    std::string m_lang;
    std::string m_device_id = "regie5", m_device_name = "Regie5", m_version;
    std::unique_ptr<MediaServer> m_jellyfin, m_emby, m_plex;
};

} // namespace api
