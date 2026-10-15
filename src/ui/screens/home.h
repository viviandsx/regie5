/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Accueil: the servers' health (disks and alerts), what is coming in the next
 * 14 days (Sonarr and Radarr calendars) and what was added lately (Jellyfin).
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class HomeScreen : public Screen {
public:
    explicit HomeScreen(Context &ctx);

    void activate() override;
    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override { m_row = 1; }
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_alerts.is_open() || m_menu.is_open(); }
    void draw_overlay(float dt, bool *animating) override
    {
        m_alerts.draw(dt, animating);
        m_menu.draw(dt, animating);
    }
    std::vector<PadHint> hints() const override;

    struct Card {
        std::string key;
        art::Ref poster, backdrop;
        std::string title, line1, line2;
        uint32_t seed = 0;
        time_t when = 0;   /* upcoming: its date (✕ opens the guide's week) */
        std::string jellyfin_id;   /* recent: the item Jelly5 opens */
    };

private:
    /* Nothing set up yet (no usable config.json, or no service on): Home is
     * a card that opens the welcome screens. Never in --mock. */
    bool unconfigured() const;
    void draw_unconfigured(float x0, float dt, bool *animating);
    struct Row {
        std::vector<Card> cards;
        int focus = 0;
        Anim scroll;
        Lifts lifts;
    };
    void reload();
    void rebuild_upcoming();
    void rebuild_recent();
    void draw_banner(float x0, float y, float dt, bool *animating);
    void draw_row(int index, Row &row, float x0, float y, double now, float dt, bool *animating);
    void open_alerts();
    /* The recent card with the focus, when Options has something for it. */
    const Card *menu_card() const;
    void open_menu();

    Context &m_ctx;
    bool m_loaded = false;
    /* Each instance of Sonarr and Radarr (config.json may list several). */
    struct Source {
        config::Service service;
        int instance;
        Load<std::vector<model::CalendarItem>> calendar;
        Load<std::vector<model::Health>> health;
        Load<std::vector<model::DiskSpace>> disks;
    };
    std::vector<Source> m_sources;
    /* "Radarr 4K": the instance's name (the service's when it has only one). */
    const std::string &name_of(const Source &src) const;
    Load<std::vector<model::RecentItem>> m_recent;
    Row m_rows[2];   /* upcoming, recent */
    int m_row = 1;   /* 0 the health banner, 1 upcoming, 2 recent */
    Anim m_page;
    Drop m_drop;
    ListSheet m_alerts;
    Sheet m_menu;   /* Options */
    /* When each alert was first seen while the app ran ("service|source|level"):
     * the servers do not say since when. */
    std::map<std::string, time_t> m_seen;
    std::map<std::string, model::Health> m_health_seen;   /* the alert behind each key (for "gone") */
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
