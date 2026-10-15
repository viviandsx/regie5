/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The welcome screens (first set-up), over the whole interface: Welcome,
 * the media server (Jellyfin, Emby, none), the phone's page (its QR code and
 * code), then each service's test, as in Settings. "Later" on every screen.
 * They open by themselves on a first start (core/welcome), and from Settings
 * or Home on demand; leaving them, however, is remembered (settings.json).
 */
#pragma once

#include "core/input.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/phone_share.h"
#include "ui/service_rows.h"

#include <string>
#include <vector>

namespace ui {

class WelcomeFlow {
public:
    explicit WelcomeFlow(Context &ctx) : m_ctx(ctx) {}
    void open();
    bool is_open() const { return m_open; }
    /* Every press while open. */
    void input(const input::State &in);
    void draw(double now, float dt, bool *animating);
    /* config.json was read again while they were open (a phone saved, or
     * "Check"): the screens behind start afresh. Cleared by the call. */
    bool take_config_changed();

private:
    enum class Step { Intro, Server, Phone, Check };
    enum class Choice { Jellyfin, Emby, None };
    enum class Button { Start, Later, Jellyfin, Emby, None, Check, Retry, Go, Fix };
    std::vector<Button> buttons() const;
    void go(Step s);
    void press(Button b);
    void close();
    void open_phone();
    /* config.json read again and every service tested: the last screen. */
    void check();
    std::string media_query() const;

    Context &m_ctx;
    bool m_open = false;
    Step m_step = Step::Intro;
    Choice m_choice = Choice::Jellyfin;
    int m_focus = 0;
    bool m_config_changed = false;
    PhoneShare m_share;
    ServiceRows m_rows;
    Anim m_show, m_page;   /* the whole fading in; each screen sliding in */
    Drop m_drop;
};

} // namespace ui
