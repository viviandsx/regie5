/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The look shared by every screen: colours, the frosted glass pane and the
 * per-card focus lift. Colours and glass after Jelly5 (app/src/ui/screen.*,
 * GPL-3.0-or-later).
 */
#pragma once

#include "ui/anim.h"
#include "ui/gfx.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ui {

constexpr float kPad = 120;   /* the screen's side margin */
constexpr uint32_t kBg = 0xff07070a;
constexpr uint32_t kText = 0xfff5f5f7;
constexpr uint32_t kText2 = 0xadebebf5;
constexpr uint32_t kText3 = 0x6bebebf5;
constexpr uint32_t kGreen = 0xff30d158;
constexpr uint32_t kRed = 0xffff453a;
constexpr uint32_t kOrange = 0xffff9f0a;

/* The glyphs most screens use, rasterised at start in the most used styles,
 * so that opening a screen does not rasterise a hundred of them in one frame. */
void prewarm_text();

/* c with its alpha multiplied by a. */
uint32_t alpha(uint32_t c, float a);

/* Frosted glass: a soft shadow, what lies under r bent and softened by the
 * liquid glass shader, or a tint when the backend could not. lift: 0 a pane,
 * 1 the brighter glass of the focus drop (no blur pass of its own: it sits
 * on glass that already bends the picture). */
void glass_panel(const gfx::Rect &r, float radius, float opacity = 1.f, bool shadow = true, float lift = 0.f);

/* A small piece of glass without a blur (tint, sheen, lit rim): for the many
 * (cards, chips) where a blur each would cost too much. */
void glass_flat(const gfx::Rect &r, float radius, float opacity = 1.f);

/* Per-card focus lift (0..1), eased; keyed by whatever identifies the card. */
class Lifts {
public:
    float step(const std::string &key, bool focused, float dt, bool *animating);

private:
    std::unordered_map<std::string, Anim> m_lift;
};

/* A row of row_w in room: how far it has slid (eased) to keep the focused item,
 * centred at c in the row, in the middle of the room; 0 while the row fits. */
float row_scroll(Anim &a, float row_w, float room, float c, float dt, bool *animating);

} // namespace ui
