/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/schedule.h"

#include <algorithm>

namespace schedule {

int column_of(time_t when, time_t monday)
{
    for (int d = 0; d < 7; d++) {
        /* Each day's local midnight, through mktime (a DST change makes a day 23 or 25 h). */
        struct tm a;
        localtime_r(&monday, &a);
        a.tm_mday += d;
        a.tm_isdst = -1;
        const time_t from = mktime(&a);
        a.tm_mday += 1;
        a.tm_isdst = -1;
        const time_t to = mktime(&a);
        if (when >= from && when < to)
            return d;
    }
    return -1;
}

State state_of(const model::CalendarItem &c, time_t now)
{
    if (c.has_file)
        return Available;
    if (c.when > now)
        return Upcoming;
    return c.release == model::CalendarItem::Cinema ? InCinemas : Missing;
}

void drop_duplicates(std::vector<model::CalendarItem> *items)
{
    auto same = [](const model::CalendarItem &a, const model::CalendarItem &b) {
        return a.from == b.from && a.title == b.title && a.season == b.season && a.episode == b.episode &&
               a.release == b.release && a.when == b.when;
    };
    std::vector<model::CalendarItem> out;
    for (const auto &c : *items) {
        auto it = std::find_if(out.begin(), out.end(), [&](const model::CalendarItem &o) { return same(o, c); });
        if (it == out.end())
            out.push_back(c);
        else if (!it->has_file && c.has_file)
            *it = c;
    }
    items->swap(out);
}

} // namespace schedule
