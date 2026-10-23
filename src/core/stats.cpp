/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/stats.h"

#include <cstdio>

namespace stats {

int days(Period p) { return p == Week ? 7 : p == Month ? 30 : 365; }

std::vector<Bar> bars(const model::PlayStats &s, Period p)
{
    std::vector<Bar> out;
    for (const auto &[date, seconds] : s.days) {   /* "yyyy-mm-dd", oldest first */
        Bar b;
        if (std::sscanf(date.c_str(), "%d-%d-%d", &b.y, &b.m, &b.d) != 3)
            continue;
        b.seconds = seconds;
        if (p != Year) {
            out.push_back(b);
            continue;
        }
        b.d = 0;
        if (!out.empty() && out.back().y == b.y && out.back().m == b.m)
            out.back().seconds += seconds;
        else
            out.push_back(b);
    }
    if (p == Year && out.size() > 12)
        out.erase(out.begin(), out.end() - 12);
    return out;
}

int64_t total_seconds(const model::PlayStats &s)
{
    int64_t t = 0;
    for (const auto &u : s.users)
        t += u.seconds;
    return t;
}

int total_plays(const model::PlayStats &s)
{
    int n = 0;
    for (const auto &u : s.users)
        n += u.plays;
    return n;
}

bool empty(const model::PlayStats &s) { return total_plays(s) == 0 && total_seconds(s) == 0; }

} // namespace stats
