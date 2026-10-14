/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/settings.h"

#include "core/i18n.h"
#include "core/versions.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/style.h"
#include "ui/tabbar.h"
#include "ui/widgets.h"

#include "qrcodegen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

constexpr float kListY = 330, kRowH = 86, kListPad = 10;
constexpr float kListW = gfx::W - 2 * kPad;
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonText{gfx::SemiBold, 28};

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
    m_rows.clear();
    for (int i = 0; i < config::ServiceCount; i++) {
        const config::Service s = (config::Service)i;
        const int n = m_ctx.services ? m_ctx.services->instances(s) : 1;
        for (int k = 0; k < n; k++)
            m_rows.push_back({s, k, {}, 0});
    }
    m_focus = std::min(m_focus, (int)m_rows.size());
}

void SettingsScreen::test_all()
{
    for (int i = 0; i < (int)m_rows.size(); i++)
        test(i);
}

void SettingsScreen::reload_config()
{
    if (m_ctx.mock || !m_ctx.services)
        return;
    config::Config next;
    const config::LoadError e = config::load(m_ctx.config_path, &next);
    if (e != config::LoadError::None) {
        if (m_ctx.config_error != config::LoadError::None)
            m_ctx.config_error = e;    /* never had one that worked: same as at start */
        else
            m_ctx.reload_error = e;    /* keep the one in use */
        return;
    }
    m_ctx.config_error = m_ctx.reload_error = config::LoadError::None;
    const unsigned changed = config::changed(m_ctx.services->config(), next);
    if (!changed)
        return;
    m_ctx.services->reconfigure(next);
    i18n::set_emby(next.emby());
    art::forget(changed);
    m_ctx.config_gen++;
    build_rows();
    notify(i18n::tr(i18n::ConfigReloaded), true, platform::now());
}

void SettingsScreen::test(int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;
    Row &r = m_rows[row];
    model::ServiceStatus &st = r.status;
    if (m_ctx.config_error != config::LoadError::None) {
        st.state = model::ServiceStatus::NotConfigured;
        return;
    }
    if (!m_ctx.services || !m_ctx.services->enabled(r.service, r.instance)) {
        st.state = m_ctx.services && !m_ctx.services->config().at(r.service, r.instance).url.empty()
                       ? model::ServiceStatus::Disabled
                       : model::ServiceStatus::NotConfigured;
        return;
    }
    st.state = model::ServiceStatus::Testing;
    const unsigned gen = ++r.gen;
    std::weak_ptr<bool> alive = m_alive;
    const config::Service svc = r.service;
    const int inst = r.instance;
    m_ctx.services->test(
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

int SettingsScreen::button_count() const
{
    /* From a phone: on a real configuration only (--mock reads no file). */
    return !m_ctx.mock && !m_ctx.config_path.empty() && platform::can_share_config() ? 4 : 3;
}

void SettingsScreen::open_share()
{
    /* The console's address that faces the user's servers: the phone is on that network. */
    std::string hint;
    if (m_ctx.services)
        for (int s = 0; s < config::ServiceCount && hint.empty(); s++) {
            const std::string &url = m_ctx.services->config().at((config::Service)s).url;
            const size_t a = url.find("://");
            if (a != std::string::npos)
                hint = url.substr(a + 3, url.find_first_of(":/", a + 3) - a - 3);
        }
    m_share_error.clear();
    m_qr.clear();
    m_qr_size = 0;
    std::string why;
    if (!platform::share_start(m_ctx.config_path, hint, &why)) {
        m_share_error = why;
        std::fprintf(stderr, "regie5: phone page: %s\n", why.c_str());
    } else {
        const platform::ShareInfo info = platform::share_info();
        std::vector<uint8_t> tmp(qrcodegen_BUFFER_LEN_MAX), code(qrcodegen_BUFFER_LEN_MAX);
        if (qrcodegen_encodeText(info.address.c_str(), tmp.data(), code.data(), qrcodegen_Ecc_MEDIUM,
                                 qrcodegen_VERSION_MIN, 10, qrcodegen_Mask_AUTO, true)) {
            m_qr_size = qrcodegen_getSize(code.data());
            m_qr.resize((size_t)m_qr_size * m_qr_size);
            for (int y = 0; y < m_qr_size; y++)
                for (int x = 0; x < m_qr_size; x++)
                    m_qr[(size_t)y * m_qr_size + x] = qrcodegen_getModule(code.data(), x, y);
        }
        std::fprintf(stderr, "regie5: phone page open on %s\n", info.shown.c_str());   /* never the code */
    }
    m_share_saves = 0;
    m_share_open = true;
    m_share_show.snap(0.f);
    m_share_show.to(1.f);
}

void SettingsScreen::close_share()
{
    platform::share_stop();
    m_share_open = false;
}

void SettingsScreen::draw_share(float dt, bool *animating)
{
    if (!m_share_open)
        return;
    const platform::ShareInfo info = platform::share_info();
    if (info.saves > m_share_saves) {   /* the phone saved: read it and test, as "test all" does */
        m_share_saves = info.saves;
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
    if (m_qr_size > 0) {
        gfx::fill({qx, qy, qs, qs}, alpha(0xffffffffu, a), 18);
        const float cell = std::floor(qs / (m_qr_size + 8));   /* a quiet zone of four modules around it */
        const float off = (qs - cell * m_qr_size) / 2;
        for (int y = 0; y < m_qr_size; y++)
            for (int x = 0; x < m_qr_size; x++)
                if (m_qr[(size_t)y * m_qr_size + x])
                    gfx::fill({qx + off + x * cell, qy + off + y * cell, cell, cell}, alpha(0xff000000u, a));
    }
    const float tx = qx + qs + 56, tw = r.x + w - 64 - tx;
    float ty = qy + 30;
    if (!info.running && m_qr_size == 0) {
        gfx::text(tx, ty, i18n::tr(i18n::ShareFailed), {gfx::SemiBold, 30, tw, 2, 40}, alpha(0xffff8a80u, a));
        gfx::text(tx, ty + 90, m_share_error.empty() ? info.error : m_share_error, {gfx::Regular, 24, tw, 3, 32},
                  alpha(kText3, a));
    } else {
        gfx::text(tx, ty, i18n::tr(i18n::ShareScan), {gfx::Regular, 28, tw, 3, 38}, alpha(kText, a));
        ty += 150;
        gfx::text(tx, ty, i18n::tr(i18n::ShareOrType), {gfx::Regular, 24, tw}, alpha(kText2, a));
        gfx::text(tx, ty + 46, info.shown, {gfx::SemiBold, 32, tw}, alpha(kText, a));
        gfx::text(tx, ty + 100, std::string(i18n::tr(i18n::ShareCode)) + "  " + info.code, {gfx::Bold, 40, tw},
                  alpha(0xffffb240u, a));
        ty += 170;
        const char *state = !info.running          ? i18n::tr(i18n::ShareFailed)
                            : m_share_saves > 0    ? i18n::tr(i18n::ShareSaved)
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
    if (m_share_open) {
        if (in.pressed & (input::Circle | input::Cross))
            close_share();
        return Action::None;
    }
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    const int n = (int)m_rows.size();
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
        } else if (m_button == 0) {
            reload_config();
            test_all();
        } else if (m_button == 1) {
            toggle_cleanup();
        } else if (m_button == 2) {   /* Auto → 60 Hz → 120 Hz → Auto; taken at the next start */
            /* A build that cannot ask for 120 Hz (0.3.1) does not offer it. */
            const bool can_120 = platform::display_mode().can_120;
            app_settings::Refresh &r = m_ctx.settings.refresh;
            r = r == app_settings::Refresh::Auto               ? app_settings::Refresh::Hz60
                : r == app_settings::Refresh::Hz60 && can_120 ? app_settings::Refresh::Hz120
                                                                : app_settings::Refresh::Auto;
            m_ctx.save_settings();
        } else {
            open_share();
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
    const int n = (int)m_rows.size();
    const bool compact = n > 5;
    const float row_h = compact ? std::min(kRowH, 400.f / n) : kRowH;
    const gfx::Rect pane{x0, kListY, kListW, n * row_h + 2 * kListPad};
    glass_panel(pane, 32, 1.f, true);

    /* Under it, the buttons: test all, the cleanup's switch, the refresh rate, and set up from a phone. */
    const float by = kListY + pane.h + 30;
    const int nb = button_count();
    if (m_button >= nb)
        m_button = nb - 1;
    const app_settings::Refresh refresh = m_ctx.settings.refresh;
    const std::string labels[4] = {i18n::tr(i18n::ButtonTestAll),
                                   i18n::tr(m_ctx.settings.cleanup_enabled ? i18n::CleanupOn : i18n::CleanupOff),
                                   i18n::tr(refresh == app_settings::Refresh::Hz60    ? i18n::Refresh60
                                            : refresh == app_settings::Refresh::Hz120 ? i18n::Refresh120
                                                                                      : i18n::RefreshAuto),
                                   i18n::tr(i18n::ButtonFromPhone)};
    gfx::Rect buttons[4];
    float bx = x0;
    for (int i = 0; i < nb; i++) {
        const float bw = gfx::text_width(labels[i], kButtonText) + 80;
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

    const bool emby = m_ctx.services && m_ctx.services->config().emby();
    const bool both_media = m_ctx.services && m_ctx.services->config().both_media;
    for (int i = 0; i < n; i++) {
        const Row &row = m_rows[i];
        const model::ServiceStatus &st = row.status;
        const float ry = kListY + kListPad + i * row_h;
        if (i > 0 && !(m_focused && (m_focus == i || m_focus == i - 1)))
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
            m_ctx.services ? m_ctx.services->config().at(row.service, row.instance) : config::ServiceConfig{};
        const std::string name = !c.name.empty()                ? c.name
                                 : row.service == config::Jellyfin ? config::media_name(m_ctx.services ? m_ctx.services->config().media
                                                                                                       : config::MediaServer::Jellyfin)
                                                                   : config::service_name(row.service);
        const bool show_url = !c.url.empty() && !m_ctx.mock && !compact;
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
