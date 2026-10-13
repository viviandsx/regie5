/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Cards: a poster with its focus lift, and the generated card shown where a
 * title has no picture (or it failed) - never a broken image.
 */
#pragma once

#include "ui/gfx.h"

#include <cstdint>
#include <string>

namespace art {
struct Ref;
}

namespace ui {

/* A title without a picture: a gradient picked by seed, its name on it. */
void draw_generated_card(const gfx::Rect &r, const std::string &title, uint32_t seed, float radius, float opacity);

/* A 2:3 card at rest in base; lift (0..1) makes it larger with a shadow. */
void draw_title_card(const gfx::Rect &base, const std::string &title, uint32_t seed, float lift, float opacity);
void draw_poster_card(const gfx::Rect &base, const art::Ref &ref, const std::string &title, uint32_t seed, float lift,
                      float opacity);

/* A stable seed from a string (a title, an id). */
uint32_t seed_of(const std::string &s);

} // namespace ui
