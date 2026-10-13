/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The oldest version of each service Régie5 works with. Older ones get a
 * clear message in Settings instead of screens that half work. A version that
 * cannot be read (a development build's) is let through.
 */
#pragma once

#include "core/config.h"

#include <string>

namespace versions {

/* "4.0", "10.9"…; "" for a service with no minimum (Glances: its API path,
 * /api/4, already asks for Glances 4). */
const char *minimum(config::Service s);

/* <0, 0, >0 as a is older, the same, newer; numbers compared one by one
 * ("10.11.2" > "10.9"), missing ones as 0. false when either is not a
 * version (no leading digit). */
bool compare(const std::string &a, const std::string &b, int *order);

/* Whether this version of the service is one Régie5 works with. */
bool supported(config::Service s, const std::string &version);

} // namespace versions
