/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/services.h"

#include <cstdio>

#include "api/arr.h"
#include "api/glances.h"
#include "api/jellyfin.h"
#include "api/plex.h"
#include "api/seerr.h"
#include "core/i18n.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>

namespace api {

Error classify(const http::Response &r, bool parsed)
{
    switch (r.fail) {
    case http::Response::Connect: return Error::Unreachable;
    case http::Response::Timeout: return Error::Timeout;
    case http::Response::Other: return Error::Unreachable;
    case http::Response::Unsupported: return Error::Unsupported;
    case http::Response::Certificate: return Error::Certificate;
    case http::Response::None: break;
    }
    if (r.status == 401 || r.status == 403)
        return Error::Unauthorized;
    if (r.status == 404)
        return Error::NotFound;
    if (r.status >= 300 && r.status < 400)
        return Error::Redirected;
    if (r.status < 200 || r.status >= 300)
        return Error::Http;
    return parsed ? Error::None : Error::BadResponse;
}

Services::Services(const config::Config &cfg, http::Pool &pool, std::string lang)
    : m_cfg(cfg), m_pool(pool), m_lang(std::move(lang))
{
}

std::string Services::url(config::Service s, const std::string &path, int instance) const
{
    return m_cfg.at(s, instance).url + path;
}

namespace {

std::string base64(const std::string &in)
{
    static const char *const kAbc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out += kAbc[(val >> bits) & 0x3f];
            bits -= 6;
        }
    }
    if (bits > -6)
        out += kAbc[((val << 8) >> (bits + 8)) & 0x3f];
    while (out.size() % 4)
        out += '=';
    return out;
}

} // namespace

std::string Services::auth_header(config::Service s, int instance) const
{
    if (s == config::Glances) {   /* none, or HTTP Basic when Glances asks for a password */
        const config::ServiceConfig &c = m_cfg.at(s, instance);
        return c.username.empty() ? std::string() : http::header("Authorization", "Basic " + base64(c.username + ":" + c.password));
    }
    const config::ServiceConfig &c = m_cfg.at(s, instance);
    const std::string &key = c.api_key;
    if (s == config::Jellyfin && m_cfg.plex())
        return plex::token_header(key);   /* none without a token */
    if (s == config::Jellyfin && m_cfg.emby())
        return http::header("X-Emby-Token", key);   /* Emby's documented header */
    if (s == config::Jellyfin && key.empty())   /* Quick Connect: who signs in, and the session once there */
        return http::header("Authorization", jellyfin::auth_client(m_device_id, m_device_name, m_version, c.token));
    if (s == config::Jellyfin)
        return http::header("Authorization", jellyfin::auth_value(key));
    return http::header("X-Api-Key", key);
}

http::Request Services::make(config::Service s, const std::string &method, const std::string &path, int instance) const
{
    http::Request r;
    r.method = method;
    r.url = url(s, path, instance);
    const std::string auth = auth_header(s, instance);
    if (!auth.empty())
        r.headers.push_back(auth);
    r.headers.push_back("Accept: application/json");
    if (s == config::Jellyfin && m_cfg.plex())   /* who Régie5 is, on every Plex request */
        for (std::string &h : plex::client_headers(m_device_id, m_device_name, m_version))
            r.headers.push_back(std::move(h));
    r.insecure = m_cfg.at(s, instance).insecure;
    return r;
}

template <typename T>
void Services::call(config::Service s, const std::string &method, const std::string &path,
                    std::function<bool(const std::string &, T *)> parse, std::function<void(Result<T>)> done,
                    int instance, const std::string &body, bool need_on)
{
    if (need_on ? !enabled(s, instance) : m_cfg.at(s, instance).url.empty()) {
        Result<T> res;
        res.error = Error::Disabled;
        if (done)
            done(std::move(res));
        return;
    }
    http::Request req = make(s, method, path, instance);
    req.body = body;
    /* Signed in by Quick Connect: a 401 is a session that ended, a 403 an
     * account that is not an administrator (never the key's fault). */
    const config::ServiceConfig &c = m_cfg.at(s, instance);
    const bool session = s == config::Jellyfin && m_cfg.media == config::MediaServer::Jellyfin && c.session();
    const bool admin = c.admin;
    /* Plex: refused to an account that is not the owner (owner-only calls),
     * a token it no longer takes, or no token at all. */
    const bool plex = s == config::Jellyfin && m_cfg.plex(), token = !c.api_key.empty();
    m_pool.submit(std::move(req), [parse, done, session, admin, plex, token](http::Response &resp) -> std::function<void()> {
        auto res = std::make_shared<Result<T>>();
        res->http_status = resp.status;
        const bool parsed = resp.ok() && parse(resp.body, &res->value);
        res->error = classify(resp, parsed);
        if (session && resp.status == 401)
            res->error = Error::Revoked;
        else if (session && resp.status == 403 && !admin)
            res->error = Error::NeedsAdmin;
        else if (plex && (resp.status == 401 || resp.status == 403))
            res->error = !admin ? Error::NeedsAdmin : token ? Error::Revoked : Error::Unauthorized;
        return [done, res] {
            if (done)
                done(std::move(*res));
        };
    });
}

namespace {

/* Each element marked with the instance it came from. */
template <typename V> bool tagged(bool ok, V *v, int instance)
{
    if (ok)
        for (auto &x : *v)
            x.instance = instance;
    return ok;
}

} // namespace

void Services::test(config::Service s, std::function<void(Result<std::string>)> done, int instance)
{
    std::string path;
    std::function<bool(const std::string &, std::string *)> parse;
    switch (s) {
    case config::Sonarr:
    case config::Radarr:
        path = arr::status_path();
        parse = arr::parse_status;
        break;
    case config::Seerr:
        path = seerr::about_path();
        parse = seerr::parse_about;
        break;
    case config::Glances:
        path = glances::quicklook_path();
        parse = glances::parse_test;
        break;
    default:
        path = m_cfg.plex() ? plex::info_path() : jellyfin::info_path();
        parse = m_cfg.plex() ? plex::parse_info : jellyfin::parse_info;
        break;
    }
    call<std::string>(s, "GET", path, parse, std::move(done), instance);
}

void Services::health(config::Service s, std::function<void(Result<std::vector<model::Health>>)> done, int instance)
{
    call<std::vector<model::Health>>(
        s, "GET", arr::health_path(),
        [s, instance](const std::string &b, std::vector<model::Health> *o) {
            return tagged(arr::parse_health(b, s, o), o, instance);
        },
        std::move(done), instance);
}

void Services::diskspace(config::Service s, std::function<void(Result<std::vector<model::DiskSpace>>)> done,
                         int instance)
{
    call<std::vector<model::DiskSpace>>(
        s, "GET", arr::diskspace_path(),
        [s, instance](const std::string &b, std::vector<model::DiskSpace> *o) {
            return tagged(arr::parse_diskspace(b, s, o), o, instance);
        },
        std::move(done), instance);
}

void Services::calendar(config::Service s, time_t start, time_t end,
                        std::function<void(Result<std::vector<model::CalendarItem>>)> done, int instance)
{
    call<std::vector<model::CalendarItem>>(
        s, "GET", arr::calendar_path(s, start, end),
        [s, start, end, instance](const std::string &b, std::vector<model::CalendarItem> *o) {
            return tagged(s == config::Sonarr ? arr::parse_sonarr_calendar(b, o)
                                              : arr::parse_radarr_calendar(b, start, end, o),
                          o, instance);
        },
        std::move(done), instance);
}

void Services::queue(config::Service s, std::function<void(Result<std::vector<model::QueueItem>>)> done, int instance)
{
    call<std::vector<model::QueueItem>>(
        s, "GET", arr::queue_path(s),
        [s, instance](const std::string &b, std::vector<model::QueueItem> *o) {
            return tagged(arr::parse_queue(b, s, time(nullptr), o), o, instance);
        },
        std::move(done), instance);
}

void Services::series(std::function<void(Result<std::vector<model::Series>>)> done, int instance)
{
    call<std::vector<model::Series>>(
        config::Sonarr, "GET", arr::series_path(),
        [instance](const std::string &b, std::vector<model::Series> *o) {
            return tagged(arr::parse_series(b, o), o, instance);
        },
        std::move(done), instance);
}

void Services::movies(std::function<void(Result<std::vector<model::Movie>>)> done, int instance)
{
    call<std::vector<model::Movie>>(
        config::Radarr, "GET", arr::movies_path(),
        [instance](const std::string &b, std::vector<model::Movie> *o) {
            return tagged(arr::parse_movies(b, o), o, instance);
        },
        std::move(done), instance);
}

void Services::user_items(const std::string &user_id, std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    using Items = Result<std::vector<model::UserItem>>;
    if (m_cfg.plex()) {   /* no cleanup with Plex yet (docs/PLEX.md § 2.10) */
        Items r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
        return;
    }
    if (!m_cfg.emby()) {
        call<std::vector<model::UserItem>>(config::Jellyfin, "GET", jellyfin::user_items_path(user_id),
                                           jellyfin::parse_user_items, std::move(done));
        return;
    }
    /* Emby dates no series, even one watched to the end: its latest episode
     * gives the date (else the cleanup would take it for long unwatched).
     * Without that answer, nothing: no list on half the facts. */
    call<std::vector<model::UserItem>>(
        config::Jellyfin, "GET", jellyfin::user_items_path(user_id, true), jellyfin::parse_user_items,
        [this, user_id, done](Items items) {
            if (!items.ok())
                return done(std::move(items));
            auto shared = std::make_shared<Items>(std::move(items));
            call<std::map<std::string, time_t>>(
                config::Jellyfin, "GET", jellyfin::played_episodes_path(user_id), jellyfin::parse_played_episodes,
                [shared, done](Result<std::map<std::string, time_t>> played) {
                    if (!played.ok()) {
                        Items failed;
                        failed.error = played.error;
                        failed.http_status = played.http_status;
                        return done(std::move(failed));
                    }
                    for (auto &i : shared->value) {
                        auto f = played.value.find(i.id);
                        if (i.series && f != played.value.end())
                            i.last_played = std::max(i.last_played, f->second);
                    }
                    done(std::move(*shared));
                });
        });
}

void Services::jellyfin_index(std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    if (m_cfg.plex()) {   /* Jelly5's links: never with Plex (its titles are looked for by name) */
        Result<std::vector<model::UserItem>> r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
        return;
    }
    call<std::vector<model::UserItem>>(config::Jellyfin, "GET", jellyfin::index_path(), jellyfin::parse_user_items,
                                       std::move(done));
}

void Services::server_id(std::function<void(Result<std::string>)> done)
{
    if (m_cfg.plex()) {   /* Jelly5's: never with Plex */
        Result<std::string> r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
        return;
    }
    call<std::string>(config::Jellyfin, "GET", jellyfin::info_path(), jellyfin::parse_server_id, std::move(done));
}

void Services::missing_episodes(std::function<void(Result<MissingEpisodes>)> done, int instance)
{
    call<MissingEpisodes>(
        config::Sonarr, "GET", arr::missing_path(config::Sonarr),
        [instance](const std::string &b, MissingEpisodes *o) {
            return tagged(arr::parse_missing_episodes(b, &o->items, &o->total), &o->items, instance);
        },
        std::move(done), instance);
}

void Services::missing_movies(std::function<void(Result<MissingMovies>)> done, int instance)
{
    call<MissingMovies>(
        config::Radarr, "GET", arr::missing_path(config::Radarr),
        [instance](const std::string &b, MissingMovies *o) {
            return tagged(arr::parse_missing_movies(b, &o->items, &o->total), &o->items, instance);
        },
        std::move(done), instance);
}

void Services::quality_profiles(config::Service s,
                                std::function<void(Result<std::vector<model::QualityProfile>>)> done, int instance)
{
    call<std::vector<model::QualityProfile>>(s, "GET", arr::quality_profiles_path(), arr::parse_quality_profiles,
                                             std::move(done), instance);
}

void Services::episode_files(int series_id, std::function<void(Result<model::FileSummary>)> done, int instance)
{
    call<model::FileSummary>(config::Sonarr, "GET", arr::episode_files_path(series_id), arr::parse_episode_files,
                             std::move(done), instance);
}

void Services::imports(config::Service s, time_t since,
                       std::function<void(Result<std::vector<model::ImportEvent>>)> done, int instance)
{
    call<std::vector<model::ImportEvent>>(
        s, "GET", arr::history_path(s, since),
        [s, instance](const std::string &b, std::vector<model::ImportEvent> *o) {
            if (!arr::parse_history_imports(b, s, o))
                return false;
            for (auto &e : *o)
                e.instance = instance;
            return true;
        },
        std::move(done), instance);
}

void Services::recent_requests(std::function<void(Result<model::RequestPage>)> done)
{
    call<model::RequestPage>(config::Seerr, "GET", seerr::recent_requests_path(), seerr::parse_requests,
                             std::move(done));
}

void Services::activity(time_t since, std::function<void(Result<std::vector<model::ActivityEntry>>)> done)
{
    if (m_cfg.plex()) {   /* Plex keeps no activity log: its finished playbacks */
        call<std::vector<model::ActivityEntry>>(config::Jellyfin, "GET", plex::history_path(since), plex::parse_history,
                                                std::move(done));
        return;
    }
    call<std::vector<model::ActivityEntry>>(config::Jellyfin, "GET", jellyfin::activity_path(since),
                                            jellyfin::parse_activity, std::move(done));
}

void Services::libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done)
{
    if (m_cfg.plex()) {
        call<std::vector<model::JellyfinLibrary>>(config::Jellyfin, "GET", plex::sections_path(), plex::parse_sections,
                                                  std::move(done));
        return;
    }
    call<std::vector<model::JellyfinLibrary>>(config::Jellyfin, "GET", jellyfin::libraries_path(),
                                              jellyfin::parse_libraries, std::move(done));
}

void Services::library_items(const std::string &library_id,
                             std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    if (m_cfg.plex()) {
        call<std::vector<model::UserItem>>(config::Jellyfin, "GET", plex::section_items_path(library_id),
                                           plex::parse_section_items, std::move(done));
        return;
    }
    call<std::vector<model::UserItem>>(config::Jellyfin, "GET", jellyfin::library_items_path(library_id),
                                       jellyfin::parse_user_items, std::move(done));
}

void Services::library_count(const std::string &library_id, const std::string &type,
                             std::function<void(Result<int>)> done)
{
    if (m_cfg.plex()) {
        call<int>(config::Jellyfin, "GET", plex::count_path(library_id, type), plex::parse_count, std::move(done));
        return;
    }
    call<int>(config::Jellyfin, "GET", jellyfin::library_count_path(library_id, type), jellyfin::parse_count,
              std::move(done));
}

void Services::user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done)
{
    if (m_cfg.plex()) {
        call<model::JellyfinUser>(config::Jellyfin, "GET", plex::account_path(user_id), plex::parse_account,
                                  std::move(done));
        return;
    }
    call<model::JellyfinUser>(config::Jellyfin, "GET", jellyfin::user_path(user_id), jellyfin::parse_user,
                              std::move(done));
}

void Services::users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done)
{
    if (m_cfg.plex())
        return plex_users(std::move(done));
    call<std::vector<model::JellyfinUser>>(config::Jellyfin, "GET", jellyfin::users_path(), jellyfin::parse_users,
                                           std::move(done));
}

void Services::items(const std::vector<std::string> &ids,
                     std::function<void(Result<std::vector<model::JellyfinItem>>)> done)
{
    if (m_cfg.plex()) {
        call<std::vector<model::JellyfinItem>>(config::Jellyfin, "GET", plex::metadata_path(ids), plex::parse_items,
                                               std::move(done));
        return;
    }
    call<std::vector<model::JellyfinItem>>(config::Jellyfin, "GET", jellyfin::items_path(ids), jellyfin::parse_items,
                                           std::move(done));
}

void Services::encoding(std::function<void(Result<model::EncodingInfo>)> done)
{
    if (m_cfg.plex())
        return plex_encoding(std::move(done));
    call<model::EncodingInfo>(config::Jellyfin, "GET", jellyfin::encoding_path(), jellyfin::parse_encoding,
                              std::move(done));
}

void Services::server_load(std::function<void(Result<model::ServerLoad>)> done)
{
    call<model::ServerLoad>(config::Glances, "GET", glances::quicklook_path(), glances::parse_quicklook,
                            std::move(done));
}

void Services::requests(RequestFilter f, std::function<void(Result<model::RequestPage>)> done)
{
    const seerr::Filter sf = f == RequestFilter::Pending    ? seerr::Filter::Pending
                             : f == RequestFilter::Approved ? seerr::Filter::Approved
                                                            : seerr::Filter::All;
    call<model::RequestPage>(config::Seerr, "GET", seerr::requests_path(sf), seerr::parse_requests, std::move(done));
}

void Services::detail(bool tv, int tmdb_id, std::function<void(Result<model::MediaDetail>)> done)
{
    call<model::MediaDetail>(
        config::Seerr, "GET", seerr::detail_path(tv, tmdb_id, m_lang),
        [tv](const std::string &b, model::MediaDetail *o) { return seerr::parse_detail(b, tv, o); }, std::move(done));
}

namespace {
bool accept_any(const std::string &, bool *out)
{
    *out = true;
    return true;
}
} // namespace

void Services::delete_movie(int instance, int movie_id, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Radarr, "DELETE", arr::delete_movie_path(movie_id), accept_any, std::move(done), instance);
}

void Services::delete_series(int instance, int series_id, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Sonarr, "DELETE", arr::delete_series_path(series_id), accept_any, std::move(done), instance);
}

void Services::approve(int request_id, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Seerr, "POST", seerr::approve_path(request_id), accept_any, std::move(done));
}

void Services::decline(int request_id, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Seerr, "POST", seerr::decline_path(request_id), accept_any, std::move(done));
}

void Services::issues(seerr::IssueFilter f, std::function<void(Result<model::IssuePage>)> done)
{
    call<model::IssuePage>(config::Seerr, "GET", seerr::issues_path(f), seerr::parse_issues, std::move(done));
}

void Services::issue(int issue_id, std::function<void(Result<model::Issue>)> done)
{
    call<model::Issue>(config::Seerr, "GET", seerr::issue_path(issue_id), seerr::parse_issue, std::move(done));
}

void Services::resolve_issue(int issue_id, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Seerr, "POST", seerr::issue_resolve_path(issue_id), accept_any, std::move(done));
}

void Services::comment_issue(int issue_id, const std::string &message, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Seerr, "POST", seerr::issue_comment_path(issue_id), accept_any, std::move(done), 0,
               seerr::comment_body(message));
}

void Services::discover(seerr::Shelf shelf, std::function<void(Result<std::vector<model::SeerrTitle>>)> done)
{
    call<std::vector<model::SeerrTitle>>(config::Seerr, "GET", seerr::discover_path(shelf, m_lang), seerr::parse_titles,
                                         std::move(done));
}

void Services::search(const std::string &query, std::function<void(Result<std::vector<model::SeerrTitle>>)> done)
{
    call<std::vector<model::SeerrTitle>>(config::Seerr, "GET", seerr::search_path(query, m_lang), seerr::parse_titles,
                                         std::move(done));
}

void Services::title_detail(bool tv, int tmdb_id, std::function<void(Result<model::SeerrDetail>)> done)
{
    call<model::SeerrDetail>(config::Seerr, "GET", seerr::detail_path(tv, tmdb_id, m_lang),
                             [tv](const std::string &b, model::SeerrDetail *o) { return seerr::parse_title_detail(b, tv, o); },
                             std::move(done));
}

void Services::seerr_public(std::function<void(Result<model::SeerrPublic>)> done)
{
    call<model::SeerrPublic>(config::Seerr, "GET", seerr::public_settings_path(), seerr::parse_public, std::move(done));
}

void Services::seerr_servers(bool tv, std::function<void(Result<std::vector<model::SeerrServer>>)> done)
{
    call<std::vector<model::SeerrServer>>(config::Seerr, "GET", seerr::servers_path(tv), seerr::parse_servers,
                                          std::move(done));
}

void Services::seerr_server(bool tv, int server_id, std::function<void(Result<model::SeerrServer>)> done)
{
    call<model::SeerrServer>(config::Seerr, "GET", seerr::server_path(tv, server_id), seerr::parse_server,
                             std::move(done));
}

void Services::create_request(const seerr::NewRequest &r, std::function<void(Result<model::SeerrCreated>)> done)
{
#ifdef REGIE5_DRY_REQUESTS
    /* A console test build (scripts/build-ps5.sh with REGIE5_DRY_REQUESTS=1):
     * the request is logged, never sent; it answers as --mock does (204). */
    std::fprintf(stderr, "[regie5] Seerr request NOT sent (test build): %s\n", seerr::request_body(r).c_str());
    Result<model::SeerrCreated> res;
    res.http_status = 204;
    if (done)
        done(std::move(res));
    return;
#endif
    call<model::SeerrCreated>(config::Seerr, "POST", seerr::create_request_path(), seerr::parse_created,
                              std::move(done), 0, seerr::request_body(r));
}

void Services::quick_connect_enabled(std::function<void(Result<bool>)> done)
{
    call<bool>(config::Jellyfin, "GET", jellyfin::quick_connect_enabled_path(), jellyfin::parse_quick_connect_enabled,
               std::move(done), 0, {}, false);
}

void Services::quick_connect_start(std::function<void(Result<model::QuickConnect>)> done)
{
    call<model::QuickConnect>(config::Jellyfin, "POST", jellyfin::quick_connect_initiate_path(),
                              jellyfin::parse_quick_connect, std::move(done), 0, {}, false);
}

void Services::quick_connect_poll(const std::string &secret, std::function<void(Result<bool>)> done)
{
    call<bool>(config::Jellyfin, "GET", jellyfin::quick_connect_poll_path(secret), jellyfin::parse_quick_connect_poll,
               std::move(done), 0, {}, false);
}

void Services::quick_connect_finish(const std::string &secret,
                                    std::function<void(Result<model::JellyfinSession>)> done)
{
    call<model::JellyfinSession>(config::Jellyfin, "POST", jellyfin::quick_connect_auth_path(), jellyfin::parse_session,
                                 std::move(done), 0, jellyfin::quick_connect_auth_body(secret), false);
}

void Services::sessions(std::function<void(Result<std::vector<model::Session>>)> done)
{
    if (m_cfg.plex())
        return plex_sessions(std::move(done));
    call<std::vector<model::Session>>(config::Jellyfin, "GET", jellyfin::sessions_path(), jellyfin::parse_sessions,
                                      std::move(done));
}

void Services::tasks(std::function<void(Result<std::vector<model::Task>>)> done)
{
    if (m_cfg.plex())
        return plex_tasks(std::move(done));
    call<std::vector<model::Task>>(config::Jellyfin, "GET", jellyfin::tasks_path(), jellyfin::parse_tasks,
                                   std::move(done));
}

void Services::recent(std::function<void(Result<std::vector<model::RecentItem>>)> done)
{
    if (m_cfg.plex())
        return plex_recent(std::move(done));
    call<std::vector<model::RecentItem>>(
        config::Jellyfin, "GET", jellyfin::recent_path(),
        [](const std::string &b, std::vector<model::RecentItem> *o) { return jellyfin::parse_recent(b, o); },
        std::move(done));
}

void Services::stop_session(const std::string &session_id, std::function<void(Result<bool>)> done)
{
    if (m_cfg.plex()) {
        /* Refused to the owner: Plex Pass, it seems (docs/PLEX.md); never "the token". */
        const bool owner = m_cfg.at(config::Jellyfin).admin;
        call<bool>(config::Jellyfin, "POST", plex::stop_path(session_id, i18n::tr(i18n::PlexStopReason)), accept_any,
                   [done, owner](Result<bool> r) {
                       if (owner && (r.http_status == 401 || r.http_status == 403))
                           r.error = Error::NeedsPlexPass;
                       if (done)
                           done(std::move(r));
                   });
        return;
    }
    call<bool>(config::Jellyfin, "POST", jellyfin::stop_path(session_id), accept_any, std::move(done));
}

void Services::refresh_library(std::function<void(Result<bool>)> done)
{
    if (m_cfg.plex())
        return plex_refresh(std::move(done));
    call<bool>(config::Jellyfin, "POST", jellyfin::library_refresh_path(), accept_any, std::move(done));
}

std::string Services::media_image(const std::string &id, const char *type, int width, const std::string &tag) const
{
    if (id.empty())
        return {};
    return m_cfg.plex() ? plex::image_path(id, type, width) : jellyfin::image_path(id, type, width, tag);
}

/* ---- Plex ---- */

namespace {

/* Several answers gathered on the main thread, then handed on once: the
 * first failure is kept, to say why when nothing came. */
template <typename T> struct Gather {
    int left = 0;
    Error error = Error::None;
    int http_status = 0;
    T value{};
    void failed(Error e, int status)
    {
        if (error == Error::None)
            error = e, http_status = status;
    }
};

} // namespace

void Services::plex_sessions(std::function<void(Result<std::vector<model::Session>>)> done)
{
    using Sessions = Result<std::vector<model::Session>>;
    call<std::vector<model::Session>>(
        config::Jellyfin, "GET", plex::sessions_path(), plex::parse_sessions, [this, done](Sessions list) {
            if (!list.ok() || list.value.empty())
                return done(std::move(list));
            /* A session's media is what is sent: the source is in each item's metadata. */
            auto shared = std::make_shared<Sessions>(std::move(list));
            auto left = std::make_shared<int>((int)shared->value.size());
            for (size_t i = 0; i < shared->value.size(); i++) {
                call<model::Session>(
                    config::Jellyfin, "GET", plex::metadata_path({shared->value[i].item_id}),
                    [base = shared->value[i]](const std::string &b, model::Session *o) {
                        *o = base;
                        return plex::apply_source(b, o);
                    },
                    [this, shared, left, i, done](Result<model::Session> r) {
                        if (r.ok())   /* else: shown without its source */
                            shared->value[i] = std::move(r.value);
                        if (--*left > 0)
                            return;
                        /* A server may leave a session's account unnamed: the accounts name it. */
                        const bool unnamed = std::any_of(shared->value.begin(), shared->value.end(), [](const model::Session &s) {
                            return s.user.empty() && !s.user_id.empty();
                        });
                        if (!unnamed)
                            return done(std::move(*shared));
                        call<std::vector<model::JellyfinUser>>(
                            config::Jellyfin, "GET", plex::accounts_path(), plex::parse_accounts,
                            [shared, done](Result<std::vector<model::JellyfinUser>> users) {
                                for (auto &s : shared->value)
                                    for (const auto &u : users.value)
                                        if (s.user.empty() && u.id == s.user_id)
                                            s.user = u.name;
                                done(std::move(*shared));
                            });
                    });
            }
        });
}

void Services::plex_encoding(std::function<void(Result<model::EncodingInfo>)> done)
{
    call<model::EncodingInfo>(config::Jellyfin, "GET", plex::prefs_path(), plex::parse_prefs,
                              [this, done](Result<model::EncodingInfo> prefs) {
                                  if (!prefs.ok())
                                      return done(std::move(prefs));
                                  auto shared = std::make_shared<Result<model::EncodingInfo>>(std::move(prefs));
                                  call<bool>(config::Jellyfin, "GET", plex::info_path(), plex::parse_plex_pass,
                                             [shared, done](Result<bool> pass) {
                                                 if (pass.ok())
                                                     shared->value.plex_pass = pass.value;
                                                 done(std::move(*shared));
                                             });
                              });
}

void Services::plex_tasks(std::function<void(Result<std::vector<model::Task>>)> done)
{
    using Tasks = Result<std::vector<model::Task>>;
    call<std::vector<model::Task>>(
        config::Jellyfin, "GET", plex::butler_path(), plex::parse_butler, [this, done](Tasks butler) {
            if (!butler.ok())
                return done(std::move(butler));
            auto shared = std::make_shared<Tasks>(std::move(butler));
            call<std::vector<model::Task>>(config::Jellyfin, "GET", plex::activities_path(), plex::parse_activities,
                                           [shared, done](Tasks running) {
                                               /* what runs now first (a scan's progress), then the Butler's */
                                               if (running.ok())
                                                   shared->value.insert(shared->value.begin(), running.value.begin(),
                                                                        running.value.end());
                                               done(std::move(*shared));
                                           });
        });
}

void Services::plex_recent(std::function<void(Result<std::vector<model::RecentItem>>)> done)
{
    using Recent = Result<std::vector<model::RecentItem>>;
    call<std::vector<model::JellyfinLibrary>>(
        config::Jellyfin, "GET", plex::sections_path(), plex::parse_sections,
        [this, done](Result<std::vector<model::JellyfinLibrary>> libs) {
            Recent out;
            out.error = libs.error, out.http_status = libs.http_status;
            if (!libs.ok())
                return done(std::move(out));
            auto g = std::make_shared<Gather<std::vector<model::RecentItem>>>();
            std::vector<std::pair<std::string, bool>> asks;   /* a library, and whether its latest are episodes */
            for (const auto &l : libs.value)
                if (l.kind == model::JellyfinLibrary::Movies || l.kind == model::JellyfinLibrary::Shows)
                    asks.push_back({l.id, l.kind == model::JellyfinLibrary::Shows});
            if (asks.empty())
                return done(std::move(out));
            g->left = (int)asks.size();
            auto ok_count = std::make_shared<int>(0);
            for (const auto &[id, episodes] : asks)
                call<std::vector<model::RecentItem>>(
                    config::Jellyfin, "GET", plex::recent_path(id, episodes, episodes ? 60 : 20), plex::parse_recent,
                    [g, ok_count, done](Recent r) {
                        if (r.ok()) {
                            ++*ok_count;
                            g->value.insert(g->value.end(), r.value.begin(), r.value.end());
                        } else {
                            g->failed(r.error, r.http_status);
                        }
                        if (--g->left > 0)
                            return;
                        Recent all;
                        if (*ok_count == 0)   /* nothing came: why */
                            all.error = g->error, all.http_status = g->http_status;
                        else
                            all.value = plex::merge_recent(std::move(g->value));
                        done(std::move(all));
                    });
        });
}

void Services::plex_refresh(std::function<void(Result<bool>)> done)
{
    call<std::vector<model::JellyfinLibrary>>(
        config::Jellyfin, "GET", plex::sections_path(), plex::parse_sections,
        [this, done](Result<std::vector<model::JellyfinLibrary>> libs) {
            Result<bool> out;
            out.error = libs.error, out.http_status = libs.http_status;
            if (!libs.ok() || libs.value.empty())
                return done(std::move(out));
            auto g = std::make_shared<Gather<bool>>();
            g->left = (int)libs.value.size();
            for (const auto &l : libs.value)   /* every library: Plex scans them one by one */
                call<bool>(config::Jellyfin, "POST", plex::section_refresh_path(l.id), accept_any,
                           [g, done](Result<bool> r) {
                               if (!r.ok())
                                   g->failed(r.error, r.http_status);
                               if (--g->left > 0)
                                   return;
                               Result<bool> all;
                               all.error = g->error, all.http_status = g->http_status;
                               all.value = g->error == Error::None;
                               done(std::move(all));
                           });
        });
}

void Services::plex_users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done)
{
    using Users = Result<std::vector<model::JellyfinUser>>;
    call<std::vector<model::JellyfinUser>>(
        config::Jellyfin, "GET", plex::accounts_path(), plex::parse_accounts, [this, done](Users users) {
            if (!users.ok() || users.value.empty())
                return done(std::move(users));
            /* Plex dates no account: its latest playback, from the last six months' history. */
            auto shared = std::make_shared<Users>(std::move(users));
            call<std::vector<model::ActivityEntry>>(
                config::Jellyfin, "GET", plex::history_path(std::time(nullptr) - 183 * 86400, 1000), plex::parse_history,
                [shared, done](Result<std::vector<model::ActivityEntry>> h) {
                    if (h.ok()) {
                        const auto last = plex::last_played_by_account(h.value);
                        for (auto &u : shared->value) {
                            auto f = last.find(u.id);
                            if (f != last.end())
                                u.last_activity = f->second;
                        }
                    }
                    done(std::move(*shared));
                });
        });
}

} // namespace api
