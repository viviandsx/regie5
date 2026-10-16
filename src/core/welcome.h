/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The welcome screens (first set-up): when they open by themselves. Only on
 * a first start, never over a configuration that works, so that someone
 * updating from 0.3.x never sees them.
 */
#pragma once

#include "core/config.h"

namespace welcome {

/* At least one of Sonarr, Radarr, Seerr and the media server is on (Glances
 * alone does not make a configuration). */
bool configured(const config::Config &cfg);

struct Start {
    config::LoadError error = config::LoadError::None;   /* config.json as read at start */
    bool from_backup = false;   /* the PS5 used the app's copy of config.json */
    bool mock = false;          /* --mock / --demo: the example data */
    bool later = false;         /* "Later" chosen once (settings.json) */
    bool forced = false;        /* REGIE5_WELCOME=1, to work on the screens */
};

/* Whether they open at start: forced, or a first start (no config.json
 * Régie5 can use: missing, empty, not JSON, or nothing on), unless "Later"
 * was chosen, the app's copy stood in, or the data is the example's; and
 * always when config.json was set aside (core/boot_guard). */
bool should_show(const Start &s, const config::Config &cfg);

} // namespace welcome
