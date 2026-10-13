/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The top bar: the wordmark, five tabs on a pill of glass, Settings as a gear
 * on its own piece of glass, the focus drop moving between them, and the clock. After Jelly5's ui::Nav (app/src/ui/nav.cpp, GPL-3.0-or-later).
 */
#pragma once

#include "core/badges.h"
#include "ui/anim.h"
#include "ui/drop.h"

namespace ui {

/* The pill's tabs, then Settings (the gear). */
enum Tab { TabHome = 0, TabLibrary, TabDownloads, TabRequests, TabJellyfin, TabSettings, TabCount };

class TabBar {
public:
    /* active: the open tab; focused: whether the bar has the focus (the drop is
     * full then, faint while it only marks the open tab). badges: a counter
     * per tab (TabCount of them), shown when above 0. */
    void draw(float opacity, int active, bool focused, const badges::Badge *badges, float dt, bool *animating);

private:
    void draw_badge(float x, float y, int tab, const badges::Badge &b, float opacity, float dt, bool *animating);

    Drop m_drop;
    /* Each counter grows in and fades out; the last one shown stays while it fades. */
    Anim m_badge_in[TabCount];
    badges::Badge m_badge_last[TabCount];
};

const char *tab_label(int tab);

} // namespace ui
