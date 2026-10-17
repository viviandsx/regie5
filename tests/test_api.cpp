/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The parsers on fixtures/ (real answers, anonymised), the request paths, the
 * errors, the mock transport and the pool.
 */
#include "check.h"

#include "api/arr.h"
#include "api/http.h"
#include "api/http_wire.h"
#include "api/jellyfin.h"
#include "api/json.h"
#include "api/mock_transport.h"
#include "api/seerr.h"
#include "core/config_web.h"
#include "api/services.h"
#include "core/diagnosis.h"
#include "core/jelly5.h"
#include "core/schedule.h"
#include "api/glances.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>

using check::fixture;

/* ---- Sonarr / Radarr ---- */

TEST(arr_status_and_paths)
{
    std::string v;
    CHECK(arr::parse_status(fixture("sonarr/system_status.json"), &v));
    CHECK(!v.empty() && v[0] == '4');
    CHECK(arr::parse_status(fixture("radarr/system_status.json"), &v));
    CHECK(!arr::parse_status("[]", &v));
    CHECK_EQ(arr::calendar_path(config::Sonarr, 0, 86400),
             std::string("/api/v3/calendar?start=1970-01-01T00:00:00Z&end=1970-01-02T00:00:00Z&unmonitored=false"
                         "&includeSeries=true"));
    CHECK(arr::queue_path(config::Radarr).find("includeMovie=true") != std::string::npos);
    CHECK_EQ(arr::mediacover_path(66, "poster-500.jpg"), std::string("/api/v3/mediacover/66/poster-500.jpg"));
}

TEST(arr_health_withholds_client_and_indexer_names)
{
    std::vector<model::Health> h;
    CHECK(arr::parse_health(fixture("sonarr/health.json"), config::Sonarr, &h));
    CHECK_EQ(h.size(), (size_t)1);
    CHECK(h[0].kind == model::Health::ClientRemovesCompleted);
    CHECK(h[0].message.empty());   /* a known check: the interface words it */
    CHECK(h[0].level == model::Health::Warning);

    CHECK(arr::parse_health(R"([
        {"source": "RootFolderCheck", "type": "error", "message": "Missing root folder: /tv"},
        {"source": "IndexerStatusCheck", "type": "warning", "message": "Indexers unavailable due to failures: SomeTracker"},
        {"source": "UpdateCheck", "type": "ok", "message": ""},
        {"source": "BrandNewCheck", "type": "warning", "message": "Something new needs a look"},
        {"source": "OtherNewCheck", "type": "error", "message": "Download client MyBox cannot reach /downloads"},
        {"source": "PackageGlobalMessageCheck", "type": "notice", "message": "Version 5 is coming"}
    ])",
                            config::Radarr, &h));
    CHECK_EQ(h.size(), (size_t)5);
    CHECK(h[0].kind == model::Health::RootFolderMissing);
    CHECK(h[0].message.empty());   /* its path is never kept */
    CHECK(h[0].level == model::Health::Error);
    CHECK(h[1].kind == model::Health::IndexerSomeDown);
    CHECK(h[1].message.empty());
    CHECK(h[2].kind == model::Health::Unknown);
    CHECK_EQ(h[2].message, std::string("Something new needs a look"));   /* unknown and clean: kept */
    CHECK(h[3].kind == model::Health::Unknown);
    CHECK(h[3].message.empty());   /* unknown but it names a client and a path: withheld */
    CHECK(h[4].kind == model::Health::Announcement);
    CHECK(h[4].level == model::Health::Notice);
}

TEST(arr_health_kind_follows_the_level)
{
    using H = model::Health;
    CHECK(arr::health_kind("DownloadClientStatusCheck", H::Error) == H::ClientsAllFailing);
    CHECK(arr::health_kind("DownloadClientStatusCheck", H::Warning) == H::ClientSomeFailing);
    CHECK(arr::health_kind("DownloadClientCheck", H::Error) == H::ClientUnreachable);
    CHECK(arr::health_kind("DownloadClientCheck", H::Warning) == H::ClientNone);
    CHECK(arr::health_kind("IndexerLongTermStatusCheck", H::Error) == H::IndexersAllDownLong);
    CHECK(arr::health_kind("IndexerLongTermStatusCheck", H::Warning) == H::IndexerSomeDownLong);
    CHECK(arr::health_kind("IndexerRssCheck", H::Error) == H::IndexersNoRss);
    CHECK(arr::health_kind("UpdateCheck", H::Warning) == H::UpdateAvailable);
    CHECK(arr::health_kind("UpdateCheck", H::Error) == H::UpdateBlocked);
    CHECK(arr::health_kind("RemotePathMappingCheck", H::Error) == H::RemotePath);
    CHECK(arr::health_kind("NoSuchCheck", H::Error) == H::Unknown);
}

TEST(arr_shareable_text)
{
    CHECK(arr::shareable("Something new needs a look"));
    CHECK(arr::shareable("Version 5.0 is coming"));
    CHECK(!arr::shareable(""));
    CHECK(!arr::shareable("Download client qBit-VPN is set to remove completed downloads"));
    CHECK(!arr::shareable("Indexers unavailable: MyTracker"));
    CHECK(!arr::shareable("Le client de téléchargement est configuré"));
    CHECK(!arr::shareable("Missing root folder: /data/tv"));
    CHECK(!arr::shareable("Cannot write to C:\\Media"));
    CHECK(!arr::shareable("Proxy at 192.168.1.10 failed"));
    CHECK(!arr::shareable("See https://example.org"));
    CHECK(!arr::shareable("SABnzbd is slow"));
}

TEST(arr_diskspace)
{
    std::vector<model::DiskSpace> d;
    CHECK(arr::parse_diskspace(fixture("radarr/diskspace.json"), config::Radarr, &d));
    CHECK(!d.empty());
    for (const auto &s : d)
        CHECK(s.total > 0 && s.free >= 0 && s.free <= s.total);
}

TEST(sonarr_calendar)
{
    std::vector<model::CalendarItem> c;
    CHECK(arr::parse_sonarr_calendar(fixture("sonarr/calendar.json"), &c));
    CHECK_EQ(c.size(), (size_t)60);   /* three weeks, for the guide */
    const model::CalendarItem *first = nullptr;   /* a real one (the others are synthetic) */
    for (const auto &e : c)
        if (e.title == "Yuki's Lighthouse II" && e.season == 1 && e.episode == 1)
            first = &e;
    CHECK(first != nullptr);
    if (first)
        CHECK(first->media_id > 0);
    for (size_t i = 1; i < c.size(); i++)
        CHECK(c[i - 1].when <= c[i].when);   /* in time order */
}

TEST(guide_days_and_states)
{
    /* A Monday at local midnight, and the week's days from it. */
    struct tm tm = {};
    tm.tm_year = 2026 - 1900, tm.tm_mon = 9, tm.tm_mday = 5, tm.tm_isdst = -1;   /* Monday 5 October 2026 */
    const time_t monday = mktime(&tm);
    CHECK_EQ(schedule::column_of(monday, monday), 0);
    CHECK_EQ(schedule::column_of(monday + 6 * 86400 + 23 * 3600, monday), 6);   /* Sunday 23:00 */
    CHECK_EQ(schedule::column_of(monday - 1, monday), -1);
    CHECK_EQ(schedule::column_of(monday + 7 * 86400 + 3600, monday), -1);

    model::CalendarItem c;
    c.when = monday;
    CHECK(schedule::state_of(c, monday + 3600) == schedule::Missing);
    CHECK(schedule::state_of(c, monday - 3600) == schedule::Upcoming);
    c.has_file = true;
    CHECK(schedule::state_of(c, monday + 3600) == schedule::Available);
    c.has_file = false;
    c.from = config::Radarr, c.release = model::CalendarItem::Cinema;
    CHECK(schedule::state_of(c, monday + 3600) == schedule::InCinemas);   /* gone by in cinemas: never "missing" */
    CHECK(schedule::state_of(c, monday - 3600) == schedule::Upcoming);
}

TEST(radarr_calendar_keeps_dates_in_window)
{
    std::vector<model::CalendarItem> c;
    const time_t start = json::parse_time("2026-10-10T00:00:00Z"), end = json::parse_time("2026-10-25T00:00:00Z");
    CHECK(arr::parse_radarr_calendar(fixture("radarr/calendar.json"), start, end, &c));
    /* Iris and the Quiet Frontier (physical 23rd), Camille's Satellite (physical 20th), Second Valley (cinema 21st). */
    CHECK_EQ(c.size(), (size_t)3);
    for (const auto &it : c)
        CHECK(it.when >= start && it.when <= end);
    CHECK(c[0].release == model::CalendarItem::Physical);
    CHECK(c[1].release == model::CalendarItem::Cinema);
}

TEST(sonarr_queue)
{
    std::vector<model::QueueItem> q;
    int total = 0;
    const time_t now = json::parse_time("2026-10-11T11:46:45Z");
    CHECK(arr::parse_queue(fixture("sonarr/queue.json"), config::Sonarr, now, &q, &total));
    CHECK_EQ(q.size(), (size_t)3);
    CHECK_EQ(total, 3);
    CHECK(q[0].status == model::QueueItem::Downloading);
    CHECK(q[0].progress > 0.61 && q[0].progress < 0.63);   /* 1 - sizeleft/size */
    CHECK_EQ((long long)q[0].time_left, 1260LL);             /* from estimatedCompletionTime */
    CHECK_EQ(q[0].quality, std::string("WEBDL-1080p"));
    CHECK(!q[0].title.empty());
    CHECK(q[0].season > 0 && q[0].episode > 0);
    CHECK(q[1].status == model::QueueItem::Queued);
    CHECK(q[2].status == model::QueueItem::Warning);         /* importBlocked */

    /* An estimate already past is stale: the timespan is used instead. */
    CHECK(arr::parse_queue(fixture("sonarr/queue.json"), config::Sonarr, now + 86400, &q));
    CHECK_EQ((long long)q[0].time_left, 1260LL);
}

TEST(radarr_queue)
{
    std::vector<model::QueueItem> q;
    CHECK(arr::parse_queue(fixture("radarr/queue.json"), config::Radarr, 0, &q));
    CHECK_EQ(q.size(), (size_t)3);
    CHECK(q[0].status == model::QueueItem::Downloading);
    CHECK(q[1].status == model::QueueItem::Importing);
    CHECK(q[2].status == model::QueueItem::Error);
    CHECK_EQ(q[0].title, std::string("Iris and the Quiet Frontier"));
    CHECK(q[0].media_id > 0);
}

/* The rule that keeps Regie5 listable: release names, trackers, download
 * clients and paths never reach the model, whatever the server sends. */
TEST(queue_never_keeps_release_names)
{
    const char *body = R"({"totalRecords": 1, "records": [{
        "id": 1, "seriesId": 2, "seasonNumber": 1,
        "series": {"title": "A Show"}, "episode": {"seasonNumber": 1, "episodeNumber": 2, "title": "Pilot"},
        "title": "LEAK-release-title", "downloadClient": "LEAK-client", "indexer": "LEAK-indexer",
        "outputPath": "/downloads/LEAK-path", "downloadId": "LEAK-id", "errorMessage": "LEAK-error",
        "statusMessages": [{"title": "LEAK-status-title", "messages": ["LEAK-message"]}],
        "quality": {"quality": {"name": "HDTV-720p"}}, "size": 100, "sizeleft": 25,
        "status": "downloading", "trackedDownloadStatus": "ok", "trackedDownloadState": "downloading",
        "timeleft": "00:01:00"
    }]})";
    std::vector<model::QueueItem> q;
    CHECK(arr::parse_queue(body, config::Sonarr, 0, &q));
    CHECK_EQ(q.size(), (size_t)1);
    const model::QueueItem &it = q[0];
    for (const std::string *s : {&it.title, &it.episode_title, &it.quality})
        CHECK(s->find("LEAK") == std::string::npos);
    CHECK_EQ(it.title, std::string("A Show"));
    CHECK_EQ(it.episode_title, std::string("Pilot"));
    CHECK_EQ((long long)it.time_left, 60LL);
    CHECK(it.progress > 0.74 && it.progress < 0.76);
}

/* ---- Seerr ---- */

TEST(seerr_paths)
{
    CHECK_EQ(seerr::approve_path(42), std::string("/api/v1/request/42/approve"));
    CHECK_EQ(seerr::decline_path(42), std::string("/api/v1/request/42/decline"));
    CHECK(seerr::requests_path(seerr::Filter::Pending).find("filter=pending") != std::string::npos);
    CHECK_EQ(seerr::image_path("/abc.jpg", "w342"), std::string("/imageproxy/tmdb/t/p/w342/abc.jpg"));
    CHECK_EQ(seerr::image_path("", "w342"), std::string());
    CHECK_EQ(seerr::detail_path(true, 5, "fr"), std::string("/api/v1/tv/5?language=fr"));
}

TEST(seerr_requests)
{
    model::RequestPage p;
    CHECK(seerr::parse_requests(fixture("seerr/request_pending.json"), &p));
    CHECK_EQ(p.requests.size(), (size_t)5);
    for (const auto &r : p.requests) {
        CHECK(r.status == model::Request::Pending);
        CHECK(r.tmdb_id > 0);
        CHECK(!r.requested_by.empty());
        CHECK(r.created > 0);
    }
    CHECK(p.requests[4].tv);
    CHECK_EQ(p.requests[4].seasons.size(), (size_t)1);

    CHECK(seerr::parse_requests(fixture("seerr/request_all.json"), &p));
    CHECK(p.total > (int)p.requests.size());   /* more pages */
    std::string v;
    CHECK(seerr::parse_about(fixture("seerr/settings_about.json"), &v));
}

TEST(seerr_issues)
{
    CHECK_EQ(seerr::issue_resolve_path(7), std::string("/api/v1/issue/7/resolved"));
    CHECK_EQ(seerr::issue_comment_path(7), std::string("/api/v1/issue/7/comment"));
    CHECK(seerr::issues_path(seerr::IssueFilter::Open).find("filter=open") != std::string::npos);
    /* The message as JSON, quotes and accents included. */
    CHECK_EQ(seerr::comment_body("C'est \"réglé\""), std::string("{\"message\":\"C'est \\\"réglé\\\"\"}"));

    model::IssuePage p;
    CHECK(seerr::parse_issues(fixture("seerr/issue_open.json"), &p));
    CHECK_EQ(p.issues.size(), (size_t)4);
    CHECK_EQ(p.total, 4);
    const model::Issue &sub = p.issues[0];   /* the last changed first */
    CHECK_EQ(sub.id, 41);
    CHECK(sub.type == model::Issue::Subtitles);
    CHECK(sub.tv && !sub.resolved);
    CHECK_EQ(sub.season, 1);
    CHECK_EQ(sub.episode, 3);
    CHECK_EQ(sub.reported_by, std::string("Alice"));
    CHECK_EQ(sub.comments.size(), (size_t)1);
    CHECK(sub.comments[0].author.empty());   /* the list has no comment authors */

    model::Issue one;
    CHECK(seerr::parse_issue(fixture("seerr/issue_37.json"), &one));
    CHECK(one.resolved);
    CHECK_EQ(one.modified_by, std::string("Admin"));
    CHECK_EQ(one.comments.size(), (size_t)3);
    CHECK_EQ(one.comments[0].author, std::string("Emma"));   /* the report first */
    CHECK_EQ(one.comments[1].author, std::string("Admin"));
    CHECK(one.comments[0].created < one.comments[2].created);
    CHECK(!seerr::parse_issue("[]", &one));

    CHECK_EQ(api::MockTransport::file_for("http://seerr.mock/api/v1/issue?take=50&skip=0&sort=modified&filter=resolved"),
             std::string("seerr/issue_resolved.json"));
    CHECK_EQ(api::MockTransport::file_for("http://seerr.mock/api/v1/issue/37"), std::string("seerr/issue_37.json"));
}

TEST(jellyfin_libraries)
{
    std::vector<model::JellyfinLibrary> libs;
    CHECK(jellyfin::parse_libraries(fixture("jellyfin/library_virtualfolders.json"), &libs));
    CHECK_EQ(libs.size(), (size_t)3);
    CHECK(libs[0].kind == model::JellyfinLibrary::Movies);
    CHECK(libs[1].kind == model::JellyfinLibrary::Shows);
    CHECK(libs[2].kind == model::JellyfinLibrary::Other);
    CHECK_EQ(libs[0].folders.size(), (size_t)1);
    CHECK_EQ(libs[0].folders[0], std::string("movies"));   /* the last name alone */
    CHECK(jellyfin::library_items_path("ab").find("ParentId=ab") != std::string::npos);
    CHECK(jellyfin::library_count_path("ab", "Episode").find("Limit=0") != std::string::npos);
    CHECK(jellyfin::library_count_path("ab", "").find("IsFolder=false") != std::string::npos);
    CHECK(jellyfin::parse_libraries(R"([{"Name": "C", "CollectionType": "boxsets", "ItemId": "x", "Locations": []}])", &libs));
    CHECK(libs[0].kind == model::JellyfinLibrary::Collections);

    int n = -1;
    CHECK(jellyfin::parse_count(R"({"Items": [], "TotalRecordCount": 814})", &n));
    CHECK_EQ(n, 814);
    CHECK(!jellyfin::parse_count("[]", &n));
    CHECK_EQ(api::MockTransport::file_for("http://jellyfin.mock/Items?ParentId=ab&Recursive=true&IncludeItemTypes=Episode&Limit=0"),
             std::string("jellyfin/items_parent_ab_episode.json"));
}

TEST(seerr_details)
{
    model::MediaDetail d;
    CHECK(seerr::parse_detail(fixture("seerr/movie_9184411.json"), false, &d));
    CHECK(!d.title.empty());
    CHECK(d.year >= 2020);
    CHECK(!d.poster_path.empty() && d.poster_path[0] == '/');
    CHECK(seerr::parse_detail(fixture("seerr/tv_9394299.json"), true, &d));
    CHECK_EQ(d.title, std::string("Iris's Voyage"));
}

namespace {

/* Runs the pool until every request is done (or 5 s). */
void drain(http::Pool &pool)
{
    for (int i = 0; i < 500 && (pool.busy() > 0 || pool.pump() > 0); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.pump();
}

} // namespace

TEST(seerr_discover_and_search)
{
    CHECK_EQ(seerr::discover_path(seerr::Shelf::Trending, "fr"), std::string("/api/v1/discover/trending?page=1&language=fr"));
    CHECK_EQ(seerr::discover_path(seerr::Shelf::UpcomingTv, "en"),
             std::string("/api/v1/discover/tv/upcoming?page=1&language=en"));
    /* Spaces as %20, accents and '&' escaped: the query never breaks the URL. */
    CHECK_EQ(seerr::search_path("l'été & co", "fr"),
             std::string("/api/v1/search?query=l%27%C3%A9t%C3%A9%20%26%20co&page=1&language=fr"));

    std::vector<model::SeerrTitle> titles;
    CHECK(seerr::parse_titles(fixture("seerr/search.json"), &titles));
    bool person = false, film = false, series = false;
    for (const auto &t : titles) {
        person |= t.title == "A Made-up Person";
        film |= !t.tv;
        series |= t.tv;
        CHECK(t.tmdb_id > 0 && !t.title.empty());
    }
    CHECK(!person && film && series);   /* people are left out */
    CHECK(seerr::parse_titles(fixture("seerr/discover_trending.json"), &titles) && !titles.empty());
    CHECK(!titles[0].poster_path.empty() && titles[0].poster_path[0] == '/');
    CHECK(!seerr::parse_titles("{}", &titles));
}

TEST(seerr_title_pages)
{
    model::SeerrDetail d;
    CHECK(seerr::parse_title_detail(fixture("seerr/tv_9394299.json"), true, &d));
    CHECK_EQ(d.title.title, std::string("Iris's Voyage"));
    CHECK(d.title.tv && d.tvdb_id > 0 && d.runtime > 0 && !d.genres.empty());
    CHECK(d.seasons.size() >= 2 && d.seasons[0].number == 0);   /* the specials first, as TMDB lists them */
    const model::SeerrSeason &s1 = d.seasons[1];
    CHECK(s1.number == 1 && s1.episodes > 0 && s1.requested && !s1.requested4k);

    CHECK(seerr::parse_title_detail(fixture("seerr/movie_9068481.json"), false, &d));
    CHECK(!d.title.tv && d.runtime > 0 && d.seasons.empty());
    CHECK(d.title.status == model::SeerrAvailable && !d.title.jellyfin_id.empty());

    model::SeerrPublic pub;
    CHECK(seerr::parse_public(fixture("seerr/settings_public.json"), &pub) && pub.partial_requests && !pub.special_episodes);

    std::vector<model::SeerrServer> servers;
    CHECK(seerr::parse_servers(fixture("seerr/service_radarr.json"), &servers) && servers.size() == 2);
    CHECK(!servers[0].is4k && servers[1].is4k && servers[0].is_default && servers[0].profile_id == 4);
    model::SeerrServer one;
    CHECK(seerr::parse_server(fixture("seerr/service_radarr_1.json"), &one));
    CHECK(one.id == 1 && one.is4k && !one.profiles.empty() && one.folders.size() == 1);
    CHECK_EQ(seerr::server_path(true, 0), std::string("/api/v1/service/sonarr/0"));
}

TEST(seerr_new_request)
{
    seerr::NewRequest r;
    r.tmdb_id = 42;
    CHECK_EQ(seerr::request_body(r), std::string("{\"mediaType\":\"movie\",\"mediaId\":42,\"is4k\":false}"));
    r.tv = true;
    r.tvdb_id = 7;
    r.seasons = {1, 3};
    r.is4k = true;
    r.server_id = 1;
    r.profile_id = 5;
    r.root_folder = "/data/series";
    CHECK_EQ(seerr::request_body(r),
             std::string("{\"mediaType\":\"tv\",\"mediaId\":42,\"tvdbId\":7,\"seasons\":[1,3],\"is4k\":true,"
                         "\"serverId\":1,\"profileId\":5,\"rootFolder\":\"/data/series\"}"));
    model::SeerrCreated c;
    CHECK(seerr::parse_created("{\"id\":12,\"status\":2}", &c) && c.id == 12 && c.status == 2);
    CHECK(seerr::parse_created("", &c) && c.id == 0);   /* --mock's 204 */
    CHECK(seerr::parse_created("{\"message\":\"No seasons available to request\"}", &c) && c.id == 0);   /* 202 */

    /* --mock: the search keeps what matches; the request is never made. */
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 1, nullptr);
    api::Services s(config::mock(), pool, "fr");
    api::Result<std::vector<model::SeerrTitle>> found;
    api::Result<model::SeerrCreated> made;
    s.search("iris's", [&](api::Result<std::vector<model::SeerrTitle>> res) { found = res; });
    s.create_request(r, [&](api::Result<model::SeerrCreated> res) { made = res; });
    drain(pool);
    CHECK(found.ok() && !found.value.empty());
    for (const auto &t : found.value)
        CHECK(t.title.find("Iris's") != std::string::npos);
    CHECK(made.ok() && made.http_status == 204 && made.value.id == 0);
}

TEST(jellyfin_quick_connect)
{
    CHECK_EQ(jellyfin::auth_client("abc123", "PS5", "0.5.0", ""),
             std::string("MediaBrowser Client=\"Regie5\", Device=\"PS5\", DeviceId=\"abc123\", Version=\"0.5.0\""));
    CHECK_EQ(jellyfin::auth_client("a\"b,c", "PS5", "0.5.0", "tok"),
             std::string("MediaBrowser Client=\"Regie5\", Device=\"PS5\", DeviceId=\"abc\", Version=\"0.5.0\", Token=\"tok\""));
    CHECK_EQ(jellyfin::quick_connect_poll_path("6f2c&x=1"), std::string("/QuickConnect/Connect?secret=6f2c1"));
    CHECK_EQ(jellyfin::quick_connect_auth_body("abc"), std::string("{\"Secret\":\"abc\"}"));
    bool on = false;
    CHECK(jellyfin::parse_quick_connect_enabled(fixture("jellyfin/quickconnect_enabled.json"), &on) && on);
    CHECK(!jellyfin::parse_quick_connect_enabled("{}", &on));
    model::QuickConnect qc;
    CHECK(jellyfin::parse_quick_connect(fixture("jellyfin/quickconnect_initiate.json"), &qc));
    CHECK(qc.code.size() == 6 && !qc.secret.empty());
    bool approved = false;
    CHECK(jellyfin::parse_quick_connect_poll(fixture("jellyfin/quickconnect_connect.json"), &approved) && approved);
    model::JellyfinSession s;
    CHECK(jellyfin::parse_session(fixture("jellyfin/users_authenticatewithquickconnect.json"), &s));
    CHECK(!s.token.empty() && s.user == "Alice" && !s.admin);
}

TEST(jellyfin_session_in_config)
{
    /* Written into "jellyfin", the rest kept; read back on, with its account. */
    const std::string text = "{\"jellyfin\": {\"url\": \"http://j:8096\", \"apiKey\": \"\"}, \"cleanup\": {\"ignoreUsers\": [\"Bob\"]}}";
    config::Config before;
    CHECK(config::parse(text, &before) && !before.services[config::Jellyfin].enabled);   /* no key, no session */
    std::string out;
    CHECK(config::with_jellyfin_session(text, "t0k", "Alice", false, &out));
    config::Config c;
    CHECK(config::parse(out, &c));
    const config::ServiceConfig &j = c.services[config::Jellyfin];
    CHECK(j.enabled && j.session() && j.token == "t0k" && j.user == "Alice" && !j.admin);
    CHECK(c.ignore_users.size() == 1);
    CHECK(!config::with_jellyfin_session("{\"sonarr\": {}}", "t", "u", true, &out));   /* nowhere to put it */

    /* The phone page never shows it, and another address drops it. */
    CHECK(config_web::public_view(out).find("t0k") == std::string::npos);
    std::string kept, moved, err;
    CHECK(config_web::apply(out, "{\"services\": {\"jellyfin\": [{\"url\": \"http://j:8096\", \"from\": 0}]}}", &kept, &err));
    CHECK(kept.find("t0k") != std::string::npos);
    CHECK(config_web::apply(out, "{\"services\": {\"jellyfin\": [{\"url\": \"http://other:8096\", \"from\": 0}]}}", &moved, &err));
    CHECK(moved.find("t0k") == std::string::npos);

    /* Signed in so: 401 is the session that ended, 403 an account that is no administrator. */
    for (const char *fail : {"jellyfin:unauthorized", "jellyfin:forbidden"}) {
        api::MockTransport mock(REGIE5_FIXTURES, 0, fail);
        http::Pool pool(mock, 1, nullptr);
        config::Config mc = config::mock();
        mc.services[config::Jellyfin].api_key.clear();
        mc.services[config::Jellyfin].token = "t0k";
        mc.services[config::Jellyfin].admin = false;
        api::Services sv(mc, pool, "en");
        api::Result<std::vector<model::Task>> tasks;
        sv.tasks([&](api::Result<std::vector<model::Task>> r) { tasks = r; });
        drain(pool);
        CHECK(tasks.error == (std::string(fail).find("forbidden") != std::string::npos ? api::Error::NeedsAdmin
                                                                                         : api::Error::Revoked));
    }
}

/* ---- Jellyfin ---- */

TEST(jellyfin_auth_and_paths)
{
    CHECK_EQ(jellyfin::auth_value("k"), std::string("MediaBrowser Token=\"k\""));
    CHECK_EQ(jellyfin::stop_path("abc"), std::string("/Sessions/abc/Playing/Stop"));
    CHECK_EQ(jellyfin::library_refresh_path(), std::string("/Library/Refresh"));
    CHECK(jellyfin::image_path("id1", "Primary", 400).find("/Items/id1/Images/Primary?fillWidth=400") == 0);
    /* Emby ignores fillWidth (the original would come, 4K backdrops and all): maxWidth. */
    CHECK_EQ(jellyfin::emby_image_path(jellyfin::image_path("id1", "Backdrop", 480, "t")),
             std::string("/Items/id1/Images/Backdrop?maxWidth=480&quality=90&format=Jpg&tag=t"));
    CHECK_EQ(jellyfin::emby_image_path("/Items/x/Images/Primary"), std::string("/Items/x/Images/Primary"));
    std::string v;
    CHECK(jellyfin::parse_info(fixture("jellyfin/system_info.json"), &v));
    CHECK(v.rfind("12.", 0) == 0);
}

TEST(jellyfin_sessions)
{
    std::vector<model::Session> s;
    CHECK(jellyfin::parse_sessions(fixture("jellyfin/sessions.json"), &s));
    CHECK_EQ(s.size(), (size_t)3);   /* only those playing something */
    CHECK(s[0].method == model::Session::DirectPlay);
    CHECK(s[0].runtime_ticks > 0);
    CHECK(!s[0].user.empty() && !s[0].device.empty());
    CHECK(s[1].method == model::Session::Transcode);
    CHECK(!s[1].video_direct);
    CHECK_EQ(s[1].transcode_reasons.size(), (size_t)3);
    CHECK_EQ(s[1].hw_accel, std::string("none"));
    CHECK(s[1].completion > 0);
    CHECK_EQ(s[1].streams.size(), (size_t)3);
    CHECK_EQ(s[1].subtitle_index, 2);

    /* Older servers send the reasons as one string. */
    CHECK(jellyfin::parse_sessions(R"([{"Id": "1", "NowPlayingItem": {"Name": "x"},
        "PlayState": {"PlayMethod": "Transcode"},
        "TranscodingInfo": {"TranscodeReasons": "ContainerNotSupported, AudioCodecNotSupported"}}])",
                                   &s));
    CHECK_EQ(s.size(), (size_t)1);
    CHECK_EQ(s[0].transcode_reasons.size(), (size_t)2);
}

TEST(jellyfin_tasks)
{
    std::vector<model::Task> t;
    CHECK(jellyfin::parse_tasks(fixture("jellyfin/scheduledtasks.json"), &t));
    CHECK_EQ(t.size(), (size_t)38);
    CHECK(t[0].state == model::Task::Running);
    CHECK(t[0].progress > 42 && t[0].progress < 43);
    int completed = 0;
    for (const auto &x : t)
        completed += x.last == model::Task::Completed && x.last_end > 0;
    CHECK(completed > 20);
}

TEST(jellyfin_recent_folds_episodes)
{
    std::vector<model::RecentItem> r;
    CHECK(jellyfin::parse_recent(fixture("jellyfin/items.json"), &r));
    /* 10 films and 30 episodes of one series: 11 entries, the series counting 30. */
    CHECK_EQ(r.size(), (size_t)11);
    int episodes = 0;
    for (const auto &x : r) {
        CHECK(!x.title.empty() && !x.image_id.empty());
        if (x.type == "Episode")
            episodes += x.new_episodes;
    }
    CHECK_EQ(episodes, 30);
}

/* ---- errors, mock, pool ---- */

TEST(errors_are_classified)
{
    http::Response r;
    r.fail = http::Response::Connect;
    CHECK(api::classify(r, false) == api::Error::Unreachable);
    r = {};
    r.fail = http::Response::Timeout;
    CHECK(api::classify(r, false) == api::Error::Timeout);
    r = {};
    r.status = 401;
    CHECK(api::classify(r, false) == api::Error::Unauthorized);
    r.status = 403;
    CHECK(api::classify(r, false) == api::Error::Unauthorized);
    r.status = 404;
    CHECK(api::classify(r, false) == api::Error::NotFound);
    r.status = 302;   /* a reverse proxy's login page: shown, never followed */
    CHECK(api::classify(r, false) == api::Error::Redirected);
    r.status = 502;
    CHECK(api::classify(r, false) == api::Error::Http);
    r.status = 200;
    CHECK(api::classify(r, false) == api::Error::BadResponse);
    CHECK(api::classify(r, true) == api::Error::None);
}

TEST(mock_maps_urls_to_fixtures)
{
    using M = api::MockTransport;
    CHECK_EQ(M::file_for("http://sonarr.mock/api/v3/system/status"), std::string("sonarr/system_status.json"));
    CHECK_EQ(M::file_for("http://sonarr.mock/api/v3/queue?page=1&includeSeries=true"),
             std::string("sonarr/queue.json"));
    CHECK_EQ(M::file_for("http://seerr.mock/api/v1/request?take=50&filter=pending"),
             std::string("seerr/request_pending.json"));
    CHECK_EQ(M::file_for("http://seerr.mock/api/v1/movie/9000128?language=fr"), std::string("seerr/movie_9000128.json"));
    CHECK_EQ(M::file_for("http://jellyfin.mock/ScheduledTasks?isHidden=false"),
             std::string("jellyfin/scheduledtasks.json"));
    CHECK_EQ(M::file_for("http://192.168.1.10:8989/api/v3/queue"), std::string());   /* never a real server */
}

TEST(services_on_mock)
{
    api::MockTransport mock(REGIE5_FIXTURES, 0, "radarr:unreachable,seerr:unauthorized");
    http::Pool pool(mock, 2, nullptr);
    config::Config cfg = config::mock();
    cfg.services[config::Jellyfin].enabled = false;
    api::Services s(cfg, pool, "fr");

    api::Result<std::string> sonarr, radarr, seerr_, jf;
    s.test(config::Sonarr, [&](api::Result<std::string> r) { sonarr = r; });
    s.test(config::Radarr, [&](api::Result<std::string> r) { radarr = r; });
    s.test(config::Seerr, [&](api::Result<std::string> r) { seerr_ = r; });
    s.test(config::Jellyfin, [&](api::Result<std::string> r) { jf = r; });   /* answered at once */
    CHECK(jf.error == api::Error::Disabled);
    drain(pool);
    CHECK(sonarr.ok());
    CHECK(!sonarr.value.empty());
    CHECK(radarr.error == api::Error::Unreachable);
    CHECK(seerr_.error == api::Error::Unauthorized);

    api::Result<std::vector<model::QueueItem>> q;
    s.queue(config::Sonarr, [&](api::Result<std::vector<model::QueueItem>> r) { q = r; });
    drain(pool);
    CHECK(q.ok());
    CHECK_EQ(q.value.size(), (size_t)3);
}

TEST(emby_on_its_fixtures)
{
    /* fixtures/emby/: a test Emby 4.10 (scripts/make-emby-fixtures.py), through
     * the same calls as Jellyfin's. */
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 2, nullptr);
    const config::Config cfg = config::mock("", false, config::MediaServer::Emby);
    CHECK(cfg.emby());
    api::Services s(cfg, pool, "fr");
    CHECK(s.auth_header(config::Jellyfin).find("X-Emby-Token") != std::string::npos);
    CHECK(s.auth_header(config::Jellyfin).find("MediaBrowser") == std::string::npos);

    api::Result<std::string> version;
    s.test(config::Jellyfin, [&](api::Result<std::string> r) { version = r; });
    api::Result<std::vector<model::Session>> sessions;
    s.sessions([&](api::Result<std::vector<model::Session>> r) { sessions = r; });
    api::Result<std::vector<model::ActivityEntry>> log;
    s.activity(0, [&](api::Result<std::vector<model::ActivityEntry>> r) { log = r; });
    api::Result<model::JellyfinUser> numbered;
    s.user("2", [&](api::Result<model::JellyfinUser> r) { numbered = r; });
    api::Result<std::vector<model::UserItem>> lea;
    s.user_items("77d617ebc7164408aa2a0ad5f48bb14a", [&](api::Result<std::vector<model::UserItem>> r) { lea = r; });
    api::Result<std::vector<model::RecentItem>> recent;
    s.recent([&](api::Result<std::vector<model::RecentItem>> r) { recent = r; });
    api::Result<model::EncodingInfo> enc;
    s.encoding([&](api::Result<model::EncodingInfo> r) { enc = r; });
    api::Result<std::vector<model::JellyfinLibrary>> libs;
    s.libraries([&](api::Result<std::vector<model::JellyfinLibrary>> r) { libs = r; });
    api::Result<std::vector<model::Task>> tasks;
    s.tasks([&](api::Result<std::vector<model::Task>> r) { tasks = r; });
    drain(pool);
    drain(pool);   /* user_items asks twice on Emby: the items, then the watched episodes */

    CHECK(version.ok());
    CHECK_EQ(version.value, std::string("4.10.1.0"));
    CHECK(sessions.ok());
    CHECK_EQ(sessions.value.size(), (size_t)1);
    if (!sessions.value.empty()) {
        const model::Session &p = sessions.value[0];
        CHECK_EQ(p.user, std::string("Lea"));
        CHECK_EQ(p.item_name, std::string("Tears of Steel"));
        CHECK(p.method == model::Session::Transcode);
        CHECK(!p.video_direct);
        CHECK_EQ(p.video_codec, std::string("h264"));
        CHECK_EQ(p.hw_accel, std::string("none"));   /* every step of Emby's pipeline in software */
        CHECK(p.completion > 0);
    }
    CHECK(log.ok());
    bool played = false, signed_in = false;
    for (const auto &e : log.value) {
        played |= e.type == "VideoPlayback" && !e.item_id.empty();
        signed_in |= e.type == "AuthenticationSucceeded";
        CHECK(e.type.find('.') == std::string::npos || e.type == "user.passwordchanged" || e.type == "user.policyupdated");
    }
    CHECK(played && signed_in);
    CHECK(numbered.ok());
    CHECK_EQ(numbered.value.name, std::string("Lea"));
    CHECK(lea.ok());
    CHECK_EQ(lea.value.size(), (size_t)5);
    CHECK(std::count_if(lea.value.begin(), lea.value.end(), [](const model::UserItem &i) { return i.series; }) == 1);
    for (const auto &i : lea.value) {
        if (i.series) {   /* dated from its latest watched episode (Emby dates no series) */
            CHECK(i.unplayed == 0);
            CHECK(i.last_played > 0);
        }
        if (i.tmdb_id == 45745)   /* Sintel, watched in March 2025 */
            CHECK(i.played && i.last_played > 0 && i.last_played < 1745000000);
    }
    CHECK(recent.ok());
    CHECK_EQ(recent.value.size(), (size_t)5);   /* four films, and the series' three episodes folded */
    CHECK(enc.ok());
    CHECK(enc.value.hw_accel.empty());   /* on (mode 1): which method, Emby does not say */
    CHECK(libs.ok());
    CHECK_EQ(libs.value.size(), (size_t)2);
    CHECK(tasks.ok() && !tasks.value.empty());

    /* Only Emby is asked for its users' last played dates. */
    CHECK(jellyfin::user_items_path("u", true).find("UserDataLastPlayedDate") != std::string::npos);
    CHECK(jellyfin::user_items_path("u").find("UserDataLastPlayedDate") == std::string::npos);
    CHECK_EQ(api::MockTransport::file_for("http://emby.mock" + jellyfin::played_episodes_path("u7")),
             std::string("emby/items_user_u7_played.json"));
    model::EncodingInfo off;
    CHECK(jellyfin::parse_encoding(R"({"HardwareAccelerationMode": 0})", &off));
    CHECK_EQ(off.hw_accel, std::string("none"));
}

TEST(services_behind_a_reverse_proxy)
{
    /* Sub-paths, with or without a final slash, one or two levels deep (tested against nginx). */
    config::Config cfg;
    CHECK(config::parse(R"({
        "sonarr": {"url": "https://proxy.example/sonarr", "apiKey": "a"},
        "radarr": {"url": "https://proxy.example/radarr/", "apiKey": "b"},
        "seerr": {"url": "https://proxy.example/requests", "apiKey": "c"},
        "jellyfin": {"url": "https://proxy.example:8443/media/jellyfin/", "apiKey": "d"}
    })", &cfg));
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 1, nullptr);
    api::Services s(cfg, pool, "en");
    CHECK_EQ(s.url(config::Sonarr, "/api/v3/calendar"), std::string("https://proxy.example/sonarr/api/v3/calendar"));
    CHECK_EQ(s.url(config::Radarr, arr::mediacover_path(7, "poster-250.jpg")),
             std::string("https://proxy.example/radarr/api/v3/mediacover/7/poster-250.jpg"));
    CHECK_EQ(s.url(config::Seerr, seerr::image_path("/abc.jpg", "w300")),
             std::string("https://proxy.example/requests/imageproxy/tmdb/t/p/w300/abc.jpg"));
    CHECK_EQ(s.url(config::Jellyfin, "/System/Info"), std::string("https://proxy.example:8443/media/jellyfin/System/Info"));
}

TEST(mock_actions_do_nothing_but_answer)
{
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 1, nullptr);
    api::Services s(config::mock(), pool, "en");
    api::Result<bool> approve, stop, scan;
    s.approve(517, [&](api::Result<bool> r) { approve = r; });
    s.stop_session("abc", [&](api::Result<bool> r) { stop = r; });
    s.refresh_library([&](api::Result<bool> r) { scan = r; });
    drain(pool);
    CHECK(approve.ok() && approve.value);
    CHECK(stop.ok() && stop.value);
    CHECK(scan.ok() && scan.value);
}

/* ---- libraries ---- */

TEST(fixtures_hold_no_forbidden_field)
{
    /* Release names and groups, scene names, file names, clients, indexers: never kept. */
    static const char *const kForbidden[] = {"\"releaseGroups\"", "\"releaseGroup\"", "\"sceneName\"",
                                             "\"relativePath\"", "\"originalFilePath\"", "\"downloadClient\"",
                                             "\"indexer\"", "\"outputPath\"", "\"downloadId\"",
                                             "\"statusMessages\"", "\"sourceTitle\"", "\"ShortOverview\""};
    for (const char *root : {REGIE5_FIXTURES, REGIE5_DEMO}) {
        int files = 0;
        for (const auto &e : std::filesystem::recursive_directory_iterator(root)) {
            if (!e.is_regular_file() || e.path().extension() != ".json")
                continue;
            files++;
            const std::string text = check::fixture(std::filesystem::relative(e.path(), root).string(), root);
            for (const char *k : kForbidden)
                if (text.find(k) != std::string::npos)
                    check::fail(__FILE__, __LINE__, e.path().string() + " holds " + k);
        }
        CHECK(files > 100);
    }
}

TEST(mock_pictures)
{
    using M = api::MockTransport;
    CHECK_EQ(M::picture_for("http://radarr.mock/api/v3/mediacover/12/poster-500.jpg"),
             std::string("radarr/mediacover/12/poster"));
    CHECK_EQ(M::picture_for("http://radarr.mock/radarr4k/api/v3/mediacover/3/fanart-360.jpg"),
             std::string("radarr/mediacover/3/fanart"));
    CHECK_EQ(M::picture_for("http://jellyfin.mock/Items/ab12/Images/Primary?fillWidth=400&tag=x"),
             std::string("jellyfin/items/ab12/primary"));
    CHECK_EQ(M::picture_for("http://seerr.mock/imageproxy/tmdb/t/p/w185/r5abc.jpg"), std::string("seerr/r5abc.jpg"));
    CHECK_EQ(M::picture_for("http://sonarr.mock/api/v3/series"), std::string());

    /* fixtures/ has no pictures: every card is a generated one. */
    M fixtures(REGIE5_FIXTURES, 0, "");
    http::Request req;
    req.url = "http://radarr.mock/api/v3/mediacover/1/poster-500.jpg";
    CHECK_EQ(fixtures.perform(req, nullptr).status, 404);

    /* demo/: every picture images.json names is there, served as JPEG. */
    M demo(REGIE5_DEMO, 0, "");
    const std::string index = check::fixture("images.json", REGIE5_DEMO);
    int served = 0;
    for (size_t p = index.find("\"radarr/mediacover/"); p != std::string::npos;
         p = index.find("\"radarr/mediacover/", p + 1)) {
        const std::string key = index.substr(p + 1, index.find('"', p + 1) - p - 1);
        const std::string id = key.substr(18, key.find('/', 18) - 18), kind = key.substr(key.rfind('/') + 1);
        req.url = "http://radarr.mock/api/v3/mediacover/" + id + "/" + kind + "-500.jpg";
        http::Response r = demo.perform(req, nullptr);
        CHECK_EQ(r.status, 200);
        CHECK_EQ(r.content_type, std::string("image/jpeg"));
        CHECK(r.body.size() > 1000 && (unsigned char)r.body[0] == 0xFF && (unsigned char)r.body[1] == 0xD8);
        served++;
    }
    CHECK(served >= 10);
    req.url = "http://radarr.mock/api/v3/mediacover/999999/poster-500.jpg";   /* a made-up title: no picture */
    CHECK_EQ(demo.perform(req, nullptr).status, 404);
}

TEST(library_series)
{
    std::vector<model::Series> s;
    CHECK(arr::parse_series(fixture("sonarr/series.json"), &s));
    CHECK_EQ(s.size(), (size_t)53);
    int ended = 0, missing = 0;
    for (const auto &x : s) {
        CHECK(x.id > 0 && !x.title.empty() && !x.sort_title.empty());
        CHECK(x.files <= x.total);
        ended += x.status == model::Series::Ended;
        missing += x.missing() > 0;
        for (size_t i = 1; i < x.seasons.size(); i++)   /* in order, specials last */
            CHECK(x.seasons[i - 1].number != 0 && (x.seasons[i].number == 0 || x.seasons[i - 1].number < x.seasons[i].number));
    }
    CHECK_EQ(ended, 30);
    CHECK_EQ(missing, 2);
}

TEST(library_movies)
{
    std::vector<model::Movie> m;
    CHECK(arr::parse_movies(fixture("radarr/movie.json"), &m));
    CHECK_EQ(m.size(), (size_t)436);
    int with_file = 0;
    for (const auto &x : m) {
        CHECK(!x.title.empty());
        if (x.has_file) {
            with_file++;
            CHECK(!x.file_quality.empty());
        }
    }
    CHECK_EQ(with_file, 414);
    model::Movie t;
    t.in_cinemas = 100, t.digital = 300, t.physical = 200;
    CHECK_EQ((long long)t.next_release(150), 200LL);
    CHECK_EQ((long long)t.next_release(400), 0LL);
}

TEST(library_missing_and_files)
{
    std::vector<model::MissingEpisode> eps;
    int total = 0;
    CHECK(arr::parse_missing_episodes(fixture("sonarr/wanted_missing.json"), &eps, &total));
    CHECK_EQ(total, 158);
    CHECK_EQ(eps.size(), (size_t)158);
    CHECK(!eps[0].series_title.empty() && eps[0].aired > 0);
    std::vector<model::Movie> movies;
    CHECK(arr::parse_missing_movies(fixture("radarr/wanted_missing.json"), &movies, &total));
    CHECK_EQ(total, 22);
    for (const auto &x : movies)
        CHECK(!x.has_file);
    std::vector<model::QualityProfile> q;
    CHECK(arr::parse_quality_profiles(fixture("sonarr/qualityprofile.json"), &q));
    CHECK(!q.empty() && !q[0].name.empty());
    model::FileSummary f;
    std::vector<model::Series> s;
    CHECK(arr::parse_series(fixture("sonarr/series.json"), &s));
    CHECK(arr::parse_episode_files(fixture("sonarr/episodefile_" + std::to_string(s[0].id) + ".json"), &f));
    int n = 0;
    for (const auto &p : f.qualities)
        n += p.second;
    CHECK_EQ(n, s[0].files);   /* one quality per file */
    CHECK(api::MockTransport::file_for("http://sonarr.mock/api/v3/episodefile?seriesId=12") ==
          std::string("sonarr/episodefile_12.json"));
}

TEST(mock_empty_keeps_the_shape)
{
    using api::MockTransport;
    std::vector<model::QueueItem> q;
    int total = -1;
    CHECK(arr::parse_queue(MockTransport::emptied("sonarr/queue.json", fixture("sonarr/queue.json")), config::Sonarr,
                           time(nullptr), &q, &total));
    CHECK(q.empty());
    CHECK_EQ(total, 0);
    CHECK_EQ(MockTransport::emptied("jellyfin/sessions.json", fixture("jellyfin/sessions.json")), std::string("[]"));
    /* What the connection test reads is kept. */
    const std::string status = fixture("sonarr/system_status.json");
    CHECK_EQ(MockTransport::emptied("sonarr/system_status.json", status), status);
}

TEST(journal_imports_never_keep_the_release)
{
    /* The history holds the release in sourceTitle (and data, downloadId):
     * none of it reaches the model, which has no field for it. */
    const std::string body = R"([{"eventType": "downloadFolderImported", "date": "2026-10-10T20:00:00Z",
        "sourceTitle": "Some.Show.S01E02.1080p.WEB.h264-GROUP", "downloadId": "ABC", "data": {"droppedPath": "/x"},
        "seriesId": 7, "quality": {"quality": {"name": "WEBDL-1080p"}},
        "series": {"id": 7, "title": "Some Show", "year": 2024},
        "episode": {"seasonNumber": 1, "episodeNumber": 2, "title": "Two"}},
        {"eventType": "grabbed", "date": "2026-10-10T19:00:00Z", "sourceTitle": "Some.Show.S01E02-GROUP"}])";
    std::vector<model::ImportEvent> e;
    CHECK(arr::parse_history_imports(body, config::Sonarr, &e));
    CHECK_EQ(e.size(), (size_t)1);   /* the grab is not an import */
    if (!e.empty()) {
        CHECK_EQ(e[0].title, std::string("Some Show"));
        CHECK_EQ(e[0].quality, std::string("WEBDL-1080p"));
        CHECK_EQ(e[0].episode, 2);
        CHECK(e[0].episode_title.find("GROUP") == std::string::npos);
    }
    std::vector<model::ImportEvent> s, r;
    CHECK(arr::parse_history_imports(fixture("sonarr/history_since.json"), config::Sonarr, &s));
    CHECK(arr::parse_history_imports(fixture("radarr/history_since.json"), config::Radarr, &r));
    CHECK(!s.empty() && !r.empty());
    for (size_t i = 1; i < s.size(); i++)
        CHECK(s[i - 1].when >= s[i].when);   /* newest first */
}

TEST(journal_jellyfin_activity_keeps_no_text)
{
    const std::string body = R"({"Items": [{"Id": 1, "Name": "alice from 192.168.1.20 on Living room TV",
        "ShortOverview": "IP address: 192.168.1.20", "Type": "AuthenticationSucceeded",
        "Date": "2026-10-10T20:00:00Z", "UserId": "u1", "Severity": "Information"}]})";
    std::vector<model::ActivityEntry> a;
    CHECK(jellyfin::parse_activity(body, &a));
    CHECK_EQ(a.size(), (size_t)1);
    if (!a.empty()) {
        CHECK_EQ(a[0].type, std::string("AuthenticationSucceeded"));
        CHECK_EQ(a[0].user_id, std::string("u1"));
    }
    std::vector<model::ActivityEntry> log;
    CHECK(jellyfin::parse_activity(fixture("jellyfin/system_activitylog_entries.json"), &log));
    CHECK(!log.empty());
    std::vector<model::JellyfinUser> users;
    CHECK(jellyfin::parse_users(fixture("jellyfin/users.json"), &users));
    CHECK(!users.empty());
    std::vector<model::JellyfinItem> items;
    CHECK(jellyfin::parse_items(fixture("jellyfin/items_ids.json"), &items));
    CHECK(!items.empty());
}

TEST(diagnosis_of_the_fixture_sessions)
{
    std::vector<model::Session> sessions;
    CHECK(jellyfin::parse_sessions(fixture("jellyfin/sessions.json"), &sessions));
    model::EncodingInfo enc;
    CHECK(jellyfin::parse_encoding(fixture("jellyfin/system_configuration_encoding.json"), &enc));
    CHECK(enc.throttling);
    const model::Session *pgs = nullptr, *bitrate = nullptr, *direct = nullptr;
    for (const auto &s : sessions) {
        if (s.method == model::Session::DirectPlay)
            direct = &s;
        else if (std::find(s.transcode_reasons.begin(), s.transcode_reasons.end(), "SubtitleCodecNotSupported") !=
                 s.transcode_reasons.end())
            pgs = &s;
        else
            bitrate = &s;
    }
    CHECK(pgs && bitrate && direct);
    if (!pgs || !bitrate || !direct)
        return;
    /* Picture subtitles and HDR on the CPU, 9 s ahead: it does not keep up. */
    const diagnosis::Pace p1 = diagnosis::pace(*pgs, -1, enc);
    CHECK(p1.state == diagnosis::Pace::Behind);
    const auto a1 = diagnosis::advice(*pgs, enc, p1);
    CHECK(!a1.empty() && a1[0] == diagnosis::Slow);
    for (diagnosis::Advice want : {diagnosis::SubImage, diagnosis::Hdr, diagnosis::Audio, diagnosis::NoHardware})
        CHECK(std::find(a1.begin(), a1.end(), want) != a1.end());
    CHECK(diagnosis::subtitle(*pgs) && !diagnosis::subtitle(*pgs)->text);
    CHECK(diagnosis::video(*pgs) && diagnosis::video(*pgs)->range == "HDR10");
    /* Over the bitrate limit with VAAPI, minutes ahead: fine. */
    const diagnosis::Pace p2 = diagnosis::pace(*bitrate, -1, enc);
    CHECK(p2.state == diagnosis::Pace::Ahead);
    const auto a2 = diagnosis::advice(*bitrate, enc, p2);
    CHECK_EQ(a2.size(), (size_t)1);
    CHECK(!a2.empty() && a2[0] == diagnosis::Bitrate);
    /* Direct play: nothing to say. */
    CHECK(diagnosis::advice(*direct, enc, diagnosis::pace(*direct, -1, enc)).empty());
    /* A lead that falls fast, under a minute: no longer "ahead". */
    model::Session s = *bitrate;
    s.completion = 100.0 * s.position_ticks / s.runtime_ticks + 100.0 * 45 / (s.runtime_ticks / 1e7);
    CHECK(diagnosis::pace(s, 90, enc).state == diagnosis::Pace::Limit);
}

TEST(glances_quicklook)
{
    model::ServerLoad l;
    CHECK(glances::parse_quicklook(fixture("glances/quicklook.json"), &l));
    CHECK(l.cpu > 0 && l.mem > 0 && l.cores == 6);
    CHECK(!glances::parse_quicklook("{\"status\": \"ok\"}", &l));
}

/* ---- Ouvrir dans Jelly5 ---- */

TEST(jelly5_open_request)
{
    /* What Jelly5 is handed: the same request both ways. */
    const jelly5::OpenRequest req{"srv1", "item2"};
    const std::vector<std::string> args = jelly5::launch_args(req);
    CHECK(args == std::vector<std::string>({"--open-item", "item2", "--server", "srv1"}));
    json::Doc h(jelly5::handoff_json(req, 1760000000));
    CHECK_EQ(json::str(h.root(), "serverId"), std::string("srv1"));
    CHECK_EQ(json::str(h.root(), "itemId"), std::string("item2"));
    CHECK_EQ(json::str(h.root(), "action"), std::string("open"));
    CHECK_EQ(json::integer(h.root(), "createdAt"), 1760000000);
    std::string server;
    CHECK(jellyfin::parse_server_id(fixture("jellyfin/system_info.json"), &server) && server.size() == 32);

    /* Library titles to Jellyfin items, by their ids: every film with a file
     * in the fixtures is found; one with no id never is. */
    CHECK_EQ(api::MockTransport::file_for("http://jellyfin.mock" + jellyfin::index_path()),
             std::string("jellyfin/items_index.json"));
    std::vector<model::UserItem> index;
    CHECK(jellyfin::parse_user_items(fixture("jellyfin/items_index.json"), &index) && !index.empty());
    std::vector<model::Movie> movies;
    CHECK(arr::parse_movies(fixture("radarr/movie.json"), &movies));
    int found = 0;
    for (const auto &m : movies)
        found += !jelly5::item_for(m, index).empty();
    CHECK(found > 100);
    model::Movie nothing;
    nothing.title = "Sans identifiant";
    CHECK(jelly5::item_for(nothing, index).empty());
    std::vector<model::Series> series;
    CHECK(arr::parse_series(fixture("sonarr/series.json"), &series));
    CHECK(std::any_of(series.begin(), series.end(), [&](const model::Series &s) { return !jelly5::item_for(s, index).empty(); }));

    /* A recent card: a film itself; several new episodes, their series. */
    std::vector<model::RecentItem> recent;
    CHECK(jellyfin::parse_recent(fixture("jellyfin/items.json"), &recent));
    for (const auto &r : recent) {
        CHECK(!jelly5::item_for(r).empty());
        if (r.type == "Episode" && r.new_episodes > 1)
            CHECK_EQ(jelly5::item_for(r), r.series_id);
        else
            CHECK_EQ(jelly5::item_for(r), r.id);
    }

    /* A request: only once Jellyfin has it, in the request's quality. */
    model::RequestPage p;
    CHECK(seerr::parse_requests(fixture("seerr/request_all.json"), &p));
    int offered = 0;
    for (const auto &r : p.requests) {
        if (r.id == 516)   /* 4K, approved: the film is there, its 4K copy not yet */
            CHECK(r.is4k && r.media_status == 3 && jelly5::item_for(r).empty());
        if (r.id == 513)   /* still on its way */
            CHECK(jelly5::item_for(r).empty());
        offered += !jelly5::item_for(r).empty();
    }
    CHECK(offered > 10);
}

/* ---- several instances ---- */

TEST(instances_tag_answers_and_calendar_keeps_one_copy)
{
    /* Two Radarr on the mock: each answer carries its instance. */
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 2, nullptr);
    api::Services svc(config::mock({}, true), pool, "fr");
    CHECK_EQ(svc.instances(config::Radarr), 2);
    std::vector<model::Movie> got[2];
    for (int i = 0; i < 2; i++)
        svc.movies([&got, i](api::Result<std::vector<model::Movie>> r) { got[i] = r.value; }, i);
    drain(pool);
    CHECK(!got[0].empty() && got[0].size() == got[1].size());
    CHECK(got[0][0].instance == 0 && got[1][0].instance == 1);

    /* The same release seen by both: kept once, the copy with its file. */
    model::CalendarItem a;
    a.from = config::Radarr, a.title = "The Glass Comet", a.release = model::CalendarItem::Digital, a.when = 1000;
    model::CalendarItem b = a;
    b.instance = 1, b.has_file = true;
    model::CalendarItem other = a;
    other.release = model::CalendarItem::Physical;
    std::vector<model::CalendarItem> items = {a, other, b};
    schedule::drop_duplicates(&items);
    CHECK_EQ(items.size(), (size_t)2);
    CHECK(items[0].instance == 1 && items[0].has_file);
    CHECK(items[1].release == model::CalendarItem::Physical);
}

/* ---- HTTP on the wire (the PS5's transport) ---- */

TEST(http_wire_urls_and_request)
{
    http::wire::Url u;
    CHECK(http::wire::parse_url("http://192.168.1.20:8989/sonarr/api/v3/calendar?start=1", &u));
    CHECK_EQ(u.scheme, std::string("http"));
    CHECK_EQ(u.host, std::string("192.168.1.20"));
    CHECK_EQ(u.port, 8989);
    CHECK_EQ(u.target, std::string("/sonarr/api/v3/calendar?start=1"));
    CHECK(http::wire::parse_url("HTTPS://media.example/radarr", &u));
    CHECK_EQ(u.scheme, std::string("https"));
    CHECK_EQ(u.port, 443);
    CHECK_EQ(u.target, std::string("/radarr"));
    CHECK(http::wire::parse_url("http://[fd00::5]:8096", &u));
    CHECK_EQ(u.host, std::string("fd00::5"));
    CHECK_EQ(u.target, std::string("/"));
    CHECK(!http::wire::parse_url("ftp://host/", &u));
    CHECK(!http::wire::parse_url("http://:80/", &u));
    CHECK(!http::wire::parse_url("http://host:99999/", &u));

    http::Request req;
    req.method = "POST";
    req.url = "http://10.0.0.2:5055/api/v1/request/7/approve";
    req.headers = {"X-Api-Key: k"};
    CHECK(http::wire::parse_url(req.url, &u));
    const std::string t = http::wire::request_text(req, u);
    CHECK(t.rfind("POST /api/v1/request/7/approve HTTP/1.1\r\nHost: 10.0.0.2:5055\r\n", 0) == 0);
    CHECK(t.find("Content-Length: 0\r\n") != std::string::npos);
    CHECK(t.find("X-Api-Key: k\r\n") != std::string::npos);
    CHECK(t.find("Connection: close\r\n") != std::string::npos);
    CHECK(t.size() >= 4 && t.compare(t.size() - 4, 4, "\r\n\r\n") == 0);
}

namespace {
/* The whole answer fed in pieces of every size from 1 up: the reader must
 * not care where the network cuts it. */
void read_in_pieces(const std::string &wire, bool close_at_end, int status, const std::string &body)
{
    for (size_t step = 1; step <= wire.size(); step = step < 8 ? step + 1 : step * 2) {
        http::wire::Reader r;
        http::wire::Reader::State st = http::wire::Reader::More;
        for (size_t i = 0; i < wire.size() && st == http::wire::Reader::More; i += step)
            st = r.feed(wire.data() + i, std::min(step, wire.size() - i));
        if (close_at_end && st == http::wire::Reader::More)
            st = r.finish();
        CHECK(st == http::wire::Reader::Done);
        CHECK_EQ(r.status, status);
        CHECK_EQ(r.body, body);
    }
}
} // namespace

TEST(http_wire_reader)
{
    read_in_pieces("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 7\r\n\r\n[1,2,3]", false,
                   200, "[1,2,3]");
    read_in_pieces("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n[1,2\r\nb;ext=1\r\n,3,4,5,6,7]\r\n"
                   "0\r\nX-Trailer: y\r\n\r\n",
                   false, 200, "[1,2,3,4,5,6,7]");
    read_in_pieces("HTTP/1.0 200 OK\r\n\r\nuntil the end", true, 200, "until the end");
    read_in_pieces("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n", false, 204, "");
    read_in_pieces("HTTP/1.1 401 Unauthorized\r\ncontent-length: 0\r\n\r\n", false, 401, "");

    http::wire::Reader ct;
    const std::string a = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: 2\r\n\r\nab";
    CHECK(ct.feed(a.data(), a.size()) == http::wire::Reader::Done);
    CHECK_EQ(ct.content_type, std::string("image/jpeg"));

    http::wire::Reader cut;   /* closed before the end of a sized body */
    const std::string b = "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc";
    CHECK(cut.feed(b.data(), b.size()) == http::wire::Reader::More);
    CHECK(cut.finish() == http::wire::Reader::Bad);
    CHECK(cut.body.empty());

    http::wire::Reader gz;   /* never asked for, never taken as JSON */
    const std::string c = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 2\r\n\r\nxx";
    CHECK(gz.feed(c.data(), c.size()) == http::wire::Reader::Bad);

    http::wire::Reader junk;
    const std::string d = "SSH-2.0-OpenSSH_9.6\r\n\r\n";
    CHECK(junk.feed(d.data(), d.size()) == http::wire::Reader::Bad);

    http::wire::Reader none;
    CHECK(none.finish() == http::wire::Reader::Bad);
}

TEST(http_wire_date)
{
    int64_t t = 0;
    CHECK(http::wire::parse_http_date("Mon, 12 Oct 2026 12:34:32 GMT", &t));
    CHECK_EQ(t, (int64_t)1791808472);
    CHECK(http::wire::parse_http_date("Thu, 01 Jan 1970 00:00:00 GMT", &t));
    CHECK_EQ(t, (int64_t)0);
    CHECK(!http::wire::parse_http_date("Monday, 12-Oct-26 12:34:32 GMT", &t));
    CHECK(!http::wire::parse_http_date("Mon, 12 Oct 2026 12:34:32 CEST", &t));
    http::wire::Reader r;
    const std::string a = "HTTP/1.1 204 No Content\r\nDate: Mon, 12 Oct 2026 12:34:32 GMT\r\n\r\n";
    CHECK(r.feed(a.data(), a.size()) == http::wire::Reader::Done);
    CHECK_EQ(r.server_date, (int64_t)1791808472);
}
