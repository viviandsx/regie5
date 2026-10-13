/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Presses and captures played in order (REGIE5_SCRIPT on Linux, the probe's
 * tour on the PS5): button names (up down left right cross circle triangle
 * square l1 r1 l2 r2 options), a number (seconds to wait), shot:<file.ppm>
 * (the next frame, saved) and quit. "1 right right 0.8 shot:/tmp/a.ppm quit".
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

class Script {
public:
    /* The frame drawn last, as RGB rows (a shot); false when it cannot. */
    using Grab = bool (*)(std::vector<uint8_t> *rgb, int *w, int *h);

    explicit Script(const char *text, Grab grab = nullptr);
    bool active() const { return m_next < m_steps.size() || !m_shot.empty(); }
    bool wants_frame() const { return !m_shot.empty(); }

    /* Before a frame: the presses due now. Sets *quit at "quit". */
    uint32_t step(double now, bool *quit);
    /* After a frame: saves it when a shot was asked for. */
    void after_frame();

private:
    static uint32_t button(const std::string &s);

    std::vector<std::string> m_steps;
    size_t m_next = 0;
    double m_wait_until = 0;
    std::string m_shot;
    Grab m_grab = nullptr;
};

} // namespace ui
