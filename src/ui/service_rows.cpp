/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/service_rows.h"

#include "core/i18n.h"
#include "core/versions.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kRowH = 86;

/* "192.168.1.10:8989/sonarr" from "http://192.168.1.10:8989/sonarr": the path is kept,
   behind a reverse proxy it is what tells the services apart. */
std::string address_of(const std::string &url)
{
    const size_t s = url.find("//");
    return url.substr(s == std::string::npos ? 0 : s + 2);
}

std::string error_text(const model::ServiceStatus &st)
{
    return fmt::error((api::Error)st.error, st.http_status);
}

} // namespace

void ServiceRows::build(const Context &ctx)
{
    m_rows.clear();
    for (int i = 0; i < config::ServiceCount; i++) {
        const config::Service s = (config::Service)i;
        const int n = ctx.services ? ctx.services->instances(s) : 1;
        for (int k = 0; k < n; k++)
            m_rows.push_back({s, k, {}, 0});
    }
}

void ServiceRows::test_all(Context &ctx)
{
    for (int i = 0; i < (int)m_rows.size(); i++)
        test(ctx, i);
}

bool ServiceRows::settled() const
{
    return std::none_of(m_rows.begin(), m_rows.end(),
                        [](const Row &r) { return r.status.state == model::ServiceStatus::Testing; });
}

void ServiceRows::test(Context &ctx, int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;
    Row &r = m_rows[row];
    model::ServiceStatus &st = r.status;
    if (ctx.config_error != config::LoadError::None) {
        st.state = model::ServiceStatus::NotConfigured;
        return;
    }
    if (!ctx.services || !ctx.services->enabled(r.service, r.instance)) {
        st.state = ctx.services && !ctx.services->config().at(r.service, r.instance).url.empty()
                       ? model::ServiceStatus::Disabled
                       : model::ServiceStatus::NotConfigured;
        return;
    }
    st.state = model::ServiceStatus::Testing;
    const unsigned gen = ++r.gen;
    std::weak_ptr<bool> alive = m_alive;
    const config::Service svc = r.service;
    const int inst = r.instance;
    ctx.services->test(
        svc,
        [this, alive, svc, inst, gen](api::Result<std::string> res) {
            if (alive.expired())
                return;
            for (Row &x : m_rows)
                if (x.service == svc && x.instance == inst && x.gen == gen) {
                    x.status.state = res.ok() ? model::ServiceStatus::Ok : model::ServiceStatus::Failed;
                    x.status.version = res.value;
                    x.status.error = (int)res.error;
                    x.status.http_status = res.http_status;
                }
        },
        inst);
}

float ServiceRows::row_height(int n) { return n > 5 ? std::min(kRowH, 400.f / n) : kRowH; }

void ServiceRows::draw(const Context &ctx, float x0, float y0, float kListW, int focus, double now,
                       bool *animating) const
{
    const int n = (int)m_rows.size();
    const bool compact = n > 5;
    const float row_h = row_height(n);
    const bool emby = ctx.services && ctx.services->config().emby();
    const bool both_media = ctx.services && ctx.services->config().both_media;
    for (int i = 0; i < n; i++) {
        const Row &row = m_rows[i];
        const model::ServiceStatus &st = row.status;
        const float ry = y0 + i * row_h;
        if (i > 0 && focus != i && focus != i - 1)   /* no line under the drop */
            gfx::fill({x0 + 40, ry, kListW - 80, 1}, alpha(0xffffffffu, 0.08f));

        uint32_t dot = 0xff6e6e78u;   /* off / not configured */
        std::string status, detail;
        switch (st.state) {
        case model::ServiceStatus::Ok:
            dot = kGreen;
            status = i18n::tr(i18n::StatusOk);
            /* A version; Glances has none to give: its CPU's name. */
            detail = st.version.empty() ? "" : row.service == config::Glances ? st.version : "v" + st.version;
            if (!versions::supported(row.service, st.version, emby)) {   /* it answers, but too old to rely on */
                dot = kOrange;
                status = i18n::tr(i18n::VersionUnsupported);
                detail = fmt::f(i18n::tr(i18n::VersionMinimum), st.version.c_str(), versions::minimum(row.service, emby));
            }
            break;
        case model::ServiceStatus::Failed:
            dot = kRed;
            status = error_text(st);
            break;
        case model::ServiceStatus::Testing: {
            const float pulse = 0.55f + 0.45f * (float)std::sin(now * 6.0);
            dot = alpha(kOrange, pulse);
            status = i18n::tr(i18n::StatusTesting);
            if (animating)
                *animating = true;
            break;
        }
        case model::ServiceStatus::Disabled: status = i18n::tr(i18n::StatusDisabled); break;
        case model::ServiceStatus::NotConfigured:
        case model::ServiceStatus::Unknown:
            status = i18n::tr(row.service == config::Glances ? i18n::GlancesOptional : i18n::StatusNotConfigured);
            break;
        }
        if (row.service == config::Jellyfin && both_media)   /* "jellyfin" and "emby" both on */
            detail = i18n::tr(i18n::MediaBoth);
        const float cy = ry + row_h / 2;
        if (st.state == model::ServiceStatus::Ok || st.state == model::ServiceStatus::Failed)
            gfx::shadow({x0 + 44, cy - 9, 18, 18}, 9, 10, 0.6f, 0);
        gfx::fill({x0 + 44, cy - 9, 18, 18}, dot, 9);
        const config::ServiceConfig &c =
            ctx.services ? ctx.services->config().at(row.service, row.instance) : config::ServiceConfig{};
        const std::string name = !c.name.empty()                ? c.name
                                 : row.service == config::Jellyfin ? config::media_name(ctx.services ? ctx.services->config().media
                                                                                                       : config::MediaServer::Jellyfin)
                                                                   : config::service_name(row.service);
        const bool show_url = !c.url.empty() && !ctx.mock && !compact;
        gfx::text(x0 + 86, ry + (show_url ? 40 : row_h / 2 + 11), name, {gfx::SemiBold, compact ? 28.f : 32.f}, kText);
        if (show_url)
            gfx::text(x0 + 86, ry + 70, address_of(c.url), {gfx::Regular, 22, kListW * 0.4f}, kText3);
        const uint32_t ink = st.state == model::ServiceStatus::Failed ? 0xffff8a80u
                             : st.state == model::ServiceStatus::Ok && !versions::supported(row.service, st.version, emby)
                                 ? 0xffffd60au
                                 : kText;
        const bool two = !detail.empty() && !compact;
        gfx::text(x0 + kListW - 44, ry + (two ? 40 : row_h / 2 + 10), status, {gfx::Medium, 27, kListW * 0.55f}, ink, 2);
        if (two)
            gfx::text(x0 + kListW - 44, ry + 70, detail, {gfx::Regular, 21, kListW * 0.55f}, kText3, 2);
    }
}

} // namespace ui
