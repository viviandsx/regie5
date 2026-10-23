/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The disks (Scrutiny) and the viewing statistics (Playback Reporting):
 * parsers on fixtures/ (made up: scripts/make-disks-stats-fixtures.py), what
 * a reading means, the periods' bars, the mock, the services, the config.
 */
#include "check.h"

#include "api/json.h"
#include "api/mock_transport.h"
#include "api/playback.h"
#include "api/scrutiny.h"
#include "api/services.h"
#include "core/config.h"
#include "core/config_web.h"
#include "core/disks.h"
#include "core/i18n.h"
#include "core/stats.h"

#include <chrono>
#include <filesystem>
#include <thread>

using check::fixture;

namespace {

void drain(http::Pool &pool)
{
    for (int i = 0; i < 500 && (pool.busy() > 0 || pool.pump() > 0); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.pump();
}

} // namespace

/* ---- Scrutiny ---- */

TEST(scrutiny_summary_09_and_08)
{
    for (const char *file : {"scrutiny/summary.json", "scrutiny/summary_0.8.json"}) {
        std::vector<model::Disk> d;
        CHECK(scrutiny::parse_summary(fixture(file), &d));
        CHECK_EQ(d.size(), (size_t)4);   /* the archived one left out */
        if (d.size() != 4)
            continue;
        CHECK_EQ(d[0].device, std::string("sda"));
        CHECK_EQ(d[3].device, std::string("nvme0"));   /* by name, shortest first */
        CHECK(d[0].kind == model::Disk::Ssd && d[1].kind == model::Disk::Hdd && d[3].kind == model::Disk::Nvme);
        CHECK_EQ(d[1].status, 1);
        CHECK_EQ(d[2].label, std::string("Sauvegardes"));
        CHECK_EQ(d[2].temp, 57);
        CHECK(d[0].has_smart && d[0].collected == json::parse_time("2026-10-23T00:00:06Z"));
        CHECK_EQ(d[1].capacity, (int64_t)8001563222016);
        CHECK_EQ(d[1].hours, (int64_t)39875);
    }
    /* 0.9: the key is the UUID; 0.8: the WWN; the path takes either. */
    std::vector<model::Disk> d9, d8;
    scrutiny::parse_summary(fixture("scrutiny/summary.json"), &d9);
    scrutiny::parse_summary(fixture("scrutiny/summary_0.8.json"), &d8);
    CHECK_EQ(d9[0].key, std::string("00000000-0000-4000-8000-000000000001"));
    CHECK_EQ(d8[0].key, std::string("0x5000000000000001"));
    CHECK_EQ(scrutiny::details_path(d9[0].key),
             std::string("/api/device/00000000-0000-4000-8000-000000000001/details?duration_key=week"));
    CHECK_EQ(scrutiny::details_path(d8[0].key), std::string("/api/device/0x5000000000000001/details?duration_key=week"));
    CHECK(scrutiny::details_path("../settings").empty());   /* nothing else reaches the path */
    CHECK(scrutiny::details_path("a?b").empty());
    CHECK(scrutiny::details_path("").empty());

    std::string n;
    CHECK(scrutiny::parse_test(fixture("scrutiny/summary.json"), &n));
    CHECK_EQ(n, std::string("4"));
    CHECK(!scrutiny::parse_summary("{\"success\": false}", &d9));
    CHECK(!scrutiny::parse_summary("<html>", &d9));
    CHECK(scrutiny::parse_summary(R"({"data": {"summary": {}}, "success": true})", &d9) && d9.empty());
}

TEST(scrutiny_settings_and_details)
{
    int t = 0;
    CHECK(scrutiny::parse_settings(fixture("scrutiny/settings.json"), &t) && t == 3);
    CHECK(scrutiny::parse_settings(R"({"settings": {"metrics": {"status_threshold": 1}}})", &t) && t == 1);
    CHECK(scrutiny::parse_settings(R"({"settings": {"metrics": {"status_threshold": 9}}})", &t) && t == 3);
    CHECK(!scrutiny::parse_settings("[]", &t));

    model::DiskDetail dd;
    CHECK(scrutiny::parse_details(fixture("scrutiny/device_00000000-0000-4000-8000-000000000002_details.json"), &dd));
    CHECK_EQ(dd.temps.size(), (size_t)7);
    CHECK(dd.temps.front().first < dd.temps.back().first);   /* oldest first */
    CHECK_EQ(dd.temps.back().second, 41);
    CHECK_EQ(dd.power_cycles, (int64_t)96);
    CHECK_EQ(dd.attributes.size(), (size_t)3);   /* only those not fine */
    if (dd.attributes.size() == 3) {
        /* failures (SMART, Scrutiny) first, then the warning */
        CHECK_EQ(dd.attributes[0].id, std::string("5"));
        CHECK_EQ(dd.attributes[0].raw, (int64_t)24);
        CHECK_EQ(dd.attributes[1].id, std::string("197"));
        CHECK_EQ(dd.attributes[2].id, std::string("198"));
        CHECK_EQ(dd.attributes[2].status, 2);
    }
    CHECK(scrutiny::parse_details(fixture("scrutiny/device_00000000-0000-4000-8000-000000000004_details.json"), &dd));
    CHECK(dd.attributes.empty());   /* the NVMe: all fine */
    CHECK(!scrutiny::parse_details("{\"success\": false}", &dd));
    /* As a real Scrutiny answers: a point each midnight (UTC) and one of the
     * moment asked, the same day: one per date, the later. */
    CHECK(scrutiny::parse_details(R"({"data": {"smart_results": [
        {"date": "2026-10-23T14:18:47.37501157Z", "temp": 35, "attrs": {}},
        {"date": "2026-10-23T00:00:00Z", "temp": 34, "attrs": {}},
        {"date": "2026-10-22T00:00:00Z", "temp": 34, "attrs": {}}]}})",
                                  &dd));
    CHECK_EQ(dd.temps.size(), (size_t)2);
    CHECK(dd.temps.size() == 2 && dd.temps[1].second == 35);
}

TEST(disks_states_heat_and_alerts)
{
    std::vector<model::Disk> d;
    scrutiny::parse_summary(fixture("scrutiny/summary.json"), &d);
    const time_t now = json::parse_time("2026-10-23T12:00:00Z");
    CHECK(disks::state(d[0], 3) == disks::State::Passed);
    CHECK(disks::state(d[1], 3) == disks::State::FailedSmart);
    CHECK(disks::state(d[1], 2) == disks::State::Passed);   /* Scrutiny set to count its own thresholds only */
    model::Disk both = d[1];
    both.status = 3;
    CHECK(disks::state(both, 3) == disks::State::FailedBoth);
    CHECK(disks::state(both, 2) == disks::State::FailedScrutiny);
    model::Disk none;
    CHECK(disks::state(none, 3) == disks::State::Unknown);
    CHECK(disks::alerts(none, 3, now).empty());

    CHECK(disks::alerts(d[0], 3, now).empty());
    auto a = disks::alerts(d[1], 3, now);
    CHECK(a.size() == 1 && a[0].kind == disks::Alert::FailedSmart && a[0].error);
    a = disks::alerts(d[2], 3, now);   /* 57 °C, a hard disk */
    CHECK(a.size() == 1 && a[0].kind == disks::Alert::Warm && !a[0].error);
    a = disks::alerts(d[3], 3, now);   /* the NVMe: 48 °C is fine, but its last reading is 4 days old */
    CHECK(a.size() == 1 && a[0].kind == disks::Alert::Stale);

    /* The thresholds: 50 / 60 °C a hard disk or a SATA SSD, 70 / 80 °C an NVMe. */
    model::Disk k;
    k.kind = model::Disk::Hdd;
    for (const auto &[temp, heat] : {std::pair<int, disks::Heat>{0, disks::Heat::Unknown}, {49, disks::Heat::Ok},
                                     {50, disks::Heat::Warm}, {59, disks::Heat::Warm}, {60, disks::Heat::Hot}}) {
        k.temp = temp;
        CHECK(disks::heat(k) == heat);
    }
    k.kind = model::Disk::Ssd;
    k.temp = 55;
    CHECK(disks::heat(k) == disks::Heat::Warm);
    k.kind = model::Disk::Nvme;
    CHECK(disks::heat(k) == disks::Heat::Ok);
    k.temp = 70;
    CHECK(disks::heat(k) == disks::Heat::Warm);
    k.temp = 80;
    CHECK(disks::heat(k) == disks::Heat::Hot);
    k.has_smart = true;
    CHECK(disks::alerts(k, 3, now).size() == 1 && disks::alerts(k, 3, now)[0].error);

    i18n::set_lang(i18n::find("fr"));
    CHECK_EQ(disks::name(d[0]), std::string("Disque sda"));
    CHECK_EQ(disks::name(d[2]), std::string("Sauvegardes"));
    CHECK(disks::attribute_name("5") == i18n::DkAttrReallocated);
    CHECK(disks::attribute_name("media_errors") == i18n::DkAttrMediaErrors);
    CHECK(disks::attribute_name("9") == i18n::Count);   /* not named: "Attribut 9" */
}

/* ---- Playback Reporting ---- */

TEST(playback_plugin_and_period)
{
    std::string v = "x";
    CHECK(playback::parse_plugins(fixture("jellyfin/plugins.json"), &v));
    CHECK_EQ(v, std::string("19.0.0.0"));
    CHECK(playback::parse_plugins(R"([{"Name": "TMDb", "Version": "1", "Status": "Active"}])", &v) && v.empty());
    /* installed but off (Disabled, NotSupported, Restart): as if absent */
    CHECK(playback::parse_plugins(R"([{"Name": "Playback Reporting", "Version": "19.0.0.0", "Status": "Restart",
                                      "Id": "5c534381-91a3-43cb-907a-35aa02eb9d2c"}])", &v) && v.empty());
    CHECK(!playback::parse_plugins("{}", &v));

    /* The plugin's own date (local), its offset in hours. */
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char today[16];
    std::snprintf(today, sizeof today, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    CHECK_EQ(playback::period_query(7, now, 7200), "days=7&endDate=" + std::string(today) + "&timezoneOffset=2");
    CHECK_EQ(playback::period_query(30, now, 19800), "days=30&endDate=" + std::string(today) + "&timezoneOffset=5.50");
    CHECK_EQ(playback::period_query(365, now, -14400), "days=365&endDate=" + std::string(today) + "&timezoneOffset=-4");
    CHECK_EQ(playback::users_path("days=7"), std::string("/user_usage_stats/user_activity?days=7"));
    CHECK_EQ(playback::days_path("days=7"), std::string("/user_usage_stats/PlayActivity?days=7&dataType=time"));
    CHECK_EQ(playback::movies_path("days=7"), std::string("/user_usage_stats/MoviesReport?days=7"));
    CHECK_EQ(playback::series_path("days=7"), std::string("/user_usage_stats/GetTvShowsReport?days=7"));
}

TEST(playback_reports)
{
    std::vector<model::PlayStats::User> users;
    CHECK(playback::parse_users(fixture("jellyfin/user_usage_stats_user_activity_30.json"), &users));
    CHECK(!users.empty());
    for (size_t i = 1; i < users.size(); i++)
        CHECK(users[i - 1].seconds >= users[i].seconds);   /* most time first */
    CHECK(playback::parse_users("[]", &users) && users.empty());   /* a plugin just installed */
    CHECK(!playback::parse_users("{}", &users));

    std::vector<std::pair<std::string, int64_t>> days;
    CHECK(playback::parse_days(fixture("jellyfin/user_usage_stats_playactivity_7.json"), &days));
    CHECK_EQ(days.size(), (size_t)7);
    CHECK_EQ(days.back().first, std::string("2026-10-23"));
    /* Every user's time added up, the empty "labels_user" giving every day. */
    CHECK(playback::parse_days(R"([{"user_id": "a", "user_name": "A", "user_usage": {"2026-10-22": 60, "2026-10-23": 30}},
                                   {"user_id": "b", "user_name": "B", "user_usage": {"2026-10-23": 90}},
                                   {"user_id": "labels_user", "user_name": "labels_user",
                                    "user_usage": {"2026-10-21": 0, "2026-10-22": 0, "2026-10-23": 0}}])",
                               &days));
    CHECK_EQ(days.size(), (size_t)3);
    CHECK(days[0] == std::make_pair(std::string("2026-10-21"), (int64_t)0));
    CHECK_EQ(days[1].second, (int64_t)60);
    CHECK_EQ(days[2].second, (int64_t)120);
    /* What a plugin just installed answers (seen on a real one). */
    CHECK(playback::parse_days(R"([{"user_id":"labels_user","user_name":"labels_user","user_usage":{"2026-10-17":0,"2026-10-18":0}}])",
                               &days) &&
          days.size() == 2);

    std::vector<model::PlayStats::Title> titles;
    CHECK(playback::parse_titles(fixture("jellyfin/user_usage_stats_gettvshowsreport_365.json"), &titles));
    CHECK(titles.size() > 5);
    for (size_t i = 1; i < titles.size(); i++)
        CHECK(titles[i - 1].plays >= titles[i].plays);
    CHECK(playback::parse_titles(R"([{"label": "", "count": 4, "time": 60}, {"label": "Film ", "count": 2, "time": 5}])",
                                 &titles));
    CHECK(titles.size() == 1 && titles[0].name == "Film");
}

TEST(stats_bars_and_totals)
{
    model::PlayStats s;
    s.users = {{"A", 3600, 2}, {"B", 1800, 1}};
    CHECK_EQ(stats::total_seconds(s), (int64_t)5400);
    CHECK_EQ(stats::total_plays(s), 3);
    CHECK(!stats::empty(s));
    CHECK(stats::empty(model::PlayStats{}));
    CHECK_EQ(stats::days(stats::Week), 7);
    CHECK_EQ(stats::days(stats::Year), 365);

    CHECK(playback::parse_days(fixture("jellyfin/user_usage_stats_playactivity_30.json"), &s.days));
    auto b = stats::bars(s, stats::Month);
    CHECK_EQ(b.size(), (size_t)30);
    CHECK(b.back().y == 2026 && b.back().m == 10 && b.back().d == 23);

    CHECK(playback::parse_days(fixture("jellyfin/user_usage_stats_playactivity_365.json"), &s.days));
    b = stats::bars(s, stats::Year);
    CHECK_EQ(b.size(), (size_t)12);   /* 2025-10-24 … 2026-10-23: October 2025, cut short, left out */
    CHECK(b.front().y == 2025 && b.front().m == 11 && b.front().d == 0);
    CHECK(b.back().y == 2026 && b.back().m == 10);
    int64_t year = 0, all = 0;
    for (const auto &x : b)
        year += x.seconds;
    for (const auto &d : s.days)
        all += d.first >= "2025-11-01" ? d.second : 0;
    CHECK_EQ(year, all);
}

/* ---- the mock, the services ---- */

TEST(mock_disks_and_stats)
{
    using M = api::MockTransport;
    CHECK_EQ(M::file_for("http://scrutiny.mock/api/summary"), std::string("scrutiny/summary.json"));
    CHECK_EQ(M::file_for("http://scrutiny.mock/scrutiny/api/device/00000000-0000-4000-8000-000000000001/details?duration_key=week"),
             std::string("scrutiny/device_00000000-0000-4000-8000-000000000001_details.json"));
    CHECK_EQ(M::file_for("http://jellyfin.mock/user_usage_stats/PlayActivity?days=30&endDate=2026-10-23&dataType=time"),
             std::string("jellyfin/user_usage_stats_playactivity_30.json"));
    CHECK_EQ(M::file_for("http://jellyfin.mock/Plugins"), std::string("jellyfin/plugins.json"));

    /* Dates moved to today: across a month, a year, keys too. */
    CHECK_EQ(M::dated(R"({"2026-10-23": 1, "d": "2026-10-23T00:00:06Z", "n": 12026-10-23})", 9),
             std::string(R"({"2026-11-01": 1, "d": "2026-11-01T00:00:06Z", "n": 12026-10-23})"));
    CHECK_EQ(M::dated("2025-12-31", 1), std::string("2026-01-01"));
    CHECK_EQ(M::dated("2026-03-01", -1), std::string("2026-02-28"));

    /* Empty: no disk, no play, but the plugin and the settings stay. */
    CHECK(M::emptied("scrutiny/summary.json", fixture("scrutiny/summary.json")).find("\"summary\":{}") != std::string::npos);
    CHECK_EQ(M::emptied("jellyfin/plugins.json", fixture("jellyfin/plugins.json")), fixture("jellyfin/plugins.json"));
    CHECK_EQ(M::emptied("jellyfin/user_usage_stats_user_activity_7.json", "[{\"a\": 1}]"), std::string("[]"));
}

TEST(services_disks_and_stats_on_mock)
{
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 2, nullptr);
    api::Services s(config::mock(), pool, "fr");
    api::Result<model::DiskReport> disks;
    api::Result<model::DiskDetail> detail;
    api::Result<std::string> plugin, test;
    api::Result<model::PlayStats> week, year;
    s.disks([&](api::Result<model::DiskReport> r) { disks = r; });
    s.disk_detail("00000000-0000-4000-8000-000000000002", [&](api::Result<model::DiskDetail> r) { detail = r; });
    s.stats_plugin([&](api::Result<std::string> r) { plugin = r; });
    s.play_stats(7, [&](api::Result<model::PlayStats> r) { week = r; });
    s.play_stats(365, [&](api::Result<model::PlayStats> r) { year = r; });
    s.test(config::Scrutiny, [&](api::Result<std::string> r) { test = r; });
    drain(pool);
    CHECK(disks.ok() && disks.value.disks.size() == 4 && disks.value.threshold == 3);
    CHECK(detail.ok() && detail.value.attributes.size() == 3);
    CHECK(plugin.ok() && plugin.value == "19.0.0.0");
    CHECK(test.ok() && test.value == "4");
    CHECK(week.ok() && week.value.days.size() == 7 && !week.value.users.empty() && !week.value.series.empty());
    CHECK(year.ok() && year.value.days.size() == 365 && !year.value.movies.empty());
    /* The mock moves the dates to today: the last day is today's. */
    if (week.ok() && !week.value.days.empty()) {
        const time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        char today[16];
        std::snprintf(today, sizeof today, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        CHECK_EQ(week.value.days.back().first, std::string(today));
    }

    /* A key Scrutiny would not give never becomes a request. */
    api::Result<model::DiskDetail> bad;
    s.disk_detail("../../api/settings", [&](api::Result<model::DiskDetail> r) { bad = r; });
    CHECK(bad.error == api::Error::BadResponse);

    /* Not with Emby nor Plex, nor with "statistics": false. */
    for (config::MediaServer m : {config::MediaServer::Emby, config::MediaServer::Plex}) {
        api::Services other(config::mock({}, false, m), pool, "fr");
        api::Result<std::string> p;
        other.stats_plugin([&](api::Result<std::string> r) { p = r; });
        CHECK(p.error == api::Error::Disabled);
    }
    config::Config off = config::mock();
    off.services[config::Jellyfin].stats = false;
    api::Services hidden(off, pool, "fr");
    api::Result<model::PlayStats> none;
    hidden.play_stats(7, [&](api::Result<model::PlayStats> r) { none = r; });
    CHECK(none.error == api::Error::Disabled);

    /* Scrutiny off: its screens' parts say nothing. */
    api::Services no_disks(config::mock("scrutiny"), pool, "fr");
    api::Result<model::DiskReport> nd;
    no_disks.disks([&](api::Result<model::DiskReport> r) { nd = r; });
    drain(pool);
    CHECK(nd.error == api::Error::Disabled);

    /* A plugin just installed: the four answers come, nothing in them. */
    api::MockTransport empty(REGIE5_FIXTURES, 0, "", "jellyfin");
    http::Pool pool2(empty, 2, nullptr);
    api::Services fresh(config::mock(), pool2, "fr");
    api::Result<std::string> p2;
    api::Result<model::PlayStats> st;
    fresh.stats_plugin([&](api::Result<std::string> r) { p2 = r; });
    fresh.play_stats(30, [&](api::Result<model::PlayStats> r) { st = r; });
    drain(pool2);
    CHECK(p2.ok() && !p2.value.empty());
    CHECK(st.ok() && stats::empty(st.value) && st.value.movies.empty());
}

/* ---- configuration, privacy ---- */

TEST(config_scrutiny_and_statistics)
{
    config::Config c;
    CHECK(config::parse(R"({"scrutiny": {"url": "https://media.example/scrutiny/", "username": "u", "password": "p"},
                            "jellyfin": {"url": "http://192.168.1.10:8096", "apiKey": "k", "statistics": false}})",
                        &c));
    CHECK(c.at(config::Scrutiny).enabled);   /* no key needed */
    CHECK_EQ(c.at(config::Scrutiny).url, std::string("https://media.example/scrutiny"));
    CHECK_EQ(c.at(config::Scrutiny).password, std::string("p"));
    CHECK(!c.at(config::Jellyfin).stats);
    CHECK(config::parse(R"({"jellyfin": {"url": "http://x:8096", "apiKey": "k"}})", &c));
    CHECK(c.at(config::Jellyfin).stats && !c.at(config::Scrutiny).enabled);
    CHECK_EQ(std::string(config::service_key(config::Scrutiny)), std::string("scrutiny"));

    /* The phone's page: an address, a user and a password kept unread, as for Glances. */
    std::string view = config_web::public_view(R"({"scrutiny": {"url": "http://192.168.1.10:8080", "password": "s3cret"}})");
    CHECK(view.find("s3cret") == std::string::npos);
    CHECK(view.find("\"hasPassword\":true") != std::string::npos);
    std::string out, err;
    CHECK(config_web::apply(R"({"scrutiny": {"url": "http://192.168.1.10:8080", "password": "s3cret"}})",
                            R"({"services": {"scrutiny": [{"from": 0, "url": "http://192.168.1.10:8081", "username": "",
                                                            "password": "", "enabled": true}]}})",
                            &out, &err));
    CHECK(config::parse(out, &c) && c.at(config::Scrutiny).url == "http://192.168.1.10:8081" &&
          c.at(config::Scrutiny).password == "s3cret");   /* left empty: kept */
}

TEST(disk_fixtures_name_no_disk)
{
    /* No serial number, model, maker or firmware but made-up ones; no client or device in the statistics. */
    for (const char *root : {REGIE5_FIXTURES, REGIE5_DEMO}) {
        int files = 0;
        for (const char *dir : {"scrutiny", "jellyfin"}) {
            const std::filesystem::path base = std::filesystem::path(root) / dir;
            if (!std::filesystem::exists(base))
                continue;
            for (const auto &e : std::filesystem::directory_iterator(base)) {
                const std::string name = e.path().filename().string();
                if (std::string(dir) == "jellyfin" && name.rfind("user_usage_stats_", 0) != 0)
                    continue;
                files++;
                const std::string text = fixture(std::string(dir) + "/" + name, root);
                for (const char *field : {"serial_number", "device_serial_id", "manufacturer", "firmware", "client_name"}) {
                    const std::string k = std::string("\"") + field + "\": \"";
                    for (size_t at = text.find(k); at != std::string::npos; at = text.find(k, at + 1))
                        if (text[at + k.size()] != '"')
                            check::fail(__FILE__, __LINE__, e.path().string() + ": " + field + " is not empty");
                }
                const std::string model = "\"model_name\": \"", made_up = "Disque de démonstration\"";
                for (size_t at = text.find(model); at != std::string::npos; at = text.find(model, at + 1))
                    if (text.compare(at + model.size(), made_up.size(), made_up) != 0)
                        check::fail(__FILE__, __LINE__, e.path().string() + ": a model name that is not made up");
            }
        }
        CHECK(files >= 15);
    }
}
