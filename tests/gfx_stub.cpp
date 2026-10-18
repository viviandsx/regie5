/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The renderer's backend for the interface's tests: no GPU. Textures are
 * counted (made, released, their bytes) so the picture cache can be checked;
 * everything drawn is dropped. Assets (the fonts) come from the source tree.
 */
#include "gfx_stub.h"

#include <cstdio>

#include "platform/platform.h"
#include "ui/gfx.h"

namespace gfx {

struct Texture {
    int w = 0, h = 0;
};

float scale() { return 1.f; }
void begin_frame() { gfx_stub::frames++; }
void end_frame() {}
uint64_t frame_index() { return gfx_stub::frames; }

Texture *texture_from_pixels(const uint8_t *, int w, int h, int)
{
    gfx_stub::made++;
    gfx_stub::live++;
    gfx_stub::live_bytes += (size_t)w * h * 4;
    return new Texture{w, h};
}

void texture_release(Texture *t)
{
    if (!t)
        return;
    gfx_stub::live--;
    gfx_stub::live_bytes -= (size_t)t->w * t->h * 4;
    delete t;
}

int texture_width(const Texture *t) { return t ? t->w : 0; }
int texture_height(const Texture *t) { return t ? t->h : 0; }
void texture_update(Texture *, int, int, int, int, const uint8_t *, int) {}
void set_solid_source(const Texture *, float, float) {}
void push_opacity(float) {}
void pop_opacity() {}
void push_scissor(const Rect &) {}
void pop_scissor() {}
void fill(const Rect &, uint32_t, float) {}
void fill_vgradient(const Rect &, uint32_t, uint32_t, float) {}
void fill_hgradient(const Rect &, uint32_t, uint32_t, float) {}
void image(const Rect &, const Texture *, float, float, bool) {}
void image_uv(const Rect &, const Texture *, float, float, float, float, float, float) {}
void image_tint(const Rect &, const Texture *, uint32_t) {}
void image_tint_uv(const Rect &, const Texture *, float, float, float, float, uint32_t) {}
int backdrop_blur(const Rect &, float, float, float, float) { return 0; }
bool set_glass_light(float, float) { return false; }
void rim(const Rect &, float, float) {}
void shadow(const Rect &, float, float, float, float) {}

} // namespace gfx

namespace gfx_stub {
uint64_t frames = 0;
int made = 0, live = 0;
size_t live_bytes = 0;
} // namespace gfx_stub

namespace platform {
bool read_asset(const std::string &rel, std::vector<uint8_t> *out)
{
    FILE *f = std::fopen((std::string(REGIE5_ASSETS) + "/" + rel).c_str(), "rb");
    if (!f)
        return false;
    out->clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out->insert(out->end(), buf, buf + n);
    std::fclose(f);
    return true;
}
} // namespace platform
