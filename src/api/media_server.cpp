/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/media_server.h"

#include "api/jellyfin.h"
#include "api/plex.h"
#include "api/services_call.h"
#include "core/i18n.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>

namespace api {

template <typename T>
void MediaServer::call(const std::string &method, const std::string &path,
                       std::function<bool(const std::string &, T *)> parse, std::function<void(Result<T>)> done,
                       const std::string &body, size_t max_body)
{
    m_s.call<T>(config::Jellyfin, method, path, std::move(parse), std::move(done), 0, body, true, max_body);
}

namespace {

/* ---- Jellyfin ---- */

class Jellyfin : public MediaServer {
public:
    using MediaServer::MediaServer;

    std::string auth_header(const config::ServiceConfig &c) const override
    {
        if (c.api_key.empty())   /* Quick Connect: who signs in, and the session once there */
            return http::header("Authorization", jellyfin::auth_client(device_id(), device_name(), version(), c.token));
        return http::header("Authorization", jellyfin::auth_value(c.api_key));
    }
    /* Signed in by Quick Connect: a 401 is a session that ended, a 403 an
     * account that is not an administrator (never the key's fault). */
    std::function<Error(int)> refusals(const config::ServiceConfig &c) const override
    {
        if (!c.session())
            return nullptr;
        const bool admin = c.admin;
        return [admin](int status) {
            if (status == 401)
                return Error::Revoked;
            if (status == 403 && !admin)
                return Error::NeedsAdmin;
            return Error::None;
        };
    }
    std::string test_path() const override { return jellyfin::info_path(); }
    std::function<bool(const std::string &, std::string *)> test_parse() const override { return jellyfin::parse_info; }
    std::string image_path(const std::string &id, const char *type, int width, const std::string &tag) const override
    {
        return jellyfin::image_path(id, type, width, tag);
    }

    void sessions(std::function<void(Result<std::vector<model::Session>>)> done) override
    {
        call<std::vector<model::Session>>("GET", jellyfin::sessions_path(), jellyfin::parse_sessions, std::move(done));
    }
    void tasks(std::function<void(Result<std::vector<model::Task>>)> done) override
    {
        call<std::vector<model::Task>>("GET", jellyfin::tasks_path(), jellyfin::parse_tasks, std::move(done));
    }
    void recent(std::function<void(Result<std::vector<model::RecentItem>>)> done) override
    {
        call<std::vector<model::RecentItem>>(
            "GET", jellyfin::recent_path(),
            [](const std::string &b, std::vector<model::RecentItem> *o) { return jellyfin::parse_recent(b, o); },
            std::move(done));
    }
    void activity(time_t since, std::function<void(Result<std::vector<model::ActivityEntry>>)> done) override
    {
        call<std::vector<model::ActivityEntry>>("GET", jellyfin::activity_path(since), jellyfin::parse_activity,
                                                std::move(done));
    }
    void libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done) override
    {
        call<std::vector<model::JellyfinLibrary>>("GET", jellyfin::libraries_path(), jellyfin::parse_libraries,
                                                  std::move(done));
    }
    void library_items(const std::string &library_id,
                       std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        call<std::vector<model::UserItem>>("GET", jellyfin::library_items_path(library_id), jellyfin::parse_user_items,
                                           std::move(done), {}, list_limit());
    }
    void library_count(const std::string &library_id, const std::string &type,
                       std::function<void(Result<int>)> done) override
    {
        call<int>("GET", jellyfin::library_count_path(library_id, type), jellyfin::parse_count, std::move(done));
    }
    void user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done) override
    {
        call<model::JellyfinUser>("GET", jellyfin::user_path(user_id), jellyfin::parse_user, std::move(done));
    }
    void users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done) override
    {
        call<std::vector<model::JellyfinUser>>("GET", jellyfin::users_path(), jellyfin::parse_users, std::move(done));
    }
    void items(const std::vector<std::string> &ids,
               std::function<void(Result<std::vector<model::JellyfinItem>>)> done) override
    {
        call<std::vector<model::JellyfinItem>>("GET", jellyfin::items_path(ids), jellyfin::parse_items, std::move(done));
    }
    void encoding(std::function<void(Result<model::EncodingInfo>)> done) override
    {
        call<model::EncodingInfo>("GET", jellyfin::encoding_path(), jellyfin::parse_encoding, std::move(done));
    }
    void user_items(const std::string &user_id,
                    std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        call<std::vector<model::UserItem>>("GET", jellyfin::user_items_path(user_id), jellyfin::parse_user_items,
                                           std::move(done), {}, list_limit());
    }
    void index(std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        call<std::vector<model::UserItem>>("GET", jellyfin::index_path(), jellyfin::parse_user_items, std::move(done), {},
                                           list_limit());
    }
    void server_id(std::function<void(Result<std::string>)> done) override
    {
        call<std::string>("GET", jellyfin::info_path(), jellyfin::parse_server_id, std::move(done));
    }
    void stop_session(const std::string &session_id, std::function<void(Result<bool>)> done) override
    {
        call<bool>("POST", jellyfin::stop_path(session_id), accept_any, std::move(done));
    }
    void refresh_library(std::function<void(Result<bool>)> done) override
    {
        call<bool>("POST", jellyfin::library_refresh_path(), accept_any, std::move(done));
    }
};

/* ---- Emby: Jellyfin's calls, with its own header, picture sizes and dates ---- */

class Emby : public Jellyfin {
public:
    using Jellyfin::Jellyfin;

    std::string auth_header(const config::ServiceConfig &c) const override
    {
        return http::header("X-Emby-Token", c.api_key);   /* Emby's documented header */
    }
    std::function<Error(int)> refusals(const config::ServiceConfig &) const override { return nullptr; }
    /* Emby ignores fillWidth (the original would come): maxWidth. */
    std::string fetch_path(const std::string &path) const override { return jellyfin::emby_image_path(path); }

    /* Emby dates no series, even one watched to the end: its latest episode
     * gives the date (else the cleanup would take it for long unwatched).
     * Without that answer, nothing: no list on half the facts. */
    void user_items(const std::string &user_id,
                    std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        using Items = Result<std::vector<model::UserItem>>;
        call<std::vector<model::UserItem>>(
            "GET", jellyfin::user_items_path(user_id, true), jellyfin::parse_user_items,
            [this, user_id, done](Items items) {
                if (!items.ok())
                    return done(std::move(items));
                auto shared = std::make_shared<Items>(std::move(items));
                call<std::map<std::string, time_t>>(
                    "GET", jellyfin::played_episodes_path(user_id), jellyfin::parse_played_episodes,
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
                    },
                    {}, list_limit());
            },
            {}, list_limit());
    }
};

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

class Plex : public MediaServer {
public:
    using MediaServer::MediaServer;

    std::string auth_header(const config::ServiceConfig &c) const override
    {
        return plex::token_header(c.api_key);   /* none without a token */
    }
    /* Who Régie5 is, on every Plex request. */
    std::vector<std::string> client_headers() const override
    {
        return plex::client_headers(device_id(), device_name(), version());
    }
    /* Refused to an account that is not the owner (owner-only calls), a
     * token it no longer takes, or no token at all. */
    std::function<Error(int)> refusals(const config::ServiceConfig &c) const override
    {
        const bool admin = c.admin, token = !c.api_key.empty();
        return [admin, token](int status) {
            if (status == 401 || status == 403)
                return !admin ? Error::NeedsAdmin : token ? Error::Revoked : Error::Unauthorized;
            return Error::None;
        };
    }
    std::string test_path() const override { return plex::info_path(); }
    std::function<bool(const std::string &, std::string *)> test_parse() const override { return plex::parse_info; }
    std::string image_path(const std::string &id, const char *type, int width, const std::string &) const override
    {
        return plex::image_path(id, type, width);
    }

    void sessions(std::function<void(Result<std::vector<model::Session>>)> done) override;
    void tasks(std::function<void(Result<std::vector<model::Task>>)> done) override;
    void recent(std::function<void(Result<std::vector<model::RecentItem>>)> done) override;
    /* Plex keeps no activity log: its finished playbacks. */
    void activity(time_t since, std::function<void(Result<std::vector<model::ActivityEntry>>)> done) override
    {
        call<std::vector<model::ActivityEntry>>("GET", plex::history_path(since), plex::parse_history, std::move(done));
    }
    void libraries(std::function<void(Result<std::vector<model::JellyfinLibrary>>)> done) override
    {
        call<std::vector<model::JellyfinLibrary>>("GET", plex::sections_path(), plex::parse_sections, std::move(done));
    }
    void library_items(const std::string &library_id,
                       std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        call<std::vector<model::UserItem>>("GET", plex::section_items_path(library_id), plex::parse_section_items,
                                           std::move(done), {}, list_limit());
    }
    void library_count(const std::string &library_id, const std::string &type,
                       std::function<void(Result<int>)> done) override
    {
        call<int>("GET", plex::count_path(library_id, type), plex::parse_count, std::move(done));
    }
    void user(const std::string &user_id, std::function<void(Result<model::JellyfinUser>)> done) override
    {
        call<model::JellyfinUser>("GET", plex::account_path(user_id), plex::parse_account, std::move(done));
    }
    void users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done) override;
    void items(const std::vector<std::string> &ids,
               std::function<void(Result<std::vector<model::JellyfinItem>>)> done) override
    {
        call<std::vector<model::JellyfinItem>>("GET", plex::metadata_path(ids), plex::parse_items, std::move(done));
    }
    void encoding(std::function<void(Result<model::EncodingInfo>)> done) override;
    /* No cleanup with Plex yet (docs/PLEX.md § 2.10). */
    void user_items(const std::string &, std::function<void(Result<std::vector<model::UserItem>>)> done) override
    {
        off(done);
    }
    /* Jelly5's links: never with Plex (its titles are looked for by name). */
    void index(std::function<void(Result<std::vector<model::UserItem>>)> done) override { off(done); }
    void server_id(std::function<void(Result<std::string>)> done) override { off(done); }
    void stop_session(const std::string &session_id, std::function<void(Result<bool>)> done) override
    {
        /* Refused to the owner: Plex Pass, it seems (docs/PLEX.md); never "the token". */
        const bool owner = cfg().at(config::Jellyfin).admin;
        call<bool>("POST", plex::stop_path(session_id, i18n::tr(i18n::PlexStopReason)), accept_any,
                   [done, owner](Result<bool> r) {
                       if (owner && (r.http_status == 401 || r.http_status == 403))
                           r.error = Error::NeedsPlexPass;
                       if (done)
                           done(std::move(r));
                   });
    }
    void refresh_library(std::function<void(Result<bool>)> done) override;
};

void Plex::sessions(std::function<void(Result<std::vector<model::Session>>)> done)
{
    using Sessions = Result<std::vector<model::Session>>;
    call<std::vector<model::Session>>(
        "GET", plex::sessions_path(), plex::parse_sessions, [this, done](Sessions list) {
            if (!list.ok() || list.value.empty())
                return done(std::move(list));
            /* A session's media is what is sent: the source is in each item's metadata. */
            auto shared = std::make_shared<Sessions>(std::move(list));
            auto left = std::make_shared<int>((int)shared->value.size());
            for (size_t i = 0; i < shared->value.size(); i++) {
                call<model::Session>(
                    "GET", plex::metadata_path({shared->value[i].item_id}),
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
                            "GET", plex::accounts_path(), plex::parse_accounts,
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

void Plex::encoding(std::function<void(Result<model::EncodingInfo>)> done)
{
    call<model::EncodingInfo>("GET", plex::prefs_path(), plex::parse_prefs,
                              [this, done](Result<model::EncodingInfo> prefs) {
                                  if (!prefs.ok())
                                      return done(std::move(prefs));
                                  auto shared = std::make_shared<Result<model::EncodingInfo>>(std::move(prefs));
                                  call<bool>("GET", plex::info_path(), plex::parse_plex_pass,
                                             [shared, done](Result<bool> pass) {
                                                 if (pass.ok())
                                                     shared->value.plex_pass = pass.value;
                                                 done(std::move(*shared));
                                             });
                              });
}

void Plex::tasks(std::function<void(Result<std::vector<model::Task>>)> done)
{
    using Tasks = Result<std::vector<model::Task>>;
    call<std::vector<model::Task>>(
        "GET", plex::butler_path(), plex::parse_butler, [this, done](Tasks butler) {
            if (!butler.ok())
                return done(std::move(butler));
            auto shared = std::make_shared<Tasks>(std::move(butler));
            call<std::vector<model::Task>>("GET", plex::activities_path(), plex::parse_activities,
                                           [shared, done](Tasks running) {
                                               /* what runs now first (a scan's progress), then the Butler's */
                                               if (running.ok())
                                                   shared->value.insert(shared->value.begin(), running.value.begin(),
                                                                        running.value.end());
                                               done(std::move(*shared));
                                           });
        });
}

void Plex::recent(std::function<void(Result<std::vector<model::RecentItem>>)> done)
{
    using Recent = Result<std::vector<model::RecentItem>>;
    call<std::vector<model::JellyfinLibrary>>(
        "GET", plex::sections_path(), plex::parse_sections,
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
                    "GET", plex::recent_path(id, episodes, episodes ? 60 : 20), plex::parse_recent,
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

void Plex::refresh_library(std::function<void(Result<bool>)> done)
{
    call<std::vector<model::JellyfinLibrary>>(
        "GET", plex::sections_path(), plex::parse_sections,
        [this, done](Result<std::vector<model::JellyfinLibrary>> libs) {
            Result<bool> out;
            out.error = libs.error, out.http_status = libs.http_status;
            if (!libs.ok() || libs.value.empty())
                return done(std::move(out));
            auto g = std::make_shared<Gather<bool>>();
            g->left = (int)libs.value.size();
            for (const auto &l : libs.value)   /* every library: Plex scans them one by one */
                call<bool>("POST", plex::section_refresh_path(l.id), accept_any,
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

void Plex::users(std::function<void(Result<std::vector<model::JellyfinUser>>)> done)
{
    using Users = Result<std::vector<model::JellyfinUser>>;
    call<std::vector<model::JellyfinUser>>(
        "GET", plex::accounts_path(), plex::parse_accounts, [this, done](Users users) {
            if (!users.ok() || users.value.empty())
                return done(std::move(users));
            /* Plex dates no account: its latest playback, from the last six months' history. */
            auto shared = std::make_shared<Users>(std::move(users));
            call<std::vector<model::ActivityEntry>>(
                "GET", plex::history_path(std::time(nullptr) - 183 * 86400, 1000), plex::parse_history,
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

} // namespace

std::unique_ptr<MediaServer> make_jellyfin(Services &s) { return std::make_unique<Jellyfin>(s); }
std::unique_ptr<MediaServer> make_emby(Services &s) { return std::make_unique<Emby>(s); }
std::unique_ptr<MediaServer> make_plex(Services &s) { return std::make_unique<Plex>(s); }

} // namespace api
