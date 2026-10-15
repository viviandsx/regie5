/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The Linux side of platform.h's text entry.
 */
#pragma once

#include <SDL.h>

#include <string>

namespace text_linux {

/* An event for the open entry: true when it took it (the interface does not see it). */
bool handle_event(const SDL_Event &ev);
/* A script's text:<words>: typed whole and confirmed (REGIE5_SCRIPT). */
void type(const std::string &text);

} // namespace text_linux
