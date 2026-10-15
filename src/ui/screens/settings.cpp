/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/settings.h"

#include "core/i18n.h"
#include "core/versions.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/config_reload.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/tabbar.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

constexpr float kListY = 330, kListPad = 10;
constexpr float kListW = gfx::W - 2 * kPad;
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonText{gfx::SemiBold, 28};

const char *config_error_text(config::LoadError e)
{
    switch (e) {
    case config::LoadError::Missing: return i18n::tr(i18n::ConfigMissing);
    case config::LoadError::Unreadable: return i18n::tr(i18n::ConfigUnreadable);
    case config::LoadError::BadJson: return i18n::tr(i18n::ConfigBadJson);
    case config::LoadError::None: break;
    }
    return "";
}

} // namespace

SettingsScreen::SettingsScreen(Context &ctx) : m_ctx(ctx) { build_rows(); }

/* The connection tests run when the screen is first opened, not when it is
 * made: nothing is asked of the servers for a screen nobody looked at. */
void SettingsScreen::activate()
{
    if (m_tested)
        return;
    m_tested = true;
    test_all();
}

void SettingsScreen::build_rows()
{
    m_list.build(m_ctx);
    m_focus = std::min(m_focus, m_list.size());
}

void SettingsScreen::test_all() { m_list.test_all(m_ctx); }

void SettingsScreen::test(int row) { m_list.test(m_ctx, row); }

void SettingsScreen::reload_config()
{
    if (ui::reload_config(m_ctx))
        build_rows();
}

/* Turning it on warns first (Cancel by default); turning it off does not ask. */
void SettingsScreen::toggle_cleanup()
{
    if (m_ctx.settings.cleanup_enabled) {
        m_ctx.settings.cleanup_enabled = false;
        m_ctx.save_settings();
        return;
    }
    m_sheet.open(i18n::tr(i18n::CleanupWarnTitle), i18n::tr(i18n::CleanupWarnBody),
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::CleanupEnable)}, 0,
                 [this](int choice) {
                     if (choice != 1)
                         return;
                     m_ctx.settings.cleanup_enabled = true;
                     m_ctx.save_settings();
                 },
                 true);
}

/* Test all, the cleanup, the refresh rate, then from a phone (a real
 * configuration only: --mock reads no file) and the welcome screens. */
std::vector<SettingsScreen::Button> SettingsScreen::buttons() const
{
    std::vector<Button> b{Button::TestAll, Button::Cleanup, Button::Refresh};
    if (!m_ctx.mock && !m_ctx.config_path.empty() && platform::can_share_config())
        b.push_back(Button::Phone);
    b.push_back(Button::Welcome);
    return b;
}

int SettingsScreen::button_count() const { return (int)buttons().size(); }

void SettingsScreen::open_share()
{
    m_share.open(m_ctx);
    m_share_show.snap(0.f);
    m_share_show.to(1.f);
}

void SettingsScreen::close_share() { m_share.close(); }

void SettingsScreen::draw_share(float dt, bool *animating)
{
    if (!m_share.is_open())
        return;
    const platform::ShareInfo info = m_share.info();
    if (m_share.saved()) {   /* the phone saved: read it and test, as "test all" does */
        reload_config();
        test_all();
    }
    m_share_show.step(dt, 12.f);
    if (animating)
        *animating = true;   /* keeps looking at the server */
    const float a = smoothstep(m_share_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.6f * a));
    const float w = 1300, h = 700;
    const gfx::Rect r{(gfx::W - w) / 2, (gfx::H - h) / 2 + (1 - a) * 40, w, h};
    glass_panel(r, 40, a, true);
    gfx::text(r.x + 64, r.y + 96, i18n::tr(i18n::ShareTitle), {gfx::Bold, 44}, alpha(kText, a));

    /* The QR code on white, with its quiet zone, as large as the panel allows. */
    const float qx = r.x + 64, qy = r.y + 140, qs = 420;
    m_share.draw_qr(qx, qy, qs, a);
    const float tx = qx + qs + 56, tw = r.x + w - 64 - tx;
    float ty = qy + 30;
    if (!info.running && !m_share.has_qr()) {
        gfx::text(tx, ty, i18n::tr(i18n::ShareFailed), {gfx::SemiBold, 30, tw, 2, 40}, alpha(0xffff8a80u, a));
        gfx::text(tx, ty + 90, m_share.error(), {gfx::Regular, 24, tw, 3, 32}, alpha(kText3, a));
    } else {
        gfx::text(tx, ty, i18n::tr(i18n::ShareScan), {gfx::Regular, 28, tw, 3, 38}, alpha(kText, a));
        ty += 150;
        gfx::text(tx, ty, i18n::tr(i18n::ShareOrType), {gfx::Regular, 24, tw}, alpha(kText2, a));
        gfx::text(tx, ty + 46, info.shown, {gfx::SemiBold, 32, tw}, alpha(kText, a));
        gfx::text(tx, ty + 100, std::string(i18n::tr(i18n::ShareCode)) + "  " + info.code, {gfx::Bold, 40, tw},
                  alpha(0xffffb240u, a));
        ty += 170;
        const char *state = !info.running          ? i18n::tr(i18n::ShareFailed)
                            : m_share.saves() > 0  ? i18n::tr(i18n::ShareSaved)
                            : info.phone_seen      ? i18n::tr(i18n::SharePhoneSeen)
                                                   : i18n::tr(i18n::ShareWaiting);
        gfx::text(tx, ty, state, {gfx::SemiBold, 26, tw, 2, 34}, alpha(info.running ? kGreen : 0xffff8a80u, a));
        gfx::text(tx, ty + 80, i18n::tr(i18n::ShareKeysNote), {gfx::Regular, 22, tw, 3, 30}, alpha(kText3, a));
    }
    gfx::text(r.x + w - 64, r.y + h - 40, std::string("○ ") + i18n::tr(i18n::ShareClose), {gfx::Medium, 24},
              alpha(kText2, a), 2);
}

Action SettingsScreen::input(const input::State &in)
{
    if (m_share.is_open()) {
        if (in.pressed & (input::Circle | input::Cross))
            close_share();
        return Action::None;
    }
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    const int n = m_list.size();
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        reload_config();
        test_all();
        return Action::None;
    }
    if (p & input::Up) {
        if (m_focus == 0)
            return Action::ToTabs;
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus == n)
            return Action::Bump;
        m_focus++;
    } else if (m_focus == n && (p & (input::Left | input::Right))) {
        m_button = std::clamp(m_button + ((p & input::Left) ? -1 : 1), 0, button_count() - 1);
    } else if (p & input::Cross) {
        if (m_focus < n) {
            test(m_focus);
        } else {
            switch (buttons()[(size_t)m_button]) {
            case Button::TestAll:
                reload_config();
                test_all();
                break;
            case Button::Cleanup: toggle_cleanup(); break;
            case Button::Refresh: {   /* Auto → 60 Hz → 120 Hz → Auto; taken at the next start */
                /* A build that cannot ask for 120 Hz (0.3.1) does not offer it. */
                const bool can_120 = platform::display_mode().can_120;
                app_settings::Refresh &r = m_ctx.settings.refresh;
                r = r == app_settings::Refresh::Auto               ? app_settings::Refresh::Hz60
                    : r == app_settings::Refresh::Hz60 && can_120 ? app_settings::Refresh::Hz120
                                                                    : app_settings::Refresh::Auto;
                m_ctx.save_settings();
                break;
            }
            case Button::Phone: open_share(); break;
            case Button::Welcome: m_ctx.welcome_request = true; break;   /* the shell opens them */
            }
        }
    }
    return Action::None;
}

void SettingsScreen::draw(double now, float dt, float ox, bool *animating)
{
    const float x0 = kPad + ox;
    gfx::text(x0, 250, tab_label(TabSettings), {gfx::Bold, 72}, kText);
    float y = 304;
    if (m_ctx.mock) {
        gfx::text(x0, y, i18n::tr(i18n::MockMode), {gfx::Medium, 26, kListW}, 0xffffd60au);
    } else if (m_ctx.config_error != config::LoadError::None) {
        gfx::text(x0, y, config_error_text(m_ctx.config_error), {gfx::Medium, 26, kListW}, 0xffff6961u);
    } else if (m_ctx.reload_error != config::LoadError::None) {
        gfx::text(x0, y,
                  std::string(config_error_text(m_ctx.reload_error)) + " " + i18n::tr(i18n::ConfigKept),
                  {gfx::Medium, 26, kListW}, 0xffff6961u);
    } else {
        gfx::text(x0, y, i18n::tr(i18n::SettingsServices), {gfx::SemiBold, 30}, kText2);
    }

    /* The services: one pane, a row each (each instance); compact past five. */
    const int n = m_list.size();
    const float row_h = ServiceRows::row_height(n);
    const gfx::Rect pane{x0, kListY, kListW, n * row_h + 2 * kListPad};
    glass_panel(pane, 32, 1.f, true);

    /* Under it, the buttons: test all, the cleanup's switch, the refresh rate, and set up from a phone. */
    const float by = kListY + pane.h + 30;
    const int nb = button_count();
    if (m_button >= nb)
        m_button = nb - 1;
    const app_settings::Refresh refresh = m_ctx.settings.refresh;
    std::vector<std::string> labels;
    for (Button b : buttons())
        labels.push_back(i18n::tr(b == Button::TestAll   ? i18n::ButtonTestAll
                                  : b == Button::Cleanup ? (m_ctx.settings.cleanup_enabled ? i18n::CleanupOn : i18n::CleanupOff)
                                  : b == Button::Refresh ? (refresh == app_settings::Refresh::Hz60    ? i18n::Refresh60
                                                            : refresh == app_settings::Refresh::Hz120 ? i18n::Refresh120
                                                                                                      : i18n::RefreshAuto)
                                  : b == Button::Phone   ? i18n::ButtonFromPhone
                                                         : i18n::ButtonWelcome));
    gfx::Rect buttons[5];
    float bx = x0;
    for (int i = 0; i < nb; i++) {
        const float bw = gfx::text_width(labels[i], kButtonText) + 56;   /* five fit in 1680 */
        const gfx::Rect bar{bx, by, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
        glass_panel(bar, bar.h / 2, 1.f, false);
        buttons[i] = {bx + kBarPad, by + kBarPad, bw, kButtonH};
        bx += bar.w + 18;
    }
    if (m_focused) {
        if (m_focus < n)
            m_drop.to({x0 + kListPad, kListY + kListPad + m_focus * row_h + 3, kListW - 2 * kListPad, row_h - 6},
                      m_focus, ox, 0);
        else
            m_drop.to(buttons[m_button], n + m_button, ox, 0);
    } else {
        m_drop.hide();
    }
    m_drop.draw(dt, 1.f, animating, m_focus == n ? -1.f : 22.f);

    m_list.draw(m_ctx, x0, kListY + kListPad, kListW, m_focused && m_focus < n ? m_focus : -1, now, animating);
    for (int i = 0; i < nb; i++) {
        const bool on = m_focused && m_focus == n && m_button == i;
        gfx::text(buttons[i].x + buttons[i].w / 2, by + kBarPad + 42, labels[i],
                  on ? gfx::TextStyle{gfx::Bold, 28} : kButtonText, on ? kText : kText2, 1);
    }

    /* The app, and the picture it sends out. */
    const float iy = by + kButtonH + 2 * kBarPad + 34;
    gfx::text(x0, iy, std::string(i18n::tr(i18n::Version)) + " " + m_ctx.version, {gfx::Medium, 24}, kText2);
    /* In --mock no file is read: its path would only show the user's home in screenshots. */
    gfx::text(x0, iy + 34,
              m_ctx.mock ? std::string(i18n::tr(i18n::ConfigUnusedMock))
                         : std::string(i18n::tr(i18n::ConfigFile)) + " " +
                               (m_ctx.config_shown.empty() ? m_ctx.config_path : m_ctx.config_shown),
              {gfx::Medium, 24, kListW}, kText2);
    /* Then the picture sent out, on up to two lines of its own: left of the pad's hints. */
    const platform::DisplayMode dm = platform::display_mode();
    if (dm.hz > 0) {
        std::string line = fmt::f(i18n::tr(i18n::DisplayInfo), dm.width, dm.height, dm.hz);
        if (dm.asked_hz == 0)
            line += std::string(" — ") + i18n::tr(i18n::DisplayDesktop);
        else if (dm.asked_hz > dm.hz)   /* Auto falls back quietly; 120 Hz by hand says why */
            line += std::string(" — ") + i18n::tr(refresh == app_settings::Refresh::Hz120 ? i18n::DisplayNoTv120
                                                                                          : i18n::DisplayAuto60);
        /* 120 Hz chosen (kept from 0.3.0) in a build that cannot ask for it. */
        const bool hz120_off = refresh == app_settings::Refresh::Hz120 && !dm.can_120;
        if (hz120_off) {
            line += std::string(" — ") + i18n::tr(i18n::Display120Unavailable);
        } else {
            if (dm.asked_hz != 0 && app_settings::refresh_asked(refresh) != dm.asked_hz)
                line += std::string(" — ") + i18n::tr(i18n::DisplayNextStart);
        }
        const bool warn = (dm.asked_hz > dm.hz && refresh == app_settings::Refresh::Hz120) || hz120_off;
        gfx::text(x0, iy + 68, line, {gfx::Medium, 24, kListW * 0.55f, 2, 32}, warn ? 0xffffd60au : kText2);
    }
}

} // namespace ui
