/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One language of the interface: its texts (one per i18n::S key) and the
 * rules the formatting follows (plurals, decimal mark, dates, units). Each
 * language is a file of its own in src/core/lang/ and a line in
 * languages.cpp; docs/TRANSLATING.md says how to add one.
 */
#pragma once

#include "core/i18n.h"

namespace i18n {

/* Which counts take the singular ("1 film" / "0 film" in French). */
enum class Plural {
    One,           /* 1 only (English, German, Spanish…) */
    ZeroAndOne,    /* 0 and 1 (French, Portuguese of Brazil…) */
};

struct Text {
    S key;
    const char *text;
};

struct Language {
    const char *code;          /* ISO 639-1, as --lang takes it: "fr" */
    const char *name;          /* in the language itself: "Français" */
    Plural plural;
    char decimal;              /* "5,6 To" or "5.6 TB" */
    const char *days[7];       /* short day names, Sunday first */
    const char *months[12];    /* short month names */
    const char *size_units[6]; /* bytes, then ×1000: B, KB, MB, GB, TB, PB */
    const char *day_unit;      /* a duration's days: "2 d 4 h" */
    /* Dates, with {wd} (day name), {d} (day of the month), {mon} (month name),
     * {y} (year); a week's span with {d1} {mon1} and {d2} {mon2}. */
    const char *date_day;      /* "Thu, Oct 15" */
    const char *date_full;     /* "Oct 15, 2026" */
    const char *week_same;     /* "Oct 12 – 18" (one month) */
    const char *week_other;    /* "Sep 28 – Oct 4" (two months) */
    const Text *texts;         /* in any order; a key left out falls back to English */
    int count;
};

/* Every language, English first (the fallback); see languages.cpp. */
const Language *const *all_languages(int *n);

} // namespace i18n
