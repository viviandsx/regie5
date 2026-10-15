/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * config.json read again ("Test all", △, a phone that saved): Settings and
 * the welcome screens alike.
 */
#pragma once

#include "ui/context.h"

namespace ui {

/* Reads the file again. One that fails keeps the configuration in use and
 * says why (ctx.reload_error, or config_error if none ever worked). True when
 * a service changed: its pictures are forgotten, the other screens start
 * afresh (ctx.config_gen), a note says so. Nothing in --mock. */
bool reload_config(Context &ctx);

} // namespace ui
