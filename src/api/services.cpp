/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/services.h"
#include "api/services_call.h"

#include <cstdio>

#include "api/arr.h"
#include "api/glances.h"
#include "api/jellyfin.h"
#include "api/playback.h"
#include "api/scrutiny.h"
#include "api/seerr.h"

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
    case http::Response::TooLarge: return Error::TooLarge;
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

namespace {
size_t s_list_limit = 64u << 20;
} // namespace

void set_list_limit(size_t bytes) { s_list_limit = bytes; }
size_t list_limit() { return s_list_limit; }

Services::Services(const config::Config &cfg, http::Pool &pool, std::string lang)
    : m_cfg(cfg), m_pool(pool), m_lang(std::move(lang)), m_jellyfin(make_jellyfin(*this)), m_emby(make_emby(*this)),
      m_plex(make_plex(*this))
{
}

Services::~Services() = default;

MediaServer &Services::media() const
{
    return m_cfg.plex() ? *m_plex : m_cfg.emby() ? *m_emby : *m_jellyfin;
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
    if (config::keyless(s)) {   /* none, or HTTP Basic when Glances (or a proxy) asks for a password */
        const config::ServiceConfig &c = m_cfg.at(s, instance);
        return c.username.empty() ? std::string() : http::header("Authorization", "Basic " + base64(c.username + ":" + c.password));
    }
    const config::ServiceConfig &c = m_cfg.at(s, instance);
    const std::string &key = c.api_key;
    if (s == config::Jellyfin)   /* Jellyfin's, Emby's or Plex's (api/media_server) */
        return media().auth_header(c);
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
    if (s == config::Jellyfin)   /* who Régie5 is, on every request (Plex) */
        for (std::string &h : media().client_headers())
            r.headers.push_back(std::move(h));
    r.insecure = m_cfg.at(s, instance).insecure;
    r.server = m_cfg.at(s, instance).url;   /* each service its own turn (http::Pool) */
    return r;
}

std::vector<std::string> Services::slow(double min_seconds, double *longest) const
{
    std::vector<std::pair<double, std::string>> found;
    for (const auto &kv : m_pool.waiting()) {
        if (kv.second < min_seconds)
            continue;
        for (int s = 0; s < config::ServiceCount; s++)
            for (int i = 0; i < m_cfg.count((config::Service)s); i++)
                if (m_cfg.at((config::Service)s, i).url == kv.first && !kv.first.empty())
                    found.push_back({kv.second, m_cfg.at((config::Service)s, i).name});
    }
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<std::string> out;
    for (const auto &f : found)
        if (std::find(out.begin(), out.end(), f.second) == out.end())
            out.push_back(f.second);
    if (longest)
        *longest = found.empty() ? 0 : found.front().first;
    return out;
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
    case config::Scrutiny:
        path = scrutiny::summary_path();
        parse = scrutiny::parse_test;
        break;
    default:
        path = media().test_path();
        parse = media().test_parse();
        break;
    }
    call<std::string>(s, "GET", path, parse, std::move(done), instance, {}, true, 0, true);
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
        std::move(done), instance, {}, true, list_limit());
}

void Services::movies(std::function<void(Result<std::vector<model::Movie>>)> done, int instance)
{
    call<std::vector<model::Movie>>(
        config::Radarr, "GET", arr::movies_path(),
        [instance](const std::string &b, std::vector<model::Movie> *o) {
            return tagged(arr::parse_movies(b, o), o, instance);
        },
        std::move(done), instance, {}, true, list_limit());
}

void Services::user_items(const std::string &user_id, std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    media().user_items(user_id, std::move(done));
}

void Services::jellyfin_index(std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    media().index(std::move(done));
}

void Services::server_id(std::function<void(Result<std::string>)> done)
{
    media().server_id(std::move(done));
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
    media().activity(since, std::move(done));
}

void Services::libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done)
{
    media().libraries(std::move(done));
}

void Services::library_items(const std::string &library_id,
                             std::function<void(Result<std::vector<model::UserItem>>)> done)
{
    media().library_items(library_id, std::move(done));
}

void Services::library_count(const std::string &library_id, const std::string &type,
                             std::function<void(Result<int>)> done)
{
    media().library_count(library_id, type, std::move(done));
}

void Services::user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done)
{
    media().user(user_id, std::move(done));
}

void Services::users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done)
{
    media().users(std::move(done));
}

void Services::items(const std::vector<std::string> &ids,
                     std::function<void(Result<std::vector<model::JellyfinItem>>)> done)
{
    media().items(ids, std::move(done));
}

void Services::encoding(std::function<void(Result<model::EncodingInfo>)> done)
{
    media().encoding(std::move(done));
}

void Services::server_load(std::function<void(Result<model::ServerLoad>)> done)
{
    call<model::ServerLoad>(config::Glances, "GET", glances::quicklook_path(), glances::parse_quicklook,
                            std::move(done));
}

void Services::disks(std::function<void(Result<model::DiskReport>)> done)
{
    /* Its settings first (which failures count; the default when they do
     * not answer), then the disks. */
    call<int>(config::Scrutiny, "GET", scrutiny::settings_path(), scrutiny::parse_settings,
              [this, done](Result<int> settings) {
                  const int threshold = settings.ok() ? settings.value : 3;
                  call<model::DiskReport>(
                      config::Scrutiny, "GET", scrutiny::summary_path(),
                      [threshold](const std::string &b, model::DiskReport *o) {
                          o->threshold = threshold;
                          return scrutiny::parse_summary(b, &o->disks);
                      },
                      done);
              });
}

void Services::disk_detail(const std::string &key, std::function<void(Result<model::DiskDetail>)> done)
{
    const std::string path = scrutiny::details_path(key);
    if (path.empty()) {
        Result<model::DiskDetail> r;
        r.error = Error::BadResponse;
        if (done)
            done(std::move(r));
        return;
    }
    call<model::DiskDetail>(config::Scrutiny, "GET", path, scrutiny::parse_details, std::move(done));
}

bool Services::stats_possible() const
{
    return m_cfg.media == config::MediaServer::Jellyfin && enabled(config::Jellyfin) && m_cfg.at(config::Jellyfin).stats;
}

void Services::stats_plugin(std::function<void(Result<std::string>)> done)
{
    if (!stats_possible()) {
        Result<std::string> r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
        return;
    }
    call<std::string>(config::Jellyfin, "GET", playback::plugins_path(), playback::parse_plugins, std::move(done));
}

void Services::play_stats(int days, std::function<void(Result<model::PlayStats>)> done)
{
    if (!stats_possible()) {
        Result<model::PlayStats> r;
        r.error = Error::Disabled;
        if (done)
            done(std::move(r));
        return;
    }
    const time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    const std::string period = playback::period_query(days, now, local.tm_gmtoff);
    /* Four answers, one after the other, into one: the first failure is the result's. */
    auto stats = std::make_shared<model::PlayStats>();
    auto fail = [done](Error e, int status) {
        Result<model::PlayStats> r;
        r.error = e;
        r.http_status = status;
        if (done)
            done(std::move(r));
    };
    using Users = std::vector<model::PlayStats::User>;
    using Days = std::vector<std::pair<std::string, int64_t>>;
    using Titles = std::vector<model::PlayStats::Title>;
    call<Users>(config::Jellyfin, "GET", playback::users_path(period), playback::parse_users,
                [this, period, stats, fail, done](Result<Users> u) {
                    if (!u.ok())
                        return fail(u.error, u.http_status);
                    stats->users = std::move(u.value);
                    call<Days>(config::Jellyfin, "GET", playback::days_path(period), playback::parse_days,
                               [this, period, stats, fail, done](Result<Days> d) {
                                   if (!d.ok())
                                       return fail(d.error, d.http_status);
                                   stats->days = std::move(d.value);
                                   call<Titles>(config::Jellyfin, "GET", playback::movies_path(period),
                                                playback::parse_titles,
                                                [this, period, stats, fail, done](Result<Titles> m) {
                                                    if (!m.ok())
                                                        return fail(m.error, m.http_status);
                                                    stats->movies = std::move(m.value);
                                                    call<Titles>(config::Jellyfin, "GET", playback::series_path(period),
                                                                 playback::parse_titles,
                                                                 [stats, fail, done](Result<Titles> s) {
                                                                     if (!s.ok())
                                                                         return fail(s.error, s.http_status);
                                                                     stats->series = std::move(s.value);
                                                                     Result<model::PlayStats> r;
                                                                     r.value = std::move(*stats);
                                                                     if (done)
                                                                         done(std::move(r));
                                                                 });
                                                });
                               });
                });
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
    media().sessions(std::move(done));
}

void Services::tasks(std::function<void(Result<std::vector<model::Task>>)> done)
{
    media().tasks(std::move(done));
}

void Services::recent(std::function<void(Result<std::vector<model::RecentItem>>)> done)
{
    media().recent(std::move(done));
}

void Services::stop_session(const std::string &session_id, std::function<void(Result<bool>)> done)
{
    media().stop_session(session_id, std::move(done));
}

void Services::refresh_library(std::function<void(Result<bool>)> done)
{
    media().refresh_library(std::move(done));
}

std::string Services::media_image(const std::string &id, const char *type, int width, const std::string &tag) const
{
    if (id.empty())
        return {};
    return media().image_path(id, type, width, tag);
}

std::string Services::media_fetch_path(const std::string &path) const { return media().fetch_path(path); }

} // namespace api
