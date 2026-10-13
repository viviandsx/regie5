/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * config.json, dates and durations.
 */
#include "check.h"

#include "api/json.h"
#include "core/app_settings.h"
#include "core/badges.h"
#include "core/cleanup.h"
#include "core/config.h"
#include "core/i18n.h"
#include "core/lang/language.h"
#include "core/versions.h"

#include <cctype>
#include <cstdio>
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

/* The printf conversions of a text ("%d", "%s"…), in order: a translation
 * must keep them, or the app would read the wrong argument. */
static std::string conversions(const char *t)
{
    std::string out;
    for (const char *p = t; *p; p++) {
        if (*p != '%')
            continue;
        if (p[1] == '%') {
            p++;
            continue;
        }
        const char *q = p + 1;
        while (*q && !std::isalpha((unsigned char)*q))
            q++;
        while (*q == 'l' || *q == 'h' || *q == 'z')
            q++;
        out += std::string(p, q + (*q ? 1 : 0)) + " ";
        p = *q ? q : q - 1;
    }
    return out;
}

TEST(i18n_languages)
{
    int n = 0;
    const i18n::Language *const *all = i18n::all_languages(&n);
    CHECK(n >= 2);
    CHECK_EQ(std::string(all[0]->code), std::string("en"));   /* the fallback */
    const i18n::Language &en = *all[0];
    const char *english[i18n::Count] = {};
    for (int i = 0; i < en.count; i++)
        english[en.texts[i].key] = en.texts[i].text;
    for (int k = 0; k < i18n::Count; k++)
        if (!english[k] || !english[k][0])
            check::fail(__FILE__, __LINE__, "English has no text for key " + std::to_string(k));

    for (int l = 0; l < n; l++) {
        const i18n::Language &lang = *all[l];
        const std::string where = std::string("language ") + lang.code + ": ";
        bool seen[i18n::Count] = {};
        int have = 0;
        for (int i = 0; i < lang.count; i++) {
            const i18n::Text &t = lang.texts[i];
            if (t.key < 0 || t.key >= i18n::Count || !t.text) {
                check::fail(__FILE__, __LINE__, where + "a bad entry, #" + std::to_string(i));
                continue;
            }
            if (seen[t.key])
                check::fail(__FILE__, __LINE__, where + "key " + std::to_string(t.key) + " twice");
            seen[t.key] = true;
            have++;
            if (english[t.key] && conversions(t.text) != conversions(english[t.key]))
                check::fail(__FILE__, __LINE__, where + "\"" + t.text + "\" does not keep the " +
                                                    conversions(english[t.key]) + "of \"" + english[t.key] + "\"");
        }
        /* The languages shipped are complete; one still being translated falls
         * back to English (and says how far it is). */
        if (have < i18n::Count)
            std::printf("  %s: %d of %d texts, the others in English\n", lang.code, have, (int)i18n::Count);
        for (int d = 0; d < 7; d++)
            CHECK(lang.days[d] && lang.days[d][0]);
        for (int m = 0; m < 12; m++)
            CHECK(lang.months[m] && lang.months[m][0]);
        for (int u = 0; u < 6; u++)
            CHECK(lang.size_units[u] && lang.size_units[u][0]);
        CHECK(std::string(lang.week_same).find("{d1}") != std::string::npos);
        CHECK(std::string(lang.week_other).find("{mon2}") != std::string::npos);
        CHECK(i18n::known(lang.code));
        CHECK(&i18n::find(lang.code) == &lang);
    }
    for (const char *shipped : {"en", "fr"}) {
        i18n::set_lang(i18n::find(shipped));
        for (int k = 0; k < i18n::Count; k++)
            CHECK(i18n::tr((i18n::S)k)[0] != '\0');
    }

    /* Codes as systems give them. */
    CHECK_EQ(std::string(i18n::find("fr_FR.UTF-8").code), std::string("fr"));
    CHECK_EQ(std::string(i18n::find("en-GB").code), std::string("en"));
    CHECK_EQ(std::string(i18n::find("xx").code), std::string("en"));   /* unknown: English */
    CHECK(!i18n::known("xx"));
    CHECK(i18n::known("FR"));

    i18n::set_lang(i18n::find("fr"));
    CHECK(i18n::singular(0) && i18n::singular(1) && !i18n::singular(2));
    CHECK(i18n::language().decimal == ',');
    i18n::set_lang(i18n::find("en"));
    CHECK(!i18n::singular(0) && i18n::singular(1));
    CHECK(i18n::language().decimal == '.');
    i18n::set_lang(i18n::find("fr"));
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
    s.kept.push_back({config::Radarr, "Radarr 4K", 42, "The Glass Comet"});
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

TEST(versions_minimum)
{
    int o = 9;
    CHECK(versions::compare("10.11.2", "10.9", &o) && o > 0);   /* number by number, not as text */
    CHECK(versions::compare("4.0", "4.0.0.0", &o) && o == 0);
    CHECK(versions::compare("3.0.10.1567", "4.0", &o) && o < 0);
    CHECK(versions::compare("4.0.1-beta", "4.0.1", &o) && o == 0);
    CHECK(!versions::compare("develop", "4.0", &o));

    CHECK(versions::supported(config::Sonarr, "4.0.20.3014"));     /* the fixtures' */
    CHECK(!versions::supported(config::Sonarr, "3.0.10.1567"));
    CHECK(versions::supported(config::Radarr, "6.4.4.10685"));
    CHECK(!versions::supported(config::Radarr, "4.7.5.7809"));
    CHECK(versions::supported(config::Seerr, "3.5.0"));
    CHECK(versions::supported(config::Seerr, "2.7.3"));             /* Jellyseerr */
    CHECK(!versions::supported(config::Seerr, "1.34.0"));           /* Overseerr */
    CHECK(versions::supported(config::Jellyfin, "12.2.0"));
    CHECK(versions::supported(config::Jellyfin, "10.11.0"));
    CHECK(!versions::supported(config::Jellyfin, "10.8.13"));
    CHECK(versions::supported(config::Jellyfin, ""));                /* unknown: let through */
    CHECK(versions::supported(config::Sonarr, "develop"));
    CHECK(versions::supported(config::Glances, "Intel(R) Core(TM) i5"));   /* no minimum */
}

TEST(tab_badges)
{
    std::vector<model::QueueItem> q(3);
    CHECK_EQ(badges::queue(q).count, 3);
    CHECK(badges::queue(q).tone == badges::Neutral);
    q[1].status = model::QueueItem::Warning;
    CHECK(badges::queue(q).tone == badges::Warning);
    q[0].status = model::QueueItem::Error;   /* the worst wins, wherever it is */
    CHECK(badges::queue(q).tone == badges::Problem);
    CHECK(!badges::queue({}).shown());   /* an empty queue: no badge */
    CHECK(!badges::Badge{}.shown());     /* unknown: none either */

    model::RequestPage page;
    page.requests.resize(20);   /* one page of a longer list */
    page.total = 27;
    CHECK_EQ(badges::requests(page).count, 27);
    page.total = 0;   /* no pageInfo: the page itself */
    CHECK_EQ(badges::requests(page).count, 20);
    CHECK(badges::requests(page).tone == badges::Attention);

    CHECK_EQ(badges::sessions(std::vector<model::Session>(2)).count, 2);
    CHECK_EQ(badges::label(7), std::string("7"));
    CHECK_EQ(badges::label(100), std::string("99+"));
}
