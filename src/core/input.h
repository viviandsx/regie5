/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Input as the app sees it: one bit per pad button, whatever the platform
 * reads (the keyboard and SDL's game controller on Linux, scePad on the PS5).
 * The shape follows Jelly5's nuvio_input: presses this frame, buttons held,
 * and the D-pad's auto-repeat, computed here so every platform repeats alike.
 */
#pragma once

#include <cstdint>

namespace input {

enum Button : uint32_t {
    Up = 1u << 0,
    Down = 1u << 1,
    Left = 1u << 2,
    Right = 1u << 3,
    Cross = 1u << 4,      /* select */
    Circle = 1u << 5,     /* back */
    Triangle = 1u << 6,   /* refresh */
    Square = 1u << 7,
    L1 = 1u << 8,         /* previous tab */
    R1 = 1u << 9,         /* next tab */
    Options = 1u << 10,   /* context menu */
    L2 = 1u << 11,        /* a grid by title: previous letter; else previous page */
    R2 = 1u << 12,        /* next letter, or next page */

    Dpad = Up | Down | Left | Right,
};

struct State {
    uint32_t pressed = 0;   /* went down this frame (D-pad: also its auto-repeats) */
    uint32_t held = 0;      /* down now */
    uint32_t repeats = 0;   /* the part of pressed that is an auto-repeat */
};

/* Turns what the platform read into a State. Call once per frame. */
class Repeater {
public:
    /* went_down: every button that went down since the last call, even if it is
     * already back up (a quick tap between two frames); held: down now. */
    State update(uint32_t went_down, uint32_t held, double now);
    /* Whether a held direction will repeat: the loop must keep ticking. */
    bool waiting() const { return m_dir != 0; }

private:
    uint32_t m_dir = 0;   /* the direction being repeated */
    double m_next = 0;
};

} // namespace input
