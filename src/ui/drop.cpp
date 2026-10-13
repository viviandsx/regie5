/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jelly5's ui::Drop (app/src/ui/screen.cpp, GPL-3.0-or-later).
 */
#include "ui/drop.h"

#include "ui/style.h"

#include <algorithm>
#include <cmath>

namespace ui {

void Drop::to(const gfx::Rect &abs, int key, float ox, float oy)
{
    const gfx::Rect r{abs.x - ox, abs.y - oy, abs.w, abs.h};
    m_ox = ox, m_oy = oy;
    if (!m_shown || !m_placed) {   /* appearing: in place, popping in */
        m_x = r.x, m_y = r.y, m_w = r.w, m_h = r.h;
        m_vx = m_vy = m_vw = m_vh = 0;
        m_pop = -0.35f, m_vpop = 0;
        m_placed = true;
    } else if (key != m_key) {
        m_vpop += 2.2f;   /* a nudge: it swells as it leaves */
    }
    m_key = key;
    m_target = r;
    m_shown = true;
}

void Drop::draw(float dt, float a, bool *animating, float radius)
{
    if (!m_shown) {
        m_placed = false;
        return;
    }
    for (float left = std::min(dt, 0.05f); left > 0; left -= 1.f / 240) {   /* small steps: stable */
        const float h = std::min(left, 1.f / 240);
        auto spring = [h](float &v, float &x, float to) {
            v += (260.f * (to - x) - 19.f * v) * h;
            x += v * h;
        };
        spring(m_vx, m_x, m_target.x);
        spring(m_vy, m_y, m_target.y);
        spring(m_vw, m_w, m_target.w);
        spring(m_vh, m_h, m_target.h);
        m_vpop += (300.f * -m_pop - 14.f * m_vpop) * h;
        m_pop += m_vpop * h;
    }
    const bool moving = std::fabs(m_target.x - m_x) + std::fabs(m_target.y - m_y) + std::fabs(m_target.w - m_w) +
                                std::fabs(m_target.h - m_h) > 0.4f ||
                        std::fabs(m_vx) + std::fabs(m_vy) > 3.f || std::fabs(m_pop) > 0.003f ||
                        std::fabs(m_vpop) > 0.05f;
    if (moving) {
        if (animating)
            *animating = true;
    } else {
        m_x = m_target.x, m_y = m_target.y, m_w = m_target.w, m_h = m_target.h;
        m_vx = m_vy = m_vw = m_vh = m_pop = m_vpop = 0;
    }
    /* Stretch along the way it moves, thin across it; swell with the pop. In
     * pixels, not in proportion: a wide row must not swell by a hundred pixels. */
    const float sx = std::min(1.f, std::fabs(m_vx) / 1600.f), sy = std::min(1.f, std::fabs(m_vy) / 1600.f);
    const float w = m_w + 10.f * m_pop + 40.f * sx - 6.f * sy;
    const float hh = m_h + 6.f * m_pop + 24.f * sy - std::min(10.f, m_h * 0.14f) * sx;
    const float grow = 1.f + 6.f * m_pop / std::max(1.f, m_h);
    const float cx = m_ox + m_x + m_w / 2, cy = m_oy + m_y + m_h / 2;
    m_drawn = {cx - w / 2, cy - hh / 2, w, hh};
    glass_panel(m_drawn, radius < 0 ? std::min(w, hh) / 2 : radius * grow, a, false, 1.f);
}

} // namespace ui
