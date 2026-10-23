/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Viewing statistics over a period, as the screen shows them: the bars (a
 * day each over 7 or 30 days, a month each over 12 months) and the totals.
 */
#pragma once

#include "core/model.h"

#include <cstdint>
#include <vector>

namespace stats {

enum Period { Week, Month, Year, PeriodCount };
/* The days asked of Playback Reporting: 7, 30, 365. */
int days(Period p);

struct Bar {
    int y = 0, m = 0, d = 0;   /* d 0: the month */
    int64_t seconds = 0;
};
/* Week and Month: each day the plugin gave; Year: the last 12 months (the
 * oldest, cut short by the 365 days, left out). */
std::vector<Bar> bars(const model::PlayStats &s, Period p);

int64_t total_seconds(const model::PlayStats &s);
int total_plays(const model::PlayStats &s);
/* Nothing watched in the period (a plugin just installed). */
bool empty(const model::PlayStats &s);

} // namespace stats
