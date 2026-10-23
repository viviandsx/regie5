/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What every tab's screen provides to the shell (after Jelly5's ui::Screen).
 */
#pragma once

#include "core/input.h"
#include "ui/art.h"
#include "ui/gfx.h"
#include "ui/hints.h"

#include <ctime>
#include <string>
#include <vector>

namespace ui {

enum class Action {
    None,
    ToTabs,   /* the focus goes up into the tab bar */
    Bump,     /* the press ran into an edge */
};

class Screen {
public:
    virtual ~Screen() = default;
    /* The tab was opened. */
    virtual void activate() {}
    virtual Action input(const input::State &in) = 0;
    /* ox: how far the screen is slid (a tab change); opacity is pushed by the shell. */
    virtual void draw(double now, float dt, float ox, bool *animating) = 0;
    /* The tab bar has the focus, not this screen: it shows no focus of its own. */
    void set_focused(bool f) { m_focused = f; }
    /* The focus came down from the tab bar. */
    virtual void enter_from_top() {}
    /* Another screen sends the user here: a view of this one, at a time (the
     * Home row's ✕ opens the Library's guide on that week), or on an item (a
     * disk's sheet, from Home's alerts). */
    virtual void go_to(int view, time_t when, const std::string &item) {}
    /* The background for what has the focus now: key names it, ref is the
     * picture (empty: the generated one for seed). false: the shell's. */
    virtual bool backdrop(std::string *key, art::Ref *ref, int *seed) const { return false; }
    /* A sheet is open over the screen: it gets every press (no tab change). */
    virtual bool modal() const { return false; }
    /* Hints of its own, before the common ones (✕ ○ L1/R1 △). */
    virtual std::vector<PadHint> hints() const { return {}; }
    /* Drawn over everything else (a sheet). */
    virtual void draw_overlay(float dt, bool *animating) {}

protected:
    bool m_focused = true;
};

} // namespace ui
