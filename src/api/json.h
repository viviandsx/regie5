/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Small helpers over cJSON for the parsers: lenient reads (a missing or
 * mistyped field gives the default), dates and durations.
 */
#pragma once

#include "cJSON.h"

#include <cstdint>
#include <ctime>
#include <functional>
#include <initializer_list>
#include <string>

namespace json {

/* Owns a parsed document. */
class Doc {
public:
    explicit Doc(const std::string &text) : m_root(cJSON_ParseWithLength(text.data(), text.size())) {}
    ~Doc() { cJSON_Delete(m_root); }
    Doc(const Doc &) = delete;
    Doc &operator=(const Doc &) = delete;
    const cJSON *root() const { return m_root; }
    bool ok() const { return m_root != nullptr; }

private:
    cJSON *m_root;
};

/* A long list read one element at a time: a whole document as cJSON takes
 * about five times its text (a 5 000-film Radarr: 19 MiB of text, 110 MiB of
 * tree), one element only a few KiB. path: the members leading to the list
 * from the top object ({"Items"}, {"MediaContainer", "Metadata"}); empty: the
 * text is the list. fn sees each element (a lone object in place of the list
 * counts as one). Bad: not JSON, or not shaped so; Missing: the last member is
 * absent (fn never called); Done: the list was read. */
enum class Each { Bad, Missing, Done };
Each each(const std::string &text, std::initializer_list<const char *> path, const std::function<void(const cJSON *)> &fn);

const cJSON *get(const cJSON *o, const char *key);
std::string str(const cJSON *o, const char *key, const std::string &def = {});
int64_t i64(const cJSON *o, const char *key, int64_t def = 0);
int integer(const cJSON *o, const char *key, int def = 0);
double num(const cJSON *o, const char *key, double def = 0);
bool boolean(const cJSON *o, const char *key, bool def = false);

/* "2026-10-11T20:00:00Z", "2026-10-11T20:00:00.000Z", "2026-10-11T20:00:00.1234567Z",
 * "2026-10-11" (midnight UTC), with an optional +hh:mm offset: seconds since
 * the epoch (UTC). 0 when it does not parse. */
time_t parse_time(const std::string &s);

/* .NET TimeSpan text: "hh:mm:ss", "d.hh:mm:ss", with optional ".fffffff".
 * Seconds, or -1 when it does not parse. */
int64_t parse_timespan(const std::string &s);

/* Days since 1970-01-01 for a civil date (no time zone involved). */
int64_t days_from_civil(int y, int m, int d);

} // namespace json
