/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Words and numbers for the screen, in the interface's language: dates,
 * durations, sizes, errors.
 */
#pragma once

#include "api/services.h"
#include "core/i18n.h"
#include "core/model.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace fmt {

/* printf into a string. */
std::string f(const char *pattern, ...) __attribute__((format(printf, 1, 2)));

/* "Aujourd'hui", "Demain", "jeu. 15 oct." (local time). */
std::string day(time_t t, time_t now);
/* "21:00" (local time). */
std::string clock(time_t t);
/* A guide column's day: "lun. 6" / "Mon 6". */
std::string weekday(time_t t);
/* A week from its first day: "6 – 12 oct." / "Oct 6 – 12" (months named when they differ). */
std::string week_range(time_t first_day);
/* Local midnight of the Monday of t's week. */
time_t week_start(time_t t);
/* "1 h 05", "21 min", "< 1 min", "2 j 3 h". */
std::string duration(int64_t seconds);
/* "il y a 3 h", "2 d ago". */
std::string ago(time_t t, time_t now);
/* "1,2 To" / "1.2 TB". */
std::string size(int64_t bytes);
/* "S01E02". */
std::string episode(int season, int episode);
/* Why a call failed, in words. */
std::string error(api::Error e, int http_status);
/* A codec as people know it: "hevc" -> "HEVC", "PGSSUB" -> "PGS", "truehd" -> "TrueHD". */
std::string codec(const std::string &name);
/* "2160p", "1080p"... from a height. */
std::string resolution(int height);
/* "7.1", "5.1", "2.0"... */
std::string channels(int n);
/* "12.5 Mb/s" / "12,5 Mb/s". */
std::string bitrate(int64_t bits_per_second);
/* A short lead or delay: "45 s", "6 min". */
std::string seconds(double s);
/* A count with its noun: one (n <= 1 in French, n == 1 in English) or many,
 * both patterns taking %d. */
std::string count(i18n::S one, i18n::S many, int n);
/* "Sonarr : text" in French, "Sonarr: text" in English. */
std::string labelled(const std::string &who, const std::string &what);
/* A health alert in words: the interface's own sentence for a known check,
 * the server's text for an unknown one when it names nothing, else a generic one. */
std::string health(const model::Health &h);
/* "Erreur", "Avertissement", "Information". */
std::string health_level(model::Health::Level l);

/* Jellyfin's transcode reasons, in words, joined. */
std::string transcode_reasons(const std::vector<std::string> &reasons);

} // namespace fmt
