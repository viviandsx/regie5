/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pieces the screens share: section titles, chips, progress bars, the
 * loading, empty and error states, the note that confirms an action, and the
 * sheet that asks before one.
 */
#pragma once

#include "core/input.h"
#include "ui/anim.h"
#include "ui/drop.h"
#include "ui/gfx.h"

#include <functional>
#include <string>
#include <vector>

namespace ui {

/* A loading row's stand-in: a pane that breathes. Sets *animating. */
void draw_skeleton(const gfx::Rect &r, float radius, double now, bool *animating);

/* A calm message where a list or row would be (nothing to show, or why not). */
void draw_empty(const gfx::Rect &r, const std::string &title, const std::string &detail = {}, bool error = false);

/* A small pill with a coloured dot and a label; returns its width. */
float draw_chip(float x, float y, const std::string &label, uint32_t dot, float opacity = 1.f, float size = 22.f);
float chip_width(const std::string &label, float size = 22.f);

/* A pane of pills (views or filters) at (x, y), or ending at x when
 * right_aligned. The drop sits on `on`, faint unless the bar has the focus
 * (focus). The chosen pill's label is bold. Returns each pill's rectangle. */
std::vector<gfx::Rect> draw_pills(float x, float y, const std::vector<std::string> &labels, int chosen, int on,
                                  bool focus, Drop &drop, float dt, bool *animating, bool right_aligned = false);
float pills_width(const std::vector<std::string> &labels);

/* A thin progress bar, value 0..1. */
void draw_progress(const gfx::Rect &r, float value, uint32_t color, float opacity = 1.f);

/* A note at the top of the screen for a few seconds (an action's outcome;
 * longer for something to remember, such as a title to look for). */
void notify(const std::string &text, bool ok, double now, double seconds = 3.0);
/* Drawn by the shell over everything. Sets *animating while shown. */
void draw_notes(double now, bool *animating);

/* A sheet over the screen that asks: a title, a line, buttons. It owns every
 * press until it closes. ○ closes it as the cancel choice (-1). */
class Sheet {
public:
    void open(std::string title, std::string body, std::vector<std::string> buttons, int focus,
              std::function<void(int)> chosen, bool danger_last = false);
    bool is_open() const { return m_open; }
    void input(const input::State &in);
    void draw(float dt, bool *animating);

private:
    bool m_open = false;
    std::string m_title, m_body;
    std::vector<std::string> m_buttons;
    int m_focus = 0;
    bool m_danger_last = false;
    std::function<void(int)> m_chosen;
    Anim m_show;
    Drop m_drop;
};

/* A sheet that lists things (the servers' alerts): each row with chips (a
 * label and the colour of its dot), a sentence and a quieter line under it.
 * ↑/↓ move through them; ○ or ✕ closes it. */
class ListSheet {
public:
    struct Item {
        std::vector<std::pair<std::string, uint32_t>> chips;
        std::string text, detail;
    };
    void open(std::string title, std::vector<Item> items, std::string empty_text);
    bool is_open() const { return m_open; }
    void input(const input::State &in);
    void draw(float dt, bool *animating);

private:
    bool m_open = false;
    std::string m_title, m_empty;
    std::vector<Item> m_items;
    int m_focus = 0;
    Anim m_show, m_scroll;
    Drop m_drop;
};

/* A vertical list's scroll: keeps the focused row (top y, height h, in list
 * coordinates) inside [top, bottom] of the screen with a margin. */
float list_scroll(Anim &a, float focus_y, float focus_h, float view_h, float list_h, float dt, bool *animating);

} // namespace ui
