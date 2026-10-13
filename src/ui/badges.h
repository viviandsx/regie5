/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Keeps the tabs' counters up to date while their screens are closed: every
 * 30 s, the queues of every Sonarr / Radarr, Seerr's pending requests and
 * Jellyfin's sessions (reads only). A screen that loads the same data posts
 * its count at once (Context::badges), so the two never disagree for long.
 */
#pragma once

#include "ui/context.h"

#include <memory>
#include <vector>

namespace ui {

class BadgeWatch {
public:
    explicit BadgeWatch(Context &ctx) : m_ctx(ctx) {}
    /* Called every turn of the loop; asks when it is time. */
    void tick(double now);

private:
    void ask_queues();

    Context &m_ctx;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
    unsigned m_gen = ~0u;   /* the config last asked with */
    double m_next = 0;
    /* The queues of one round: answered, and joined once all have. */
    struct Round {
        int waiting = 0, ok = 0;
        std::vector<model::QueueItem> items;
    };
    std::shared_ptr<Round> m_round;
    bool m_requests_out = false, m_sessions_out = false;
};

} // namespace ui
