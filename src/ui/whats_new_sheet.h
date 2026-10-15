/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * "What's new": a glass panel over the screens, with the version's few new
 * things (the language files' WnItem lines) and one button, OK. It opens by
 * itself once per version (core/whats_new), and from Settings.
 */
#pragma once

#include "core/input.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"

namespace ui {

class WhatsNewSheet {
public:
    explicit WhatsNewSheet(Context &ctx) : m_ctx(ctx) {}
    void open();
    bool is_open() const { return m_open; }
    void input(const input::State &in);
    void draw(float dt, bool *animating);

private:
    Context &m_ctx;
    bool m_open = false;
    Anim m_show;
    Drop m_drop;
};

} // namespace ui
