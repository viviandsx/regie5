/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The focus drop: the one focus marker for controls (tabs, buttons, rows), a
 * drop of brighter liquid glass. One per group of controls: give it the
 * focused control's rect every frame, draw it over the controls, then the
 * focused label crisp on top. It follows on an underdamped spring - it
 * overshoots and settles back - stretches along its speed, thins as it does,
 * and swells when it sets off for a new control.
 *
 * Jelly5's ui::Drop (app/src/ui/screen.cpp, GPL-3.0-or-later), its constants kept.
 */
#pragma once

#include "ui/gfx.h"

namespace ui {

class Drop {
public:
    /* key: identifies the focused control; a new key nudges the swell. ox, oy:
     * where the group is (it may scroll or slide): the drop springs within the
     * group and moves with it rigidly. */
    void to(const gfx::Rect &r, int key, float ox = 0, float oy = 0);
    void hide() { m_shown = false; }
    /* Steps the springs (sets *animating while it moves) and draws the drop.
     * radius < 0: a pill (half its height). */
    void draw(float dt, float opacity, bool *animating, float radius = -1.f);
    /* Where it is drawn now, for a label that rides on it. */
    gfx::Rect rect() const { return m_drawn; }

private:
    float m_x = 0, m_y = 0, m_w = 0, m_h = 0, m_vx = 0, m_vy = 0, m_vw = 0, m_vh = 0;
    float m_pop = 0, m_vpop = 0;
    gfx::Rect m_target{0, 0, 0, 0}, m_drawn{0, 0, 0, 0};   /* m_target: within the group */
    float m_ox = 0, m_oy = 0;
    int m_key = -1;
    bool m_shown = false, m_placed = false;
};

} // namespace ui
