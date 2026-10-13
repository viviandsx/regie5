/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The keyboard and game controllers (a DualSense over USB or Bluetooth) through
 * SDL, as input::Button bits.
 *
 *   arrows / D-pad / left stick   Up Down Left Right
 *   Enter / Cross                 Cross
 *   Esc, Backspace / Circle       Circle
 *   Q / L1, E / R1                L1, R1
 *   R / Triangle                  Triangle
 *   M / Options                   Options
 *   Z / L2, C / R2                L2, R2
 */
#pragma once

#include <SDL.h>

#include <cstdint>

namespace input_sdl {

/* Opens the controllers already plugged in (later ones come as events). */
void init();
void shutdown();
/* Every SDL event goes through here. */
void handle_event(const SDL_Event &ev);
/* The buttons that went down since the last call. */
uint32_t take_went_down();
/* The buttons down now. */
uint32_t held();

} // namespace input_sdl
