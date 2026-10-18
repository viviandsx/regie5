/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every parser on hostile answers: each fixture cut short, its numbers made
 * huge or infinite, its strings made numbers, its values null, its objects
 * and lists swapped, and plain junk. Nothing may crash or hang (run it under
 * AddressSanitizer and UBSan too: see docs/ROADMAP.md); a parser may refuse.
 */
#include "check.h"

#include "api/arr.h"
#include "api/glances.h"
#include "api/jellyfin.h"
#include "api/json.h"
#include "api/plex.h"
#include "api/seerr.h"

#include <climits>
#include <cmath>

using check::fixture;

namespace {

struct Parser {
    const char *fixture;
    std::function<bool(const std::string &)> parse;
};

template <typename T> std::function<bool(const std::string &)> into(bool (*f)(const std::string &, T *))
{
    return [f](const std::string &b) {
        T out{};
        return f(b, &out);
    };
}

std::vector<Parser> parsers()
{
    using namespace model;
    const time_t now = 1792000000;
    return {
        {"sonarr/system_status.json", into(arr::parse_status)},
        {"sonarr/health.json", [](const std::string &b) { std::vector<Health> o; return arr::parse_health(b, config::Sonarr, &o); }},
        {"radarr/diskspace.json", [](const std::string &b) { std::vector<DiskSpace> o; return arr::parse_diskspace(b, config::Radarr, &o); }},
        {"sonarr/history_since.json", [](const std::string &b) { std::vector<ImportEvent> o; return arr::parse_history_imports(b, config::Sonarr, &o); }},
        {"sonarr/calendar.json", into(arr::parse_sonarr_calendar)},
        {"radarr/calendar.json", [now](const std::string &b) { std::vector<CalendarItem> o; return arr::parse_radarr_calendar(b, now - 86400 * 30, now + 86400 * 30, &o); }},
        {"sonarr/queue.json", [now](const std::string &b) { std::vector<QueueItem> o; int t = 0; return arr::parse_queue(b, config::Sonarr, now, &o, &t); }},
        {"radarr/queue.json", [now](const std::string &b) { std::vector<QueueItem> o; return arr::parse_queue(b, config::Radarr, now, &o); }},
        {"sonarr/series.json", into(arr::parse_series)},
        {"radarr/movie.json", into(arr::parse_movies)},
        {"sonarr/wanted_missing.json", [](const std::string &b) { std::vector<MissingEpisode> o; int t = 0; return arr::parse_missing_episodes(b, &o, &t); }},
        {"radarr/wanted_missing.json", [](const std::string &b) { std::vector<Movie> o; int t = 0; return arr::parse_missing_movies(b, &o, &t); }},
        {"radarr/qualityprofile.json", into(arr::parse_quality_profiles)},
        {"sonarr/episodefile_10.json", into(arr::parse_episode_files)},
        {"glances/quicklook.json", into(glances::parse_quicklook)},
        {"glances/quicklook.json", into(glances::parse_test)},
        {"emby/items_user_00fd1b015c664b5ab371dcd5ae315403_played.json", into(jellyfin::parse_played_episodes)},
        {"emby/users_1.json", into(jellyfin::parse_user)},
        {"jellyfin/items_index.json", into(jellyfin::parse_user_items)},
        {"jellyfin/library_virtualfolders.json", into(jellyfin::parse_libraries)},
        {"jellyfin/items_parent_7f008543d077e24931dfc5a50483b576_episode.json", into(jellyfin::parse_count)},
        {"jellyfin/system_configuration_encoding.json", into(jellyfin::parse_encoding)},
        {"jellyfin/system_info.json", into(jellyfin::parse_info)},
        {"jellyfin/system_info.json", into(jellyfin::parse_server_id)},
        {"jellyfin/system_activitylog_entries.json", into(jellyfin::parse_activity)},
        {"jellyfin/users.json", into(jellyfin::parse_users)},
        {"jellyfin/items_ids.json", into(jellyfin::parse_items)},
        {"jellyfin/sessions.json", into(jellyfin::parse_sessions)},
        {"emby/sessions.json", into(jellyfin::parse_sessions)},
        {"jellyfin/scheduledtasks.json", into(jellyfin::parse_tasks)},
        {"jellyfin/items.json", [](const std::string &b) { std::vector<RecentItem> o; return jellyfin::parse_recent(b, &o); }},
        {"jellyfin/quickconnect_enabled.json", into(jellyfin::parse_quick_connect_enabled)},
        {"jellyfin/quickconnect_initiate.json", into(jellyfin::parse_quick_connect)},
        {"jellyfin/quickconnect_connect.json", into(jellyfin::parse_quick_connect_poll)},
        {"jellyfin/users_authenticatewithquickconnect.json", into(jellyfin::parse_session)},
        {"seerr/settings_about.json", into(seerr::parse_about)},
        {"seerr/request_all.json", into(seerr::parse_requests)},
        {"seerr/movie_9068481.json", [](const std::string &b) { MediaDetail o; return seerr::parse_detail(b, false, &o); }},
        {"seerr/tv_9248046.json", [](const std::string &b) { MediaDetail o; return seerr::parse_detail(b, true, &o); }},
        {"seerr/issue_all.json", into(seerr::parse_issues)},
        {"seerr/issue_36.json", into(seerr::parse_issue)},
        {"seerr/discover_trending.json", into(seerr::parse_titles)},
        {"seerr/search.json", into(seerr::parse_titles)},
        {"seerr/movie_9068481.json", [](const std::string &b) { SeerrDetail o; return seerr::parse_title_detail(b, false, &o); }},
        {"seerr/tv_9248046.json", [](const std::string &b) { SeerrDetail o; return seerr::parse_title_detail(b, true, &o); }},
        {"seerr/settings_public.json", into(seerr::parse_public)},
        {"seerr/service_radarr.json", into(seerr::parse_servers)},
        {"seerr/service_radarr_0.json", into(seerr::parse_server)},
        {"seerr/request_all.json", into(seerr::parse_created)},
        {"plex/identity.json", [](const std::string &b) { std::string m, v; return plex::parse_identity(b, &m, &v); }},
        {"plex/root.json", into(plex::parse_info)},
        {"plex/root.json", into(plex::parse_plex_pass)},
        {"plex/status_sessions.json", into(plex::parse_sessions)},
        {"plex/library_metadata_ids.json", into(plex::parse_items)},
        {"plex/library_sections_all.json", into(plex::parse_sections)},
        {"plex/library_sections_1_all_recent.json", into(plex::parse_recent)},
        {"plex/library_sections_1_all.json", into(plex::parse_section_items)},
        {"plex/library_sections_2_all_count.json", into(plex::parse_count)},
        {"plex/butler.json", into(plex::parse_butler)},
        {"plex/activities.json", into(plex::parse_activities)},
        {"plex/status_sessions_history_all.json", into(plex::parse_history)},
        {"plex/accounts.json", into(plex::parse_accounts)},
        {"plex/accounts_1.json", into(plex::parse_account)},
        {"plex/prefs.json", into(plex::parse_prefs)},
    };
}

/* A copy of the document, each value changed by fn (which may replace it). */
using Change = std::function<cJSON *(cJSON *)>;

/* A new tree: v's copy, each value passed through fn (leaves first). */
cJSON *changed(const cJSON *v, const Change &fn)
{
    cJSON *n;
    if (cJSON_IsArray(v) || cJSON_IsObject(v)) {
        n = cJSON_IsArray(v) ? cJSON_CreateArray() : cJSON_CreateObject();
        for (const cJSON *c = v->child; c; c = c->next) {
            cJSON *k = changed(c, fn);
            if (cJSON_IsArray(v))
                cJSON_AddItemToArray(n, k);
            else
                cJSON_AddItemToObject(n, c->string ? c->string : "", k);
        }
    } else {
        n = cJSON_Duplicate(v, false);
    }
    cJSON *r = fn(n);
    if (r != n)
        cJSON_Delete(n);
    return r;
}

std::string mutate(const std::string &text, const Change &fn, const std::string &marker = {},
                   const std::string &replacement = {})
{
    cJSON *source = cJSON_ParseWithLength(text.data(), text.size());
    if (!source)
        return text;
    cJSON *root = changed(source, fn);
    cJSON_Delete(source);
    char *printed = cJSON_PrintUnformatted(root);
    std::string out = printed ? printed : "";
    cJSON_free(printed);
    cJSON_Delete(root);
    if (!marker.empty())
        for (size_t at = out.find(marker); at != std::string::npos; at = out.find(marker, at + replacement.size()))
            out.replace(at, marker.size(), replacement);
    return out;
}

std::vector<std::string> hostile_versions(const std::string &good)
{
    std::vector<std::string> out = {"", " ", "null", "0", "true", "\"text\"", "[]", "{}", "[null]", "[[[[[[]]]]]]",
                                    "[1,\"x\",true,null,{}]", "{\"records\":null,\"results\":7,\"Items\":\"x\"}",
                                    "<html><body>502 Bad Gateway</body></html>", "\xff\xfe\x00garbage",
                                    std::string(1000, '['), "{\"a\":" + std::string(500, '{')};
    for (size_t cut : {good.size() / 3, good.size() / 2, good.size() * 2 / 3, good.size() ? good.size() - 1 : 0})
        out.push_back(good.substr(0, cut));
    auto numbers_to = [](double d) {
        return [d](cJSON *v) -> cJSON * {
            if (cJSON_IsNumber(v))
                cJSON_SetNumberValue(v, d);
            return v;
        };
    };
    out.push_back(mutate(good, numbers_to(1e300)));
    out.push_back(mutate(good, numbers_to(-1e300)));
    out.push_back(mutate(good, numbers_to(9.3e18)));   /* just past int64_t */
    out.push_back(mutate(good, numbers_to(-1)));
    /* 1e400 reads as infinity: cJSON cannot print it, so a marker is replaced. */
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * { return cJSON_IsNumber(v) ? cJSON_CreateString("@INF@") : v; },
                         "\"@INF@\"", "1e400"));
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * { return cJSON_IsNumber(v) ? cJSON_CreateString("@INF@") : v; },
                         "\"@INF@\"", "-1e400"));
    /* Strings made huge numbers, numbers made strings, everything null. */
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * {
        return cJSON_IsString(v) ? cJSON_CreateNumber(1.2345678901234567e19) : v;
    }));
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * {
        return cJSON_IsNumber(v) ? cJSON_CreateString("99999999999999999999999") : v;
    }));
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * {
        return cJSON_IsArray(v) || cJSON_IsObject(v) ? v : cJSON_CreateNull();
    }));
    /* Objects and lists swapped (their content lost). */
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * {
        if (cJSON_IsObject(v) && v->child)
            return cJSON_CreateArray();
        if (cJSON_IsArray(v) && v->child)
            return cJSON_CreateObject();
        return v;
    }));
    /* Every string empty, or very long. */
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * { return cJSON_IsString(v) ? cJSON_CreateString("") : v; }));
    out.push_back(mutate(good, [](cJSON *v) -> cJSON * {
        return cJSON_IsString(v) ? cJSON_CreateString(std::string(5000, 'x').c_str()) : v;
    }));
    return out;
}

} // namespace

TEST(every_parser_survives_hostile_answers)
{
    int runs = 0, refused = 0;
    for (const Parser &p : parsers()) {
        const std::string good = fixture(p.fixture);
        CHECK(!good.empty());
        for (const std::string &bad : hostile_versions(good)) {
            refused += !p.parse(bad);
            runs++;
        }
    }
    CHECK(runs > 1500);
    CHECK(refused > 0);
}

TEST(json_numbers_out_of_range_give_the_default)
{
    json::Doc d(R"({"big":1e300,"inf":1e400,"neg":-1e400,"past":9.3e18,"ok":42.6,"int_past":3e9,"s":"12"})");
    CHECK_EQ(json::i64(d.root(), "big", -7), (int64_t)-7);
    CHECK_EQ(json::i64(d.root(), "inf", -7), (int64_t)-7);
    CHECK_EQ(json::i64(d.root(), "neg", -7), (int64_t)-7);
    CHECK_EQ(json::i64(d.root(), "past", -7), (int64_t)-7);
    CHECK_EQ(json::i64(d.root(), "ok"), (int64_t)43);
    CHECK_EQ(json::integer(d.root(), "int_past", -1), -1);   /* past an int */
    CHECK_EQ(json::i64(d.root(), "int_past"), (int64_t)3000000000LL);
    CHECK_EQ(json::num(d.root(), "inf", 5.0), 5.0);
    CHECK_EQ(json::integer(d.root(), "s", 3), 3);   /* a string is not a number here */
    int64_t n = 0;
    CHECK(!json::whole(NAN, &n) && !json::whole(INFINITY, &n) && !json::whole(9223372036854775808.0, &n));
    CHECK(json::whole(-9223372036854775808.0, &n) && n == INT64_MIN);
    CHECK_EQ(json::int_of("123"), 123);
    CHECK_EQ(json::int_of("-4"), -4);
    CHECK_EQ(json::int_of("12abc"), 12);
    CHECK_EQ(json::int_of("abc", -1), -1);
    CHECK_EQ(json::int_of("99999999999999999999999", -1), -1);
    CHECK_EQ(json::int_of("3000000000", -1), -1);
    CHECK_EQ(json::int_of(""), 0);
}
