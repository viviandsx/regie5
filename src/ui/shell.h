/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The app as the platform's loop sees it: input in, a frame out. Owns the tab
 * bar, the five screens, the focus between them and the ambient background.
 */
#pragma once

#include "core/input.h"
#include "ui/ambient.h"
#include "ui/anim.h"
#include "ui/badges.h"
#include "ui/context.h"
#include "ui/screen.h"
#include "ui/tabbar.h"
#include "ui/welcome_flow.h"
#include "ui/whats_new_sheet.h"

#include <memory>
#include <string>

namespace ui {

class Shell {
public:
    explicit Shell(Context &ctx);

    /* One frame's input. Returns true when it changed something to draw. */
    bool input(const input::State &in, double now);
    /* Draws a frame (between gfx::begin_frame and end_frame). Returns true
     * while something moves: the loop draws again at once. */
    bool draw(double now, float dt);

private:
    void switch_tab(int tab, double now);
    std::unique_ptr<Screen> make_screen(int tab);
    /* Every screen made again (config.json changed behind them). */
    void remake_screens(bool current_too);

    Context &m_ctx;
    unsigned m_config_gen = 0;   /* the config the screens were made with */
    ArrLists m_lists;            /* m_ctx.lists */

    std::unique_ptr<Screen> m_screens[TabCount];
    TabBar m_bar;
    BadgeWatch m_watch;
    Ambient m_ambient;
    int m_tab = TabHome;
    bool m_on_bar = false;   /* the focus is in the tab bar */
    Anim m_enter;            /* 0..1: the open tab sliding in */
    float m_dir = 1.f;       /* the way it slides in from */
    WelcomeFlow m_welcome;   /* the first set-up, over everything */
    WhatsNewSheet m_whats_new;   /* over the screens, once per version */
};

} // namespace ui
