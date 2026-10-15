/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/shell.h"

#include "core/i18n.h"
#include "ui/hints.h"
#include "ui/art.h"
#include "ui/screens/downloads.h"
#include "ui/screens/home.h"
#include "ui/screens/jellyfin_screen.h"
#include "ui/screens/library.h"
#include "ui/screens/requests.h"
#include "ui/screens/settings.h"
#include "ui/style.h"
#include "ui/widgets.h"

namespace ui {

Shell::Shell(Context &ctx) : m_ctx(ctx), m_config_gen(ctx.config_gen), m_watch(ctx), m_welcome(ctx)
{
    static const char *const kMade[TabCount] = {"screen made: Home",     "screen made: Library",
                                                "screen made: Activity", "screen made: Requests",
                                                "screen made: Jellyfin", "screen made: Settings"};
    for (int t = 0; t < TabCount; t++) {
        m_screens[t] = make_screen(t);
        if (m_ctx.trace)
            m_ctx.trace(kMade[t]);
    }
    m_enter.snap(1.f);
    if (m_ctx.trace)
        m_ctx.trace("opening the first screen (its data is asked for)");
    m_screens[m_tab]->activate();
    if (m_ctx.trace)
        m_ctx.trace("first screen opened");
}

std::unique_ptr<Screen> Shell::make_screen(int tab)
{
    switch (tab) {
    case TabHome: return std::make_unique<HomeScreen>(m_ctx);
    case TabLibrary: return std::make_unique<LibraryScreen>(m_ctx);
    case TabDownloads: return std::make_unique<DownloadsScreen>(m_ctx);
    case TabRequests: return std::make_unique<RequestsScreen>(m_ctx);
    case TabJellyfin: return std::make_unique<JellyfinScreen>(m_ctx);
    default: return std::make_unique<SettingsScreen>(m_ctx);
    }
}

void Shell::remake_screens(bool current_too)
{
    m_config_gen = m_ctx.config_gen;
    m_ctx.jelly5.reset();
    for (int t = 0; t < TabCount; t++)
        if (current_too || t != m_tab)
            m_screens[t] = make_screen(t);
    if (current_too)
        m_screens[m_tab]->activate();
}

void Shell::switch_tab(int tab, double now)
{
    if (tab == m_tab || tab < 0 || tab >= TabCount)
        return;
    m_dir = tab > m_tab ? 1.f : -1.f;
    m_tab = tab;
    m_enter.snap(0.f);
    m_enter.to(1.f);
    m_screens[m_tab]->activate();
    if (!m_on_bar)
        m_screens[m_tab]->enter_from_top();
}

bool Shell::input(const input::State &in, double now)
{
    m_watch.tick(now);   /* every turn, pressed or not */
    const uint32_t p = in.pressed;
    if (!p)
        return false;
    if (m_welcome.is_open()) {   /* the welcome screens have every press */
        m_welcome.input(in);
        if (!m_welcome.is_open()) {   /* left: Home, made afresh if config.json changed */
            if (m_welcome.take_config_changed() || m_ctx.config_gen != m_config_gen)
                remake_screens(true);
            m_on_bar = false;
            switch_tab(TabHome, now);
            m_screens[m_tab]->enter_from_top();
        }
        return true;
    }
    if (m_screens[m_tab]->modal()) {   /* a sheet has every press */
        m_screens[m_tab]->input(in);
        return true;
    }
    /* L1 / R1 change tab from anywhere. */
    if (p & input::L1) {
        switch_tab(m_tab - 1, now);
        return true;
    }
    if (p & input::R1) {
        switch_tab(m_tab + 1, now);
        return true;
    }
    if (m_on_bar) {
        if (p & input::Left)
            switch_tab(m_tab - 1, now);
        else if (p & input::Right)
            switch_tab(m_tab + 1, now);
        else if (p & (input::Down | input::Cross)) {
            m_on_bar = false;
            m_screens[m_tab]->enter_from_top();
        }
        return true;
    }
    if (m_screens[m_tab]->input(in) == Action::ToTabs)
        m_on_bar = true;
    if (m_ctx.go.tab >= 0) {   /* a screen sends the user elsewhere */
        const Context::Goto go = m_ctx.go;
        m_ctx.go = {};
        m_on_bar = false;
        switch_tab(go.tab, now);
        m_screens[m_tab]->go_to(go.view, go.when);
    }
    /* config.json changed a service: the other tabs start afresh (their data
     * came from the old addresses; answers still on their way are dropped
     * with the screens that asked). */
    if (m_ctx.config_gen != m_config_gen)
        remake_screens(false);
    return true;
}

bool Shell::draw(double now, float dt)
{
    bool animating = false;
    art::frame();
    if (m_ctx.welcome_request) {   /* at start (core/welcome), or Settings / Home asked */
        m_ctx.welcome_request = false;
        m_welcome.open();
    }
    if (m_welcome.is_open()) {   /* over everything: the backdrop, the screens, then the hints */
        m_ambient.set("welcome", {}, 7, now);
        m_ambient.draw(dt, 0.45f, &animating);
        m_welcome.draw(now, dt, &animating);
        draw_pad_hints(gfx::W - kPad, gfx::H - 52,
                       {PadHint{PadButton::Cross, i18n::tr(i18n::HintSelect)},
                        PadHint{PadButton::Circle, i18n::tr(i18n::HintBack)}},
                       2, 28.f, 0.9f);
        draw_notes(now, &animating);
        return animating;
    }
    Screen &scr = *m_screens[m_tab];
    scr.set_focused(!m_on_bar);

    std::string key;
    art::Ref ref;
    int seed = m_tab;
    if (!scr.backdrop(&key, &ref, &seed)) {
        key = "tab:" + std::to_string(m_tab);
        ref = {};
        seed = m_tab;
    }
    m_ambient.set(key, ref, seed, now);
    m_ambient.draw(dt, 0.45f, &animating);

    /* The open tab slides in a little and fades in. */
    if (m_enter.step(dt, 10.f))
        animating = true;
    const float e = smoothstep(m_enter.value);
    gfx::push_opacity(e);
    scr.draw(now, dt, (1.f - e) * 80.f * m_dir, &animating);
    gfx::pop_opacity();

    /* Content that scrolls under the tab bar or the hints fades into the background. */
    gfx::fill_vgradient({0, 0, gfx::W, 170}, alpha(kBg, 0.92f), alpha(kBg, 0.f));
    gfx::fill_vgradient({0, gfx::H - 150, gfx::W, 150}, alpha(kBg, 0.f), alpha(kBg, 0.92f));
    m_bar.draw(1.f, m_tab, m_on_bar, m_ctx.badges, dt, &animating);

    std::vector<PadHint> hints = scr.hints();
    for (PadHint h : {PadHint{PadButton::Cross, i18n::tr(i18n::HintSelect)},
                      PadHint{PadButton::Circle, i18n::tr(i18n::HintBack)}, PadHint{PadButton::L1, ""},
                      PadHint{PadButton::R1, i18n::tr(i18n::HintTabs)},
                      PadHint{PadButton::Triangle, i18n::tr(i18n::HintRefresh)}})
        hints.push_back(h);
    draw_pad_hints(gfx::W - kPad, gfx::H - 52, hints, 2, 28.f, 0.9f);
    scr.draw_overlay(dt, &animating);
    draw_notes(now, &animating);
    return animating;
}

} // namespace ui
