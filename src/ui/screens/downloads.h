/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Activité: two views on one row of pills. File d'attente: Sonarr's and
 * Radarr's queues in one list, read-only - the title, the episode, the
 * progress, the time left, the quality, the status. Never a release, a client
 * or an indexer (the model has none to show). Refreshes every 5 seconds while
 * it is on screen. Journal: the last 14 days (ui/screens/journal), with its
 * filters on the right of the same row.
 */
#pragma once

#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/screens/journal.h"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class DownloadsScreen : public Screen {
public:
    explicit DownloadsScreen(Context &ctx);

    void activate() override;
    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override
    {
        m_bar = true;
        m_bar_focus = m_view;
    }
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    std::vector<PadHint> hints() const override;

    enum View { Queue = 0, Journal, ViewCount };

private:
    void refresh();
    void merge();
    static std::string key_of(const model::QueueItem &q);

    Context &m_ctx;
    /* One queue per instance of Sonarr and Radarr (config.json may list several). */
    struct Source {
        config::Service service;
        int instance;
        Load<std::vector<model::QueueItem>> queue;
    };
    std::vector<Source> m_queues;
    std::vector<model::QueueItem> m_items;
    int m_focus = 0;
    std::string m_focus_key;
    double m_last = -100;
    Anim m_scroll;
    Drop m_drop, m_view_drop, m_filter_drop;
    View m_view = Queue;
    bool m_bar = false;     /* the focus is on the row of pills */
    int m_bar_focus = 0;    /* the views, then (the journal) its filters */
    JournalView m_journal;
    void draw_queue(double now, float x0, float chips_x, float dt, bool *animating);
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
