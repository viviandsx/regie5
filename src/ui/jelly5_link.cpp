/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/jelly5_link.h"

#include "core/i18n.h"
#include "core/jelly5.h"
#include "platform/platform.h"
#include "ui/format.h"
#include "ui/widgets.h"

#include <ctime>

namespace ui {

bool Jelly5Link::available(const api::Services *services) const
{
    return services && services->enabled(config::Jellyfin);
}

bool Jelly5Link::can_open(const api::Services *services)
{
    return platform::can_launch_apps() && services && services->config().media == config::MediaServer::Jellyfin;
}

const char *Jelly5Link::label(const api::Services *services) const
{
    return i18n::tr(can_open(services) ? i18n::OpenInJelly5 : i18n::FindInJelly5);
}

/* The console starts no other app: the title to look for, long enough to read. */
void Jelly5Link::look_for(const std::string &title)
{
    notify(fmt::f(i18n::tr(i18n::Jelly5LookFor), title.c_str()), true, platform::now(), kLookForSeconds);
}

void Jelly5Link::prepare(api::Services *services)
{
    if (!available(services))
        return;
    std::weak_ptr<bool> alive = m_alive;
    if (m_server.state == load::Idle || m_server.state == load::Failed) {
        m_server.state = load::Loading;
        services->server_id([this, alive](api::Result<std::string> r) {
            if (alive.expired())
                return;
            m_server.take(std::move(r), platform::now());
            settle();
        });
    }
    if (m_index.state == load::Idle || m_index.state == load::Failed) {
        m_index.state = load::Loading;
        services->jellyfin_index([this, alive](api::Result<std::vector<model::UserItem>> r) {
            if (alive.expired())
                return;
            m_index.take(std::move(r), platform::now());
            settle();
        });
    }
}

bool Jelly5Link::may_have(const model::Movie &m) const
{
    return !m_index.ready() || !jelly5::item_for(m, m_index.value).empty();
}

bool Jelly5Link::may_have(const model::Series &s) const
{
    return !m_index.ready() || !jelly5::item_for(s, m_index.value).empty();
}

/* Both answers in (or failed): what waited on them runs. */
void Jelly5Link::settle()
{
    if (m_server.loading() || m_index.loading())
        return;
    std::vector<std::function<void()>> waiting;
    waiting.swap(m_waiting);
    for (auto &f : waiting)
        f();
}

void Jelly5Link::when_ready(api::Services *services, std::function<void()> then)
{
    prepare(services);
    if (m_server.loading() || m_index.loading())
        m_waiting.push_back(std::move(then));
    else
        then();
}

void Jelly5Link::open(api::Services *services, const std::string &item_id, const std::string &title)
{
    if (!can_open(services))
        return look_for(title);
    when_ready(services, [this, item_id, title] { launch(item_id, title); });
}

void Jelly5Link::open(api::Services *services, const model::Movie &m)
{
    if (!can_open(services))
        return look_for(m.title);
    if (!m_index.ready())
        notify(fmt::f(i18n::tr(i18n::Jelly5Searching), m.title.c_str()), true, platform::now());
    when_ready(services, [this, m] {
        if (!m_index.ready()) {
            notify(fmt::f(i18n::tr(i18n::Jelly5Failed), fmt::error(m_index.error, m_index.http_status).c_str()),
                   false, platform::now());
            return;
        }
        const std::string id = jelly5::item_for(m, m_index.value);
        if (id.empty())
            notify(fmt::f(i18n::tr(i18n::Jelly5NotInJellyfin), m.title.c_str()), false, platform::now());
        else
            launch(id, m.title);
    });
}

void Jelly5Link::open(api::Services *services, const model::Series &s)
{
    if (!can_open(services))
        return look_for(s.title);
    if (!m_index.ready())
        notify(fmt::f(i18n::tr(i18n::Jelly5Searching), s.title.c_str()), true, platform::now());
    when_ready(services, [this, s] {
        if (!m_index.ready()) {
            notify(fmt::f(i18n::tr(i18n::Jelly5Failed), fmt::error(m_index.error, m_index.http_status).c_str()),
                   false, platform::now());
            return;
        }
        const std::string id = jelly5::item_for(s, m_index.value);
        if (id.empty())
            notify(fmt::f(i18n::tr(i18n::Jelly5NotInJellyfin), s.title.c_str()), false, platform::now());
        else
            launch(id, s.title);
    });
}

/* Jelly5 checks the item is on the server it is signed in to: without the
 * server's Id, nothing is started. */
void Jelly5Link::launch(const std::string &item_id, const std::string &title)
{
    if (!m_server.ready()) {
        notify(fmt::f(i18n::tr(i18n::Jelly5Failed), fmt::error(m_server.error, m_server.http_status).c_str()), false,
               platform::now());
        return;
    }
    const jelly5::OpenRequest req{m_server.value, item_id};
    platform::Launch l;
    l.title_id = jelly5::kTitleId;
    l.args = jelly5::launch_args(req);
    l.handoff_path = jelly5::kHandoffPath;
    l.handoff = jelly5::handoff_json(req, std::time(nullptr));
    std::string error;
    if (platform::launch_app(l, &error))
        notify(fmt::f(i18n::tr(i18n::Jelly5Opening), title.c_str()), true, platform::now());
    else if (!platform::can_launch_apps())   /* refused for good: Options now says "Find" */
        look_for(title);
    else
        notify(fmt::f(i18n::tr(i18n::Jelly5Failed), error.c_str()), false, platform::now());
}

void Jelly5Link::reset()
{
    m_alive = std::make_shared<bool>(true);
    m_server = {};
    m_index = {};
    m_waiting.clear();
}

} // namespace ui
