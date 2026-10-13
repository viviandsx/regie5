/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/input.h"

namespace input {

namespace {
/* Held D-pad: first repeat after 400 ms, then every 90 ms (Jelly5's timing). */
constexpr double kRepeatDelay = 0.40;
constexpr double kRepeatEvery = 0.09;
/* What repeats when held: the directions, and L2/R2 (letters, pages). */
constexpr uint32_t kRepeating = Dpad | L2 | R2;
} // namespace

State Repeater::update(uint32_t went_down, uint32_t held, double now)
{
    State s;
    s.held = held;
    s.pressed = went_down;
    const uint32_t dir = held & kRepeating;
    if (went_down & kRepeating) {   /* a new direction starts its own wait */
        m_dir = went_down & kRepeating & held;
        m_next = now + kRepeatDelay;
    } else if (dir && dir == m_dir && now >= m_next) {
        s.repeats = dir;
        s.pressed |= dir;
        m_next = now + kRepeatEvery;
    } else if (dir != m_dir) {
        m_dir = 0;   /* released, or another direction joined: no repeat until a new press */
    }
    return s;
}

} // namespace input
