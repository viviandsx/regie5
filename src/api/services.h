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
#include "core/config.h"
#include "core/model.h"

#include <functional>
#include <string>
#include <vector>

namespace api {

/* Why a call failed, in the terms the interface explains. */
enum class Error {
    None,
    Disabled,       /* off in config.json */
    Unreachable,    /* no connection (address, port, server down) */
    Timeout,        /* connected, no answer in time */
    Unauthorized,   /* 401/403: the key */
    NotFound,       /* 404: the address points somewhere else */
    Http,           /* another HTTP status */
    BadResponse,    /* answered, but not what this service says */
    Unsupported,    /* an address this platform cannot reach yet (https on the PS5) */
    Certificate,    /* https: the server's certificate is refused */
};

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

    const config::Config &config() const { return m_cfg; }
    /* A new config.json (main thread). Requests already sent keep the
     * address and key they were made with. */
    void reconfigure(const config::Config &cfg) { m_cfg = cfg; }
    bool enabled(config::Service s, int instance = 0) const { return m_cfg.at(s, instance).enabled; }
    /* How many instances a service has (1 unless config.json lists more). */
    int instances(config::Service s) const { return m_cfg.count(s); }
    /* A path on a service, as a full URL (images included). The service's url
     * may hold a path of its own (a reverse proxy, the *arr's urlBase). */
    std::string url(config::Service s, const std::string &path, int instance = 0) const;
    /* The auth header for a service (for images fetched elsewhere); empty when none. */
    std::string auth_header(config::Service s, int instance = 0) const;

    /* The connection test: an authenticated read that answers the version. */
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
    /* The cleanup. One user's films and series, with what they watched. */
    void user_items(const std::string &user_id, std::function<void(Result<std::vector<model::UserItem>>)> done);
    /* DELETE, files included, kept out of import lists. They change things on the server. */
    void delete_movie(int instance, int movie_id, std::function<void(Result<bool>)> done);
    void delete_series(int instance, int series_id, std::function<void(Result<bool>)> done);

    /* The playback diagnosis. */
    void encoding(std::function<void(Result<model::EncodingInfo>)> done);
    void server_load(std::function<void(Result<model::ServerLoad>)> done);   /* Glances */
    void items(const std::vector<std::string> &ids, std::function<void(Result<std::vector<model::JellyfinItem>>)> done);

    /* Seerr. */
    enum class RequestFilter { Pending, Approved, All };
    void requests(RequestFilter f, std::function<void(Result<model::RequestPage>)> done);
    void detail(bool tv, int tmdb_id, std::function<void(Result<model::MediaDetail>)> done);
    /* Actions: POST. They change things on the server. */
    void approve(int request_id, std::function<void(Result<bool>)> done);
    void decline(int request_id, std::function<void(Result<bool>)> done);

    /* Jellyfin. */
    void sessions(std::function<void(Result<std::vector<model::Session>>)> done);
    void tasks(std::function<void(Result<std::vector<model::Task>>)> done);
    void recent(std::function<void(Result<std::vector<model::RecentItem>>)> done);
    /* "Ouvrir dans Jelly5": every film and series with their ids, and the server's Id. */
    void jellyfin_index(std::function<void(Result<std::vector<model::UserItem>>)> done);
    void server_id(std::function<void(Result<std::string>)> done);
    /* Actions: POST. */
    void stop_session(const std::string &session_id, std::function<void(Result<bool>)> done);
    void refresh_library(std::function<void(Result<bool>)> done);

private:
    http::Request make(config::Service s, const std::string &method, const std::string &path, int instance = 0) const;
    /* parse: on the worker; false when the body is not what was expected. */
    template <typename T>
    void call(config::Service s, const std::string &method, const std::string &path,
              std::function<bool(const std::string &, T *)> parse, std::function<void(Result<T>)> done,
              int instance = 0, const std::string &body = {});

    config::Config m_cfg;
    http::Pool &m_pool;
    std::string m_lang;
};

} // namespace api
