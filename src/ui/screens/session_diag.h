/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A Jellyfin session's sheet, "why does it stutter?": how it plays and why,
 * the source against what is sent to the client, the hardware acceleration,
 * whether the transcode keeps ahead (an estimate, said so), advice in plain
 * words, and the server's load when Glances is set up. Stopping the playback
 * starts from here (and still asks).
 */
#pragma once

#include "core/diagnosis.h"
#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/drop.h"
#include "ui/load.h"

#include <string>

namespace ui {

class SessionDiag {
public:
    enum Result { None, Close, Stop };

    void open(const std::string &session_id);
    void close() { m_open = false; }
    bool is_open() const { return m_open; }
    const std::string &session_id() const { return m_id; }
    Result input(const input::State &in);
    /* s: the session (refreshed every few seconds); load: Glances, if set up. */
    void draw(const model::Session &s, const model::EncodingInfo &enc, const diagnosis::Pace &pace,
              const Load<model::ServerLoad> *load, float dt, bool *animating);

private:
    bool m_open = false;
    std::string m_id;
    int m_focus = 1;   /* 0 stop, 1 close */
    Anim m_show;
    Drop m_drop;
};

/* "CPU 71 %" ... chips for the server's load; returns the width drawn (right-aligned at right). */
float draw_server_load(float right, float y, const model::ServerLoad &l);

} // namespace ui
