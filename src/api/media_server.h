/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What Régie5 asks of the media server, whichever it is: Jellyfin, Emby (a
 * Jellyfin with its own header, picture sizes and dates) or Plex. One class
 * each, chosen by config.json's media server (Services keeps all three, so
 * that answers still on their way after config.json changed find theirs);
 * each builds its requests through Services (address, auth, the pool, the
 * errors), with api/jellyfin and api/plex for the paths and the parsing.
 */
#pragma once

#include "api/services.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace api {

class MediaServer {
public:
    explicit MediaServer(Services &services) : m_s(services) {}
    virtual ~MediaServer() = default;
    MediaServer(const MediaServer &) = delete;
    MediaServer &operator=(const MediaServer &) = delete;

    /* Who Régie5 is to the server: the auth header (empty: none), and the
     * headers sent with every request (Plex's X-Plex-*). */
    virtual std::string auth_header(const config::ServiceConfig &c) const = 0;
    virtual std::vector<std::string> client_headers() const { return {}; }
    /* How a refusal (401, 403) reads for this account (an ended session,
     * not the owner...); null: as api::classify says. Runs on a worker. */
    virtual std::function<Error(int status)> refusals(const config::ServiceConfig &c) const
    {
        (void)c;
        return nullptr;
    }
    /* The connection test: an authenticated read that answers the version. */
    virtual std::string test_path() const = 0;
    virtual std::function<bool(const std::string &, std::string *)> test_parse() const = 0;
    /* A picture's path (an item's poster or backdrop at a width), and the
     * path it is fetched by (Emby: another size parameter). */
    virtual std::string image_path(const std::string &id, const char *type, int width, const std::string &tag) const = 0;
    virtual std::string fetch_path(const std::string &path) const { return path; }

    virtual void sessions(std::function<void(Result<std::vector<model::Session>>)> done) = 0;
    virtual void tasks(std::function<void(Result<std::vector<model::Task>>)> done) = 0;
    virtual void recent(std::function<void(Result<std::vector<model::RecentItem>>)> done) = 0;
    virtual void activity(time_t since, std::function<void(Result<std::vector<model::ActivityEntry>>)> done) = 0;
    virtual void libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done) = 0;
    virtual void library_items(const std::string &library_id,
                               std::function<void(Result<std::vector<model::UserItem>>)> done) = 0;
    virtual void library_count(const std::string &library_id, const std::string &type,
                               std::function<void(Result<int>)> done) = 0;
    virtual void user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done) = 0;
    virtual void users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done) = 0;
    virtual void items(const std::vector<std::string> &ids,
                       std::function<void(Result<std::vector<model::JellyfinItem>>)> done) = 0;
    virtual void encoding(std::function<void(Result<model::EncodingInfo>)> done) = 0;
    /* The cleanup: one user's films and series, with what they watched. */
    virtual void user_items(const std::string &user_id, std::function<void(Result<std::vector<model::UserItem>>)> done) = 0;
    /* "Ouvrir dans Jelly5": every title with its ids, and the server's Id. */
    virtual void index(std::function<void(Result<std::vector<model::UserItem>>)> done) = 0;
    virtual void server_id(std::function<void(Result<std::string>)> done) = 0;
    /* Actions: POST. They change things on the server. */
    virtual void stop_session(const std::string &session_id, std::function<void(Result<bool>)> done) = 0;
    virtual void refresh_library(std::function<void(Result<bool>)> done) = 0;

protected:
    /* Services::call on the media server's slot (config::Jellyfin). */
    template <typename T>
    void call(const std::string &method, const std::string &path, std::function<bool(const std::string &, T *)> parse,
              std::function<void(Result<T>)> done, const std::string &body = {}, size_t max_body = 0);
    const config::Config &cfg() const { return m_s.config(); }
    const std::string &device_id() const { return m_s.m_device_id; }
    const std::string &device_name() const { return m_s.m_device_name; }
    const std::string &version() const { return m_s.m_version; }
    /* An answer at once that the call does not apply here. */
    template <typename T> static void off(const std::function<void(Result<T>)> &done)
    {
        Result<T> r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
    }

    Services &m_s;
};

/* One of each, for Services. */
std::unique_ptr<MediaServer> make_jellyfin(Services &s);
std::unique_ptr<MediaServer> make_emby(Services &s);
std::unique_ptr<MediaServer> make_plex(Services &s);

} // namespace api
