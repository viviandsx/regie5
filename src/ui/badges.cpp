/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/badges.h"

namespace ui {

namespace {
constexpr double kEvery = 30.0;   /* seconds between two rounds */
constexpr double kFirst = 2.0;    /* after the start: the first screen's own data goes first */
} // namespace

void BadgeWatch::tick(double now)
{
    if (!m_ctx.services)
        return;
    if (m_gen != m_ctx.config_gen) {   /* another config.json: the old counts say nothing */
        m_gen = m_ctx.config_gen;
        m_alive = std::make_shared<bool>(true);   /* answers to the old one are dropped */
        m_round.reset();
        m_requests_out = m_sessions_out = false;
        for (badges::Badge &b : m_ctx.badges)
            b = {};
        m_next = now + kFirst;
    }
    if (now < m_next)
        return;
    m_next = now + kEvery;
    std::weak_ptr<bool> alive = m_alive;
    api::Services &svc = *m_ctx.services;

    if (!m_round)
        ask_queues();
    if (!m_requests_out) {
        m_requests_out = true;
        svc.requests(api::Services::RequestFilter::Pending, [this, alive](api::Result<model::RequestPage> r) {
            if (alive.expired())
                return;
            m_requests_out = false;
            m_ctx.badges[TabRequests] = r.ok() ? badges::requests(r.value) : badges::Badge{};
        });
    }
    if (!m_sessions_out) {
        m_sessions_out = true;
        svc.sessions([this, alive](api::Result<std::vector<model::Session>> r) {
            if (alive.expired())
                return;
            m_sessions_out = false;
            m_ctx.badges[TabJellyfin] = r.ok() ? badges::sessions(r.value) : badges::Badge{};
        });
    }
}

void BadgeWatch::ask_queues()
{
    api::Services &svc = *m_ctx.services;
    auto round = std::make_shared<Round>();
    for (config::Service s : {config::Sonarr, config::Radarr})
        round->waiting += svc.instances(s);
    if (round->waiting == 0)
        return;
    m_round = round;
    std::weak_ptr<bool> alive = m_alive;
    for (config::Service s : {config::Sonarr, config::Radarr})
        for (int i = 0; i < svc.instances(s); i++)
            svc.queue(
                s,
                [this, alive, round](api::Result<std::vector<model::QueueItem>> r) {
                    if (alive.expired())
                        return;
                    if (r.ok()) {
                        round->ok++;
                        round->items.insert(round->items.end(), r.value.begin(), r.value.end());
                    }
                    if (--round->waiting > 0)
                        return;
                    /* Every instance answered: a count from those that could. */
                    m_ctx.badges[TabDownloads] = round->ok ? badges::queue(round->items) : badges::Badge{};
                    m_round.reset();
                },
                i);
}

} // namespace ui
