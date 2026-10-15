/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Configuring from a phone: the page on the local network (platform.h,
 * share_*) and its QR code, for Settings and the welcome screens. One server
 * at a time: opening it here stops it wherever else it was open.
 */
#pragma once

#include "platform/platform.h"
#include "ui/context.h"

#include <string>
#include <vector>

namespace ui {

class PhoneShare {
public:
    ~PhoneShare() { close(); }
    /* Starts the page. query: added to the QR code's address ("m=emby": the
     * page shows that media server only). In --mock nothing listens: an
     * example address and code are shown (demo). False when it could not
     * start (error() says why). */
    bool open(const Context &ctx, const std::string &query = {});
    void close();
    bool is_open() const { return m_open; }
    bool demo() const { return m_demo; }
    /* The phone saved a configuration since the last call. */
    bool saved();
    int saves() const { return m_saves; }
    /* Why it is not serving ("bind: Permission denied"...), "" while it is. */
    std::string error() const;
    platform::ShareInfo info() const;
    /* The QR code on white, with its quiet zone, in a square of side s. */
    void draw_qr(float x, float y, float s, float a) const;
    bool has_qr() const { return m_qr_size > 0; }

private:
    bool m_open = false, m_demo = false;
    std::string m_error;
    int m_saves = 0;            /* configurations the phone saved, seen */
    std::vector<bool> m_qr;     /* the QR code's modules, row by row */
    int m_qr_size = 0;
    platform::ShareInfo m_demo_info;
};

} // namespace ui
