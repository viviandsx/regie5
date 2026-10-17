/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/welcome_flow.h"

#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/config_reload.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kPanelX = 200, kPanelY = 90, kPanelW = gfx::W - 400, kPanelH = 880;
constexpr float kIn = 80;   /* the panel's inner margin */
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonText{gfx::SemiBold, 28};
const gfx::TextStyle kButtonOn{gfx::Bold, 28};
const gfx::TextStyle kTitle{gfx::Bold, 56};
constexpr uint32_t kAccent = 0xff7c8cffu;   /* the wordmark's "5" */

} // namespace

void WelcomeFlow::open()
{
    m_open = true;
    const config::MediaServer m = m_ctx.services ? m_ctx.services->config().media : config::MediaServer::Jellyfin;
    m_choice = m == config::MediaServer::Emby ? Choice::Emby : m == config::MediaServer::Plex ? Choice::Plex : Choice::Jellyfin;
    m_show.snap(0.f);
    m_show.to(1.f);
    m_drop.hide();
    go(Step::Intro);
}

/* However they are left, they no longer open by themselves; the texts go
 * back to the media server config.json names. */
void WelcomeFlow::close()
{
    m_share.close();
    m_open = false;
    m_ctx.settings.welcome_later = true;
    m_ctx.save_settings();
    i18n::set_media(m_ctx.services ? m_ctx.services->config().media : config::MediaServer::Jellyfin);
}

bool WelcomeFlow::take_config_changed()
{
    const bool c = m_config_changed;
    m_config_changed = false;
    return c;
}

void WelcomeFlow::go(Step s)
{
    if (s != Step::Phone && s != Step::Check)
        m_share.close();   /* back before the phone: its page closes */
    m_step = s;
    m_focus = 0;
    if (s == Step::Server)
        m_focus = (int)m_choice;
    m_page.snap(0.f);
    m_page.to(1.f);
}

std::string WelcomeFlow::media_query() const
{
    return m_choice == Choice::Emby   ? "m=emby"
           : m_choice == Choice::Plex ? "m=plex"
           : m_choice == Choice::None ? "m=none"
                                      : "m=jellyfin";
}

void WelcomeFlow::open_phone()
{
    if (!m_share.is_open() || !m_share.error().empty())
        m_share.open(m_ctx, media_query());
    go(Step::Phone);
}

void WelcomeFlow::check()
{
    if (reload_config(m_ctx))
        m_config_changed = true;
    m_rows.build(m_ctx);
    m_rows.test_all(m_ctx);
    if (m_step != Step::Check)
        go(Step::Check);
}

std::vector<WelcomeFlow::Button> WelcomeFlow::buttons() const
{
    switch (m_step) {
    case Step::Intro: return {Button::Start, Button::Later};
    case Step::Server: return {Button::Jellyfin, Button::Emby, Button::Plex, Button::None, Button::Later};
    case Step::Phone:
        if (!m_share.error().empty())
            return {Button::Retry, Button::Check, Button::Later};
        return {Button::Check, Button::Later};
    case Step::Check: {
        /* Jellyfin's address is there but no key: Quick Connect signs in instead. */
        const bool qc = m_choice == Choice::Jellyfin && QuickConnectSheet::offered(m_ctx) &&
                        m_ctx.services->config().at(config::Jellyfin).api_key.empty() &&
                        m_ctx.services->config().at(config::Jellyfin).token.empty();
        if (qc)
            return {Button::Go, Button::QuickConnect, Button::Fix};
        return {Button::Go, Button::Fix};
    }
    }
    return {};
}

void WelcomeFlow::press(Button b)
{
    switch (b) {
    case Button::Start: go(Step::Server); break;
    case Button::Later:
    case Button::Go: close(); break;
    case Button::Jellyfin:
    case Button::Emby:
    case Button::Plex:
    case Button::None:
        m_choice = b == Button::Emby ? Choice::Emby : b == Button::Plex ? Choice::Plex : b == Button::None ? Choice::None : Choice::Jellyfin;
        /* the texts follow the choice at once */
        i18n::set_media(m_choice == Choice::Emby   ? config::MediaServer::Emby
                        : m_choice == Choice::Plex ? config::MediaServer::Plex
                                                   : config::MediaServer::Jellyfin);
        m_share.close();                            /* its address carries the choice */
        open_phone();
        break;
    case Button::Retry:
        m_share.close();
        open_phone();
        break;
    case Button::Check: check(); break;
    case Button::Fix: open_phone(); break;
    case Button::QuickConnect: m_qc.open(); break;
    }
}

void WelcomeFlow::input(const input::State &in)
{
    if (m_qc.is_open()) {
        m_qc.input(in);
        return;
    }
    const uint32_t p = in.pressed;
    const int n = (int)buttons().size();
    if (p & input::Left)
        m_focus = std::max(0, m_focus - 1);
    else if (p & input::Right)
        m_focus = std::min(n - 1, m_focus + 1);
    else if (p & input::Cross)
        press(buttons()[(size_t)std::clamp(m_focus, 0, n - 1)]);
    else if (p & input::Triangle && m_step == Step::Check)
        check();
    else if (p & input::Circle) {
        if (m_step == Step::Server)
            go(Step::Intro);
        else if (m_step == Step::Phone)
            go(Step::Server);
        else if (m_step == Step::Check)
            open_phone();
    }
}

void WelcomeFlow::draw(double now, float dt, bool *animating)
{
    if (!m_open)
        return;
    /* The phone saved, or Quick Connect signed in: config.json is read again
     * and every service tested. */
    if (m_share.saved() || m_qc.take_signed_in())
        check();
    if (m_show.step(dt, 10.f) || m_page.step(dt, 12.f))
        *animating = true;
    if (m_share.is_open())
        *animating = true;   /* keeps looking at the phone's page */
    const float a = smoothstep(m_show.value), e = smoothstep(m_page.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(kBg, 0.35f * a));
    const gfx::Rect r{kPanelX, kPanelY + (1 - a) * 40, kPanelW, kPanelH};
    glass_panel(r, 44, a, true);
    gfx::push_opacity(a * e);
    const float x = r.x + kIn + (1 - e) * 40, w = r.w - 2 * kIn;
    float y = r.y + kIn + 40;

    switch (m_step) {
    case Step::Intro: {
        y += 80;
        const float bw = gfx::text(x, y, "Régie", {gfx::Bold, 120}, kText);
        gfx::text(x + bw, y, "5", {gfx::Bold, 120}, kAccent);
        y += 110;
        gfx::text(x, y, i18n::tr(i18n::WelTitle), {gfx::SemiBold, 44, w}, kText);
        y += 70;
        if (m_ctx.config_error == config::LoadError::SetAside)   /* core/boot_guard: the file to fix */
            gfx::text(x, y, i18n::tr(i18n::WelSetAside), {gfx::Regular, 32, w * 0.8f, 4, 46}, kOrange);
        else
            gfx::text(x, y, i18n::tr(i18n::WelIntro), {gfx::Regular, 32, w * 0.8f, 3, 46}, kText2);
        break;
    }
    case Step::Server:
        gfx::text(x, y, i18n::tr(i18n::WelServerTitle), kTitle, kText);
        y += 80;
        gfx::text(x, y, i18n::tr(i18n::WelServerText), {gfx::Regular, 32, w * 0.8f, 3, 46}, kText2);
        break;
    case Step::Phone: {
        gfx::text(x, y, i18n::tr(i18n::WelPhoneTitle), kTitle, kText);
        const platform::ShareInfo info = m_share.info();
        const std::string why = m_share.error();
        const float qy = y + 50, qs = 440, tx = x + qs + 64, tw = x + w - tx;
        if (why.empty() && m_share.has_qr()) {
            m_share.draw_qr(x, qy, qs, a * e);
            float ty = qy + 40;
            gfx::text(tx, ty, i18n::tr(i18n::WelPhoneScan), {gfx::SemiBold, 32, tw, 3, 44}, kText);
            ty += 140;
            gfx::text(tx, ty, std::string(i18n::tr(i18n::ShareCode)) + "  " + info.code, {gfx::Bold, 48, tw},
                      0xffffb240u);
            ty += 70;
            gfx::text(tx, ty, i18n::tr(i18n::ShareOrType), {gfx::Regular, 24, tw}, kText2);
            gfx::text(tx, ty + 42, info.shown, {gfx::SemiBold, 30, tw}, kText);
            ty += 110;
            const char *state = m_share.demo()          ? i18n::tr(i18n::WelDemo)
                                : info.phone_seen        ? i18n::tr(i18n::SharePhoneSeen)
                                                         : i18n::tr(i18n::ShareWaiting);
            gfx::text(tx, ty, state, {gfx::SemiBold, 26, tw, 2, 34}, m_share.demo() ? 0xffffd60au : kGreen);
            if (!info.kept_invalid.empty())
                gfx::text(tx, ty + 76, i18n::tr(i18n::WelInvalidKept), {gfx::Regular, 22, tw, 2, 30}, kText3);
        } else {   /* the console would not let the page open: the file by hand */
            float ty = qy + 40;
            gfx::text(x, ty, i18n::tr(i18n::ShareFailed), {gfx::SemiBold, 34, w}, 0xffff8a80u);
            gfx::text(x, ty + 50, why, {gfx::Regular, 24, w}, kText3);
            ty += 130;
            const std::string where = m_ctx.config_shown.empty() ? m_ctx.config_path : m_ctx.config_shown;
            gfx::text(x, ty, fmt::f(i18n::tr(i18n::WelFtp), where.c_str()), {gfx::Regular, 30, w, 4, 42}, kText);
        }
        break;
    }
    case Step::Check: {
        gfx::text(x, y, i18n::tr(i18n::WelCheckTitle), kTitle, kText);
        y += 64;
        const std::string kept = m_share.is_open() ? m_share.info().kept_invalid : std::string();
        gfx::text(x, y, std::string(i18n::tr(i18n::WelCheckText)) + (kept.empty() ? "" : std::string(" ") + i18n::tr(i18n::WelInvalidKept)),
                  {gfx::Regular, 26, w, 2, 34}, kText2);
        y += kept.empty() ? 40 : 74;
        const int n = m_rows.size();
        const float row_h = ServiceRows::row_height(n);
        const gfx::Rect pane{x, y, w, n * row_h + 20};
        glass_panel(pane, 32, a * e, false);
        m_rows.draw(m_ctx, x, y + 10, w, -1, now, animating);
        break;
    }
    }

    /* The buttons, along the panel's foot; the drop on the focused one. */
    const std::vector<Button> bs = buttons();
    m_focus = std::clamp(m_focus, 0, (int)bs.size() - 1);
    float bx = x;
    const float by = r.y + r.h - kIn - kButtonH;
    std::vector<gfx::Rect> rects;
    std::vector<std::string> labels;
    for (Button b : bs) {
        const char *t = b == Button::Start      ? i18n::tr(i18n::WelStart)
                        : b == Button::Later    ? i18n::tr(i18n::WelLater)
                        : b == Button::Jellyfin ? "Jellyfin"   /* names, never translated nor swapped */
                        : b == Button::Emby     ? "Emby"
                        : b == Button::Plex     ? "Plex"
                        : b == Button::None     ? i18n::tr(i18n::WelNone)
                        : b == Button::Check    ? i18n::tr(i18n::WelCheck)
                        : b == Button::Retry    ? i18n::tr(i18n::WelRetry)
                        : b == Button::Go       ? i18n::tr(i18n::WelGo)
                        : b == Button::QuickConnect ? i18n::tr(i18n::QcTitle)
                                                : i18n::tr(i18n::WelFix);
        labels.push_back(t);
        /* "Later" stands apart, at the right. */
        const float bw = std::max(160.f, gfx::text_width(t, kButtonText) + 64);
        const float px = b == Button::Later ? x + w - bw - 2 * kBarPad : bx;
        const gfx::Rect bar{px, by - kBarPad, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
        glass_panel(bar, bar.h / 2, a * e, false);
        rects.push_back({px + kBarPad, by, bw, kButtonH});
        if (b != Button::Later)
            bx += bar.w + 18;
    }
    gfx::pop_opacity();
    m_drop.to(rects[(size_t)m_focus], (int)m_step * 10 + m_focus, 0, 0);
    m_drop.draw(dt, a, animating);
    for (size_t i = 0; i < rects.size(); i++) {
        const bool on = (int)i == m_focus;
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 42, labels[i], on ? kButtonOn : kButtonText,
                  alpha(on ? kText : kText2, a * e), 1);
    }
    m_qc.draw(dt, animating);
}

} // namespace ui
