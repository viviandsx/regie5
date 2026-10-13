/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Controller hints, drawn the way the PS5's own hints look (shapes of our own,
 * not Sony's artwork): the button on a dark disc (Options and the shoulder
 * buttons as a pill), then the label. Jelly5's (app/src/ui/screen.cpp,
 * GPL-3.0-or-later).
 */
#pragma once

#include <string>
#include <vector>

namespace ui {

enum class PadButton { Cross, Circle, Triangle, Square, Options, L1, R1, L2, R2 };

struct PadHint {
    PadButton button;
    std::string label;
};

/* x: the left edge, cy: the vertical centre, size: the disc's height. Returns the width. */
float draw_pad_hint(float x, float cy, PadButton b, const std::string &label, float size = 30.f,
                    float opacity = 1.f);
float pad_hint_width(PadButton b, const std::string &label, float size = 30.f);
/* A row of hints; align 0 left of x, 1 centred on x, 2 ending at x. Returns the width. */
float draw_pad_hints(float x, float cy, const std::vector<PadHint> &hints, int align = 0, float size = 28.f,
                     float opacity = 1.f);

} // namespace ui
