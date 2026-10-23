/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/playback.h"

#include "api/json.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace playback {

namespace {

/* The plugin's id (its build.yaml): the name could be translated. */
const char *const kId = "5c53438191a343cb907a35aa02eb9d2c";

std::string lowered_id(std::string s)
{
    std::string out;
    for (char c : s)
        if (c != '-')
            out += (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    return out;
}

} // namespace

std::string plugins_path() { return "/Plugins"; }

bool parse_plugins(const std::string &body, std::string *version)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    version->clear();
    const cJSON *p;
    cJSON_ArrayForEach(p, d.root())
    {
        if (lowered_id(json::str(p, "Id")) != kId && json::str(p, "Name") != "Playback Reporting")
            continue;
        if (json::str(p, "Status") == "Active")
            *version = json::str(p, "Version", "?");
    }
    return true;
}

std::string period_query(int days, time_t now, long utc_offset_seconds)
{
    struct tm t;
    localtime_r(&now, &t);
    char buf[96];
    /* The offset in hours, as the plugin takes it ("2", "5.5", "-4"). */
    const long h = utc_offset_seconds / 3600, m = (utc_offset_seconds % 3600) / 60;
    if (m == 0)
        std::snprintf(buf, sizeof buf, "days=%d&endDate=%04d-%02d-%02d&timezoneOffset=%ld", days, t.tm_year + 1900,
                      t.tm_mon + 1, t.tm_mday, h);
    else
        std::snprintf(buf, sizeof buf, "days=%d&endDate=%04d-%02d-%02d&timezoneOffset=%ld.%02ld", days,
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, h, (m < 0 ? -m : m) * 100 / 60);
    return buf;
}

std::string users_path(const std::string &period) { return "/user_usage_stats/user_activity?" + period; }
std::string days_path(const std::string &period) { return "/user_usage_stats/PlayActivity?" + period + "&dataType=time"; }
std::string movies_path(const std::string &period) { return "/user_usage_stats/MoviesReport?" + period; }
std::string series_path(const std::string &period) { return "/user_usage_stats/GetTvShowsReport?" + period; }

bool parse_users(const std::string &body, std::vector<model::PlayStats::User> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    /* Never item_name nor client_name (the last thing played, and on what). */
    const cJSON *u;
    cJSON_ArrayForEach(u, d.root())
    {
        model::PlayStats::User x;
        x.name = json::str(u, "user_name");
        x.seconds = json::i64(u, "total_time");
        x.plays = json::integer(u, "total_count");
        if (!x.name.empty() && (x.seconds > 0 || x.plays > 0))
            out->push_back(std::move(x));
    }
    std::stable_sort(out->begin(), out->end(),
                     [](const model::PlayStats::User &a, const model::PlayStats::User &b) { return a.seconds > b.seconds; });
    return true;
}

bool parse_days(const std::string &body, std::vector<std::pair<std::string, int64_t>> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    /* One row per user and an empty "labels_user" with every day of the period. */
    std::map<std::string, int64_t> days;
    const cJSON *u;
    cJSON_ArrayForEach(u, d.root())
    {
        const cJSON *usage = json::get(u, "user_usage");
        const cJSON *day;
        cJSON_ArrayForEach(day, usage)
        {
            if (!day->string || std::string(day->string).size() != 10)
                continue;
            int64_t v = 0;
            if (cJSON_IsNumber(day))
                json::whole(day->valuedouble, &v);
            days[day->string] += std::max<int64_t>(0, v);
        }
    }
    out->assign(days.begin(), days.end());
    return true;
}

bool parse_titles(const std::string &body, std::vector<model::PlayStats::Title> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *t;
    cJSON_ArrayForEach(t, d.root())
    {
        model::PlayStats::Title x;
        x.name = json::str(t, "label");
        x.plays = json::integer(t, "count");
        x.seconds = json::i64(t, "time");
        /* The series report cuts "Series - s01e01 - Title" at " - ": a title without one comes out empty. */
        while (!x.name.empty() && x.name.back() == ' ')
            x.name.pop_back();
        if (!x.name.empty() && x.plays > 0)
            out->push_back(std::move(x));
    }
    std::stable_sort(out->begin(), out->end(), [](const model::PlayStats::Title &a, const model::PlayStats::Title &b) {
        return a.plays != b.plays ? a.plays > b.plays : a.seconds > b.seconds;
    });
    return true;
}

} // namespace playback
