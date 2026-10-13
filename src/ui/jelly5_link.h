/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Options › « Ouvrir dans Jelly5 »: one for the whole app (in the Context).
 * It knows the Jellyfin server's Id and which item each film and series is
 * (both read once, when first needed), and starts Jelly5 on an item through
 * the platform. A note tells what came of it. Where the platform starts no
 * other app (a console that refuses it), the entry becomes « Chercher dans
 * Jelly5 » and the note says which title to look for there.
 */
#pragma once

#include "core/model.h"
#include "ui/load.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class Jelly5Link {
public:
    /* The entry may be offered at all: Jellyfin is on. */
    bool available(const api::Services *services) const;
    /* Its words: « Ouvrir dans Jelly5 », or « Chercher dans Jelly5 » when no
     * app can be started from here. */
    const char *label() const;
    /* Reads the server's Id and the items' ids, unless done or on their way. */
    void prepare(api::Services *services);
    /* A library title: false once it is known that Jellyfin does not have it
     * (until then, and if Jellyfin could not say, the entry is offered). */
    bool may_have(const model::Movie &m) const;
    bool may_have(const model::Series &s) const;

    /* Starts Jelly5 on an item (its title for the note). */
    void open(api::Services *services, const std::string &item_id, const std::string &title);
    /* A library title: its item found first. */
    void open(api::Services *services, const model::Movie &m);
    void open(api::Services *services, const model::Series &s);

    /* config.json changed: what was read came from the old server. */
    void reset();

private:
    void when_ready(api::Services *services, std::function<void()> then);
    void launch(const std::string &item_id, const std::string &title);
    static void look_for(const std::string &title);
    static constexpr double kLookForSeconds = 8.0;
    void settle();

    Load<std::string> m_server;
    Load<std::vector<model::UserItem>> m_index;
    std::vector<std::function<void()>> m_waiting;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
