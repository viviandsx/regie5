/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The "What's new" screen: when it opens by itself. Once, at the first start
 * of a new version (major.minor: a fix such as 0.5.1 says nothing new),
 * never on a first install (the welcome screens are the ones then).
 */
#pragma once

#include <string>

namespace whats_new {

/* "0.5" for "0.5.0", "0.5.2-dev" or "0.5"; empty for anything else. */
std::string series(const std::string &version);

struct Start {
    std::string current;       /* the app's version */
    std::string seen;          /* settings.json "seenVersion": empty before 0.5.0 */
    bool ran_before = false;   /* the app ran on this machine before this start */
    bool welcome = false;      /* the welcome screens open at this start */
    bool mock = false;         /* --mock / --demo */
    bool forced = false;       /* REGIE5_WHATSNEW=1, for the screenshots */
};

/* Forced; else never in --mock, with the welcome screens, or on a first
 * start; else when the version's series differs from the one last seen
 * (none seen but the app ran before: an update from 0.4.x or earlier). */
bool should_show(const Start &s);

} // namespace whats_new
