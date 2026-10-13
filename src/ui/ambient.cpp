/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/ambient.h"

#include "ui/style.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace ui {

namespace {
constexpr int kBackW = 192, kBackH = 108, kBackBlur = 3;
} // namespace

const gfx::Texture *Ambient::texture(const Pic &p)
{
    if (p.ref.empty() || art::failed(p.ref, kBackW, kBackH, kBackBlur))
        return generated_backdrop(p.seed);
    return art::get(p.ref, kBackW, kBackH, kBackBlur);
}

void Ambient::set(const std::string &key, const art::Ref &ref, int seed, double now)
{
    m_now = now;
    if (key.empty())
        return;
    if (m_cur.empty()) {
        m_cur = {key, ref, seed};
        return;
    }
    if (key != m_pending.key) {
        m_pending = {key, ref, seed};
        m_pending_since = now;
    }
    texture(m_pending);   /* start fetching it now */
}

void Ambient::draw(float dt, float dim, bool *animating)
{
    /* Settled on a new picture (and it is there): start (or redirect) the fade.
     * Back on the one a fade is leaving: it turns round from where it is. */
    const bool waiting = !m_pending.empty() && (m_pending.key != m_cur.key || !m_next.empty()) &&
                         m_pending.key != m_next.key;
    if (waiting && animating)
        *animating = true;
    if (waiting && m_now - m_pending_since > 0.35 && texture(m_pending)) {
        if (m_pending.key == m_cur.key) {
            std::swap(m_cur, m_next);
            m_mix.snap(1.f - m_mix.value);
        } else {
            if (!m_next.empty() && m_mix.value > 0.5f)
                m_cur = m_next;   /* mostly there already: that becomes the base */
            m_next = m_pending;
            m_mix.snap(0.f);
        }
        m_mix.to(1.f);
    }
    const gfx::Rect full{0, 0, gfx::W, gfx::H};
    gfx::fill(full, kBg);
    const gfx::Texture *cur = texture(m_cur);
    if (cur)
        gfx::image(full, cur, 1.f, 0, true);
    if (!m_next.empty()) {
        if (m_mix.step(dt, 3.2f)) {
            if (animating)
                *animating = true;
        } else {
            m_cur = m_next;
            m_next = {};
        }
        if (const gfx::Texture *t = texture(m_next))
            gfx::image(full, t, smoothstep(m_mix.value), 0, true);
    }
    gfx::fill(full, alpha(kBg, dim));
    /* A darker top and bottom, for the tab bar and the hints. */
    gfx::fill_vgradient({0, 0, gfx::W, 260}, alpha(kBg, 0.55f), alpha(kBg, 0.f));
    gfx::fill_vgradient({0, gfx::H - 220, gfx::W, 220}, alpha(kBg, 0.f), alpha(kBg, 0.6f));
}

const gfx::Texture *generated_backdrop(int seed)
{
    static std::map<int, gfx::Texture *> s_cache;
    auto it = s_cache.find(seed);
    if (it != s_cache.end())
        return it->second;

    /* Small (it is stretched to the screen, linear filtering keeps it soft). */
    const int w = 192, h = 108;
    uint32_t state = 2166136261u ^ (uint32_t)seed * 16777619u;
    auto rnd = [&state]() {
        state ^= state << 13, state ^= state >> 17, state ^= state << 5;
        return (state & 0xffffff) / 16777216.f;
    };
    auto hue = [](float hh, float s, float v, float out[3]) {
        const float c = v * s, x = c * (1 - std::fabs(std::fmod(hh * 6, 2.f) - 1)), m = v - c;
        const int k = (int)(hh * 6) % 6;
        const float rgb[6][3] = {{c, x, 0}, {x, c, 0}, {0, c, x}, {0, x, c}, {x, 0, c}, {c, 0, x}};
        for (int i = 0; i < 3; i++)
            out[i] = rgb[k][i] + m;
    };
    struct Blob {
        float x, y, r, col[3];
    };
    const float base_hue = rnd();
    std::vector<Blob> blobs;
    for (int i = 0; i < 4; i++) {
        Blob b;
        b.x = rnd() * w, b.y = rnd() * h, b.r = (0.25f + 0.25f * rnd()) * w;
        hue(std::fmod(base_hue + (i % 2 ? 0.12f : -0.08f) * (1 + rnd()) + 1.f, 1.f), 0.65f + 0.3f * rnd(),
            0.55f + 0.35f * rnd(), b.col);
        blobs.push_back(b);
    }
    std::vector<uint8_t> px((size_t)w * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float c[3] = {0.035f, 0.035f, 0.05f};
            for (const Blob &b : blobs) {
                const float dx = (x - b.x) / b.r, dy = (y - b.y) / b.r;
                const float k = std::exp(-(dx * dx + dy * dy) * 1.6f);
                for (int i = 0; i < 3; i++)
                    c[i] += b.col[i] * k * 0.8f;
            }
            uint8_t *p = &px[((size_t)y * w + x) * 4];
            for (int i = 0; i < 3; i++)
                p[i] = (uint8_t)std::lround(255.f * std::min(1.f, c[i]));
            p[3] = 255;
        }
    gfx::Texture *t = gfx::texture_from_pixels(px.data(), w, h, w);
    s_cache[seed] = t;
    return t;
}

} // namespace ui
