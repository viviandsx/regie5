/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/quick_connect.h"

#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/config_reload.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {

constexpr float kW = 1180, kH = 640, kIn = 72;
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonText{gfx::SemiBold, 28}, kButtonOn{gfx::Bold, 28};
constexpr double kPollEvery = 2.0;   /* seconds between two questions to Jellyfin */

} // namespace

bool QuickConnectSheet::offered(const Context &ctx)
{
    return ctx.services && ctx.services->config().media == config::MediaServer::Jellyfin &&
           !ctx.services->config().at(config::Jellyfin).url.empty();
}

void QuickConnectSheet::open()
{
    m_open = true;
    m_show.snap(0.f);
    m_show.to(1.f);
    m_drop.hide();
    start();
}

bool QuickConnectSheet::take_signed_in()
{
    const bool s = m_signed_in;
    m_signed_in = false;
    return s;
}

void QuickConnectSheet::fail(const std::string &why)
{
    m_step = Step::Failed;
    m_message = why;
    m_ok = false;
    m_polling = false;
    m_focus = 0;
}

void QuickConnectSheet::start()
{
    m_step = Step::Checking;
    m_code = {};
    m_message.clear();
    m_polling = false;
    m_focus = 0;
    const unsigned gen = ++m_gen;
    if (!offered(m_ctx)) {
        fail(i18n::tr(i18n::QcNoUrl));
        return;
    }
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->quick_connect_enabled([this, alive, gen](api::Result<bool> r) {
        if (alive.expired() || gen != m_gen || !m_open)
            return;
        if (!r.ok()) {
            fail(fmt::labelled("Jellyfin", fmt::error(r.error, r.http_status)));
            return;
        }
        if (!r.value) {
            fail(i18n::tr(i18n::QcDisabled));
            return;
        }
        m_ctx.services->quick_connect_start([this, alive, gen](api::Result<model::QuickConnect> c) {
            if (alive.expired() || gen != m_gen || !m_open)
                return;
            if (!c.ok()) {
                fail(fmt::labelled("Jellyfin", fmt::error(c.error, c.http_status)));
                return;
            }
            m_code = c.value;
            m_step = Step::Code;
            m_next_poll = platform::now() + kPollEvery;
        });
    });
}

void QuickConnectSheet::finish(const model::JellyfinSession &s)
{
    m_polling = false;
    m_step = Step::Done;
    m_ok = true;
    m_focus = 0;
    m_message = fmt::f(i18n::tr(s.admin ? i18n::QcDone : i18n::QcDoneNotAdmin), s.user.c_str());
    if (m_ctx.mock) {   /* nothing written: the session lives in memory */
        config::Config c = m_ctx.services->config();
        config::ServiceConfig &j = c.services[config::Jellyfin];
        j.api_key.clear();
        j.token = s.token;
        j.user = s.user;
        j.admin = s.admin;
        j.enabled = true;
        m_ctx.services->reconfigure(c);
        m_ctx.config_gen++;
        m_message += std::string(" ") + i18n::tr(i18n::QcMock);
        m_signed_in = true;
        return;
    }
    std::string text, updated;
    if (!config::read_file(m_ctx.config_path, &text) ||
        !config::with_jellyfin_session(text, s.token, s.user, s.admin, &updated) ||
        !config::write_file(m_ctx.config_path, updated)) {
        fail(i18n::tr(i18n::QcSaveFailed));
        return;
    }
    reload_config(m_ctx);
    m_signed_in = true;
}

void QuickConnectSheet::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    const int buttons = m_step == Step::Failed ? 2 : 1;
    if (p & input::Circle) {
        m_open = false;
        ++m_gen;   /* what is on its way is no one's now */
    } else if (p & input::Left) {
        m_focus = std::max(0, m_focus - 1);
    } else if (p & input::Right) {
        m_focus = std::min(buttons - 1, m_focus + 1);
    } else if (p & input::Cross) {
        if (m_step == Step::Failed && m_focus == 0) {
            start();
        } else {
            m_open = false;
            ++m_gen;
        }
    }
}

void QuickConnectSheet::draw(float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 10.f))
        *animating = true;
    /* Waiting for the phone: Jellyfin is asked every two seconds. */
    if (m_step == Step::Code) {
        *animating = true;
        const double now = platform::now();
        if (!m_polling && now >= m_next_poll) {
            m_polling = true;
            const unsigned gen = m_gen;
            const std::string secret = m_code.secret;
            std::weak_ptr<bool> alive = m_alive;
            m_ctx.services->quick_connect_poll(secret, [this, alive, gen, secret](api::Result<bool> r) {
                if (alive.expired() || gen != m_gen || !m_open)
                    return;
                m_polling = false;
                m_next_poll = platform::now() + kPollEvery;
                if (!r.ok()) {   /* the code expired, or Jellyfin went away */
                    fail(fmt::labelled("Jellyfin", fmt::error(r.error, r.http_status)));
                    return;
                }
                if (!r.value)
                    return;
                m_polling = true;
                m_ctx.services->quick_connect_finish(secret, [this, alive, gen](api::Result<model::JellyfinSession> s) {
                    if (alive.expired() || gen != m_gen || !m_open)
                        return;
                    if (!s.ok())
                        fail(fmt::labelled("Jellyfin", fmt::error(s.error, s.http_status)));
                    else
                        finish(s.value);
                });
            });
        }
    }
    const float a = smoothstep(m_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.55f * a));
    const gfx::Rect r{(gfx::W - kW) / 2, (gfx::H - kH) / 2 + (1 - a) * 40, kW, kH};
    glass_panel(r, 44, a, true);
    gfx::push_opacity(a);
    const float x = r.x + kIn, w = kW - 2 * kIn;
    float y = r.y + kIn + 40;
    gfx::text(x, y, i18n::tr(i18n::QcTitle), {gfx::Bold, 52, w}, kText);
    y += 70;
    switch (m_step) {
    case Step::Checking: gfx::text(x, y, i18n::tr(i18n::QcChecking), {gfx::Regular, 30, w}, kText2); break;
    case Step::Code: {
        gfx::text(x, y, i18n::tr(i18n::QcCodeLead), {gfx::Regular, 30, w, 2, 42}, kText);
        y += 150;
        /* The code, its digits apart, large enough to read from the couch. */
        std::string spaced;
        for (char c : m_code.code)
            spaced += (spaced.empty() ? "" : " ") + std::string(1, c);
        gfx::text(r.x + kW / 2, y, spaced, {gfx::Bold, 96}, 0xffffb240u, 1);
        y += 70;
        gfx::text(x, y, i18n::tr(i18n::QcAdminNote), {gfx::Regular, 24, w, 2, 32}, kText3);
        const float pulse = 0.5f + 0.5f * (float)std::sin(platform::now() * 4.0);
        gfx::fill({x, y + 50, 14, 14}, alpha(kOrange, 0.4f + 0.6f * pulse), 7);
        gfx::text(x + 28, y + 64, i18n::tr(i18n::QcWaiting), {gfx::Medium, 26, w - 28}, kText2);
        break;
    }
    case Step::Done:
    case Step::Failed:
        gfx::text(x, y, m_message, {gfx::SemiBold, 30, w, 4, 42}, m_ok ? kGreen : 0xffff8a80u);
        break;
    }

    /* The buttons: Cancel while it waits, OK once done, Retry and Close after a failure. */
    std::vector<std::string> labels;
    if (m_step == Step::Failed)
        labels = {i18n::tr(i18n::WelRetry), i18n::tr(i18n::Close)};
    else
        labels = {i18n::tr(m_step == Step::Done ? i18n::WnOk : i18n::Cancel)};
    m_focus = std::clamp(m_focus, 0, (int)labels.size() - 1);
    std::vector<gfx::Rect> rects;
    float bx = x;
    const float by = r.y + r.h - kIn - kButtonH;
    for (const std::string &l : labels) {
        const float bw = std::max(170.f, gfx::text_width(l, kButtonText) + 64);
        const gfx::Rect bar{bx, by - kBarPad, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
        glass_panel(bar, bar.h / 2, 1.f, false);
        rects.push_back({bx + kBarPad, by, bw, kButtonH});
        bx += bar.w + 18;
    }
    gfx::pop_opacity();
    m_drop.to(rects[(size_t)m_focus], (int)m_step * 10 + m_focus, 0, 0);
    m_drop.draw(dt, a, animating);
    for (size_t i = 0; i < rects.size(); i++) {
        const bool on = (int)i == m_focus;
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 42, labels[i], on ? kButtonOn : kButtonText,
                  alpha(on ? kText : kText2, a), 1);
    }
}

} // namespace ui
