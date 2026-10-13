/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * config.json, dates and durations.
 */
#include "check.h"

#include "api/json.h"
#include "core/app_settings.h"
#include "core/cleanup.h"
#include "core/config.h"
#include "core/i18n.h"

#include <cstdlib>
#include <unistd.h>

TEST(config_parses_services)
{
    config::Config c;
    CHECK(config::parse(R"({
        "sonarr": {"enabled": true, "url": "http://192.168.1.10:8989/", "apiKey": "abc"},
        "radarr": {"enabled": false, "url": "http://192.168.1.10:7878", "apiKey": "def"},
        "seerr": {"url": "http://192.168.1.10:5055", "apiKey": "ghi"},
        "jellyfin": {"enabled": true, "url": "http://192.168.1.10:8096"}
    })",
                        &c));
    CHECK(c.at(config::Sonarr).enabled);
    CHECK_EQ(c.at(config::Sonarr).url, std::string("http://192.168.1.10:8989"));   /* trailing slash dropped */
    CHECK(!c.at(config::Radarr).enabled);                                          /* off */
    CHECK(c.at(config::Seerr).enabled);                                            /* on unless said otherwise */
    CHECK(!c.at(config::Jellyfin).enabled);                                        /* no key: off */
}

TEST(config_rejects_bad_json)
{
    config::Config c;
    CHECK(!config::parse("{ not json", &c));
    CHECK(!config::parse("[1, 2]", &c));
    CHECK(config::load("/nonexistent/regie5/config.json", &c) == config::LoadError::Missing);
}

TEST(config_reread_tells_what_changed)
{
    config::Config a, b;
    const char *json = R"({
        "sonarr": {"url": "http://192.168.1.10:8989", "apiKey": "k1"},
        "jellyfin": {"url": "http://192.168.1.10:8096", "apiKey": "k2"}
    })";
    CHECK(config::parse(json, &a));
    CHECK(config::parse(json, &b));
    CHECK_EQ(config::changed(a, b), 0u);
    b.services[config::Jellyfin].api_key = "k3";
    CHECK_EQ(config::changed(a, b), 1u << config::Jellyfin);
    b.services[config::Sonarr].enabled = false;
    CHECK_EQ(config::changed(a, b), (1u << config::Jellyfin) | (1u << config::Sonarr));

    /* A file that turned bad fails to load: the caller keeps the one it has. */
    char path[] = "/tmp/regie5-test-XXXXXX";
    const int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    const char bad[] = "{ \"sonarr\": ";
    CHECK(write(fd, bad, sizeof bad - 1) == (ssize_t)(sizeof bad - 1));
    close(fd);
    config::Config c;
    CHECK(config::load(path, &c) == config::LoadError::BadJson);
    unlink(path);
}

TEST(config_example_is_valid)
{
    config::Config c;
    CHECK(config::load(std::string(REGIE5_SOURCE) + "/config.example.json", &c) == config::LoadError::None);
    for (int i = 0; i < config::ServiceCount; i++) {
        /* Glances is optional: described, off until installed. */
        CHECK(c.services[i].enabled == (i != config::Glances));
        CHECK(c.services[i].url.rfind("http://192.168.", 0) == 0);   /* local addresses in the example */
    }
}

TEST(json_parses_times)
{
    CHECK_EQ((long long)json::parse_time("1970-01-01T00:00:00Z"), 0LL);
    CHECK_EQ((long long)json::parse_time("2026-10-11T20:00:00Z"), 1791748800LL);
    CHECK_EQ((long long)json::parse_time("2026-10-11T20:00:00.000Z"), 1791748800LL);
    CHECK_EQ((long long)json::parse_time("2026-10-11T20:00:00.1234567Z"), 1791748800LL);
    CHECK_EQ((long long)json::parse_time("2026-10-11T22:00:00+02:00"), 1791748800LL);
    CHECK_EQ((long long)json::parse_time("2026-10-11"), 1791676800LL);
    CHECK_EQ((long long)json::parse_time(""), 0LL);
    CHECK_EQ((long long)json::parse_time("soon"), 0LL);
}

TEST(json_parses_timespans)
{
    CHECK_EQ((long long)json::parse_timespan("00:21:00"), 1260LL);
    CHECK_EQ((long long)json::parse_timespan("1.02:03:04"), 93784LL);
    CHECK_EQ((long long)json::parse_timespan("00:00:05.1234567"), 5LL);
    CHECK_EQ((long long)json::parse_timespan(""), -1LL);
    CHECK_EQ((long long)json::parse_timespan("later"), -1LL);
}

TEST(i18n_every_string_in_both_languages)
{
    for (i18n::Lang l : {i18n::Lang::French, i18n::Lang::English}) {
        i18n::set_lang(l);
        for (int k = 0; k < i18n::Count; k++)
            CHECK(i18n::tr((i18n::S)k)[0] != '\0');
    }
    i18n::set_lang(i18n::Lang::French);
}

TEST(config_instances_and_ignored_users)
{
    config::Config c;
    CHECK(config::parse(R"({
        "radarr": [
            {"name": "Radarr", "url": "http://192.168.1.10:7878", "apiKey": "a"},
            {"name": "Radarr 4K", "url": "https://media.example/radarr4k/", "apiKey": "b"}
        ],
        "sonarr": {"url": "http://192.168.1.10:8989", "apiKey": "c"},
        "cleanup": {"ignoreUsers": ["test", "Invité"]}
    })", &c));
    CHECK_EQ(c.count(config::Radarr), 2);
    CHECK_EQ(c.at(config::Radarr, 1).name, std::string("Radarr 4K"));
    CHECK_EQ(c.at(config::Radarr, 1).url, std::string("https://media.example/radarr4k"));   /* a path kept */
    CHECK_EQ(c.count(config::Sonarr), 1);
    CHECK_EQ(c.at(config::Sonarr).name, std::string("Sonarr"));
    CHECK_EQ(c.ignore_users.size(), (size_t)2);
    config::Config d = c;
    d.extra[config::Radarr][0].api_key = "z";
    CHECK_EQ(config::changed(c, d), 1u << config::Radarr);   /* the second instance counts */
}

TEST(config_is4k_is_said_never_guessed)
{
    config::Config c;
    CHECK(config::parse(R"({
        "radarr": [
            {"name": "Radarr 4K", "url": "http://a:7878", "apiKey": "a"},
            {"name": "UHD", "url": "http://b:7878", "apiKey": "b", "is4k": true}
        ],
        "sonarr": {"url": "http://c:8989", "apiKey": "c", "is4k": "yes"}
    })", &c));
    CHECK(!c.at(config::Radarr, 0).is4k);   /* "4K" in the name is not enough */
    CHECK(c.at(config::Radarr, 1).is4k);
    CHECK(!c.at(config::Sonarr).is4k);      /* only a JSON true counts */
    config::Config d = c;
    d.extra[config::Radarr][0].is4k = false;
    CHECK_EQ(config::changed(c, d), 1u << config::Radarr);
    CHECK(config::mock({}, true).at(config::Radarr, 1).is4k);
    /* The example that documents it reads as it says. */
    config::Config e;
    CHECK(config::load(std::string(REGIE5_SOURCE) + "/config.example.json", &e) == config::LoadError::None);
    CHECK(e.at(config::Radarr).enabled && !e.at(config::Radarr).is4k && !e.at(config::Glances).enabled);
}

TEST(app_settings_round_trip)
{
    app_settings::Settings s;
    CHECK(!s.cleanup_enabled);   /* off by default */
    s.cleanup_enabled = true;
    s.cleanup_everyone = false;
    s.cleanup_months = 12;
    s.kept.push_back({config::Radarr, "Radarr 4K", 42, "Dune"});
    app_settings::Settings t;
    CHECK(app_settings::from_json(app_settings::to_json(s), &t));
    CHECK(t.cleanup_enabled && !t.cleanup_everyone && t.cleanup_months == 12);
    CHECK(t.is_kept(config::Radarr, "Radarr 4K", 42));
    CHECK(!t.is_kept(config::Radarr, "Radarr", 42));
    CHECK_EQ(app_settings::path_beside("/home/x/.config/regie5/config.json"),
             std::string("/home/x/.config/regie5/settings.json"));
}

TEST(cleanup_candidates)
{
    const time_t now = 1800000000, year = 365 * 86400;
    std::vector<model::JellyfinUser> users = {
        {"u1", "Alice", false, true, now - 86400},
        {"u2", "Bruno", false, false, now - 86400},
        {"u3", "Test", false, false, now - 86400},          /* ignored by name */
        {"u4", "Ancien", false, false, now - 2 * year},     /* inactive */
        {"u5", "Désactivé", true, false, now - 86400},      /* disabled */
    };
    const std::vector<std::string> ignore = {"test"};
    CHECK_EQ(cleanup::active_users(users, ignore, now).size(), (size_t)2);

    auto movie = [](int id, const char *title, int tmdb, const char *imdb, int64_t size) {
        model::Movie m;
        m.id = id, m.title = title, m.tmdb_id = tmdb, m.imdb_id = imdb, m.size = size;
        return m;
    };
    cleanup::Library radarr;
    radarr.service = config::Radarr, radarr.instance_name = "Radarr";
    radarr.movies = {movie(1, "Old, both", 101, "", 8e9), movie(2, "Old, one", 102, "", 9e9),
                     movie(3, "Favourite", 103, "", 7e9), movie(4, "Recent", 104, "", 6e9),
                     movie(5, "Unknown to Jellyfin", 105, "", 5e9), movie(6, "By IMDb", 0, "tt6", 4e9),
                     movie(7, "Kept", 107, "", 3e9), movie(8, "No file", 108, "", 0)};
    cleanup::Library sonarr;
    sonarr.service = config::Sonarr, sonarr.instance_name = "Sonarr";
    model::Series ended, running;
    ended.id = 20, ended.title = "Ended", ended.tvdb_id = 201, ended.size = 20e9, ended.status = model::Series::Ended;
    running.id = 21, running.title = "Running", running.tvdb_id = 202, running.size = 30e9;
    sonarr.series = {ended, running};

    auto item = [](int tmdb, const char *imdb, bool played, time_t last, bool fav = false) {
        model::UserItem u;
        u.tmdb_id = tmdb, u.imdb_id = imdb, u.played = played, u.last_played = last, u.favorite = fav;
        return u;
    };
    auto show = [](int tvdb, int left, time_t last) {
        model::UserItem u;
        u.series = true, u.tvdb_id = tvdb, u.unplayed = left, u.last_played = last;
        return u;
    };
    const time_t old = now - year;
    std::map<std::string, std::vector<model::UserItem>> items;
    items["u1"] = {item(101, "", true, old), item(102, "", true, old), item(103, "", true, old),
                   item(104, "", true, now - 86400), item(0, "tt6", true, old), item(107, "", true, old),
                   item(108, "", true, old), show(201, 0, old), show(202, 0, old)};
    items["u2"] = {item(101, "", true, old), item(102, "", false, 0), item(103, "", true, old, true),
                   item(104, "", true, old), item(0, "tt6", true, old), item(107, "", true, old),
                   item(108, "", true, old), show(201, 0, old), show(202, 0, old)};
    items["u3"] = {item(102, "", false, 0)};   /* ignored: does not count against "everyone" */

    app_settings::Settings kept;
    kept.kept.push_back({config::Radarr, "Radarr", 7, "Kept"});
    cleanup::Criteria everyone{true, 6};
    auto ids = [](const std::vector<cleanup::Candidate> &c) {
        std::vector<int> v;
        for (const auto &x : c)
            v.push_back(x.id);
        return v;
    };
    const auto c1 = cleanup::candidates({radarr, sonarr}, users, items, ignore, everyone, kept, now);
    /* Biggest first: the ended series, then the films everyone watched long ago. */
    CHECK(ids(c1) == (std::vector<int>{20, 1, 6}));
    cleanup::Criteria anyone{false, 6};
    const auto c2 = cleanup::candidates({radarr, sonarr}, users, items, ignore, anyone, kept, now);
    CHECK(ids(c2) == (std::vector<int>{20, 2, 1, 6}));   /* "one" joins; favourite, recent, kept never */
    if (!c1.empty())
        CHECK_EQ(c1[0].watched_by.size(), (size_t)2);
}
