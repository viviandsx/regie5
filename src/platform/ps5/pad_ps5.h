/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The DualSense through scePad, as core/input's bits: what went down since
 * the last read, and what is held (the left stick counts as the D-pad). The
 * sample layout and the opening sequence follow ps5-homebrew-ui's
 * platform/ps5/pad.cpp (Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later).
 */
#pragma once

#include <cstdint>

namespace ps5 {

class Pad {
public:
    ~Pad() { close(); }
    bool open();
    void close();
    bool is_open() const { return m_handle >= 0; }
    /* Reads the pad: *went_down since the last call, *held now. */
    void read(uint32_t *went_down, uint32_t *held);

private:
    int m_handle = -1;
    int m_user = -1;
    uint32_t m_held = 0;
};

} // namespace ps5
