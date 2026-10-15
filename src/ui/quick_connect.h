/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Quick Connect: Jellyfin without an API key. Jellyfin gives a code, shown
 * here; the viewer approves it in Jellyfin on their phone (their profile ›
 * Quick Connect) and Régie5 gets that account's session, written into
 * config.json ("accessToken", "userName", "isAdmin"; nothing in --mock). An
 * account that is not an administrator is accepted: the screens that need
 * one say so. Never for Emby (it has no such thing on the local network).
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"

#include <memory>
#include <string>

namespace ui {

class QuickConnectSheet {
public:
    explicit QuickConnectSheet(Context &ctx) : m_ctx(ctx) {}
    /* Offered for this configuration: Jellyfin (not Emby) has an address. */
    static bool offered(const Context &ctx);
    void open();
    bool is_open() const { return m_open; }
    void input(const input::State &in);
    void draw(float dt, bool *animating);
    /* A session was saved since the last call (the rows are tested again). */
    bool take_signed_in();

private:
    enum class Step { Checking, Code, Done, Failed };
    void start();
    void fail(const std::string &why);
    void finish(const model::JellyfinSession &s);

    Context &m_ctx;
    bool m_open = false;
    Step m_step = Step::Checking;
    model::QuickConnect m_code;
    std::string m_message;
    bool m_ok = true;
    double m_next_poll = 0;
    bool m_polling = false;
    unsigned m_gen = 0;   /* a newer start wins over an older answer */
    int m_focus = 0;
    bool m_signed_in = false;
    Anim m_show;
    Drop m_drop;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
