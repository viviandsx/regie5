/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/phone_share.h"

#include "ui/style.h"

#include "qrcodegen.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ui {

namespace {

/* The console's address that faces the user's servers: the phone is on that network. */
std::string hint_host(const Context &ctx)
{
    std::string hint;
    if (ctx.services)
        for (int s = 0; s < config::ServiceCount && hint.empty(); s++) {
            const std::string &url = ctx.services->config().at((config::Service)s).url;
            const size_t a = url.find("://");
            if (a != std::string::npos)
                hint = url.substr(a + 3, url.find_first_of(":/", a + 3) - a - 3);
        }
    return hint;
}

} // namespace

bool PhoneShare::open(const Context &ctx, const std::string &query)
{
    close();
    m_error.clear();
    m_qr.clear();
    m_qr_size = 0;
    m_saves = 0;
    m_open = true;
    m_demo = ctx.mock;
    std::string address;
    if (m_demo) {   /* --mock: nothing listens, nothing is written */
        m_demo_info = platform::ShareInfo{};
        m_demo_info.running = true;
        m_demo_info.code = "123456";
        m_demo_info.shown = "http://192.168.1.20:8085";
        m_demo_info.address = m_demo_info.shown + "/?c=" + m_demo_info.code;
        address = m_demo_info.address;
    } else {
        std::string why;
        if (!platform::share_start(ctx.config_path, hint_host(ctx), &why)) {
            m_error = why;
            std::fprintf(stderr, "regie5: phone page: %s\n", why.c_str());
            return false;
        }
        const platform::ShareInfo info = platform::share_info();
        address = info.address;
        std::fprintf(stderr, "regie5: phone page open on %s\n", info.shown.c_str());   /* never the code... */
        if (const char *c = std::getenv("REGIE5_SHARE_LOG_CODE"); c && *c == '1')   /* ...but to test it from a script (Linux) */
            std::fprintf(stderr, "regie5: phone page code %s\n", info.code.c_str());
    }
    if (!query.empty())
        address += "&" + query;
    std::vector<uint8_t> tmp(qrcodegen_BUFFER_LEN_MAX), code(qrcodegen_BUFFER_LEN_MAX);
    if (qrcodegen_encodeText(address.c_str(), tmp.data(), code.data(), qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
                             10, qrcodegen_Mask_AUTO, true)) {
        m_qr_size = qrcodegen_getSize(code.data());
        m_qr.resize((size_t)m_qr_size * m_qr_size);
        for (int y = 0; y < m_qr_size; y++)
            for (int x = 0; x < m_qr_size; x++)
                m_qr[(size_t)y * m_qr_size + x] = qrcodegen_getModule(code.data(), x, y);
    }
    return true;
}

void PhoneShare::close()
{
    if (m_open && !m_demo)
        platform::share_stop();
    m_open = false;
}

bool PhoneShare::saved()
{
    if (!m_open || m_demo)
        return false;
    const int now = platform::share_info().saves;
    if (now <= m_saves)
        return false;
    m_saves = now;
    return true;
}

platform::ShareInfo PhoneShare::info() const { return m_demo ? m_demo_info : platform::share_info(); }

std::string PhoneShare::error() const
{
    if (!m_error.empty())
        return m_error;
    const platform::ShareInfo i = info();
    return i.running ? std::string() : i.error.empty() ? std::string("stopped") : i.error;
}

void PhoneShare::draw_qr(float qx, float qy, float qs, float a) const
{
    if (m_qr_size <= 0)
        return;
    gfx::fill({qx, qy, qs, qs}, alpha(0xffffffffu, a), 18);
    const float cell = std::floor(qs / (m_qr_size + 8));   /* a quiet zone of four modules around it */
    const float off = (qs - cell * m_qr_size) / 2;
    for (int y = 0; y < m_qr_size; y++)
        for (int x = 0; x < m_qr_size; x++)
            if (m_qr[(size_t)y * m_qr_size + x])
                gfx::fill({qx + off + x * cell, qy + off + y * cell, cell, cell}, alpha(0xff000000u, a));
}

} // namespace ui
