/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The guide's rules, apart from any drawing (and so tested): which day of a
 * week an episode or a release falls on, and where it stands.
 */
#pragma once

#include "core/model.h"

#include <ctime>
#include <vector>

namespace schedule {

enum State { Available, Upcoming, Missing, InCinemas };

/* The column (0 Monday .. 6 Sunday) of when in the week starting at monday
 * (local midnight), or -1 outside it. By local calendar day. */
int column_of(time_t when, time_t monday);

/* Available: its file is there. Missing: aired or out, and no file. Upcoming:
 * not yet. InCinemas: a film's cinema release gone by without a file (it
 * cannot be had then: never "missing"). */
State state_of(const model::CalendarItem &c, time_t now);

/* The same episode or release seen by two instances (Radarr and Radarr 4K)
 * kept once: the copy that has its file if one does, else the first. Same
 * service, title, episode, kind of release and time. Order otherwise kept. */
void drop_duplicates(std::vector<model::CalendarItem> *items);

} // namespace schedule
