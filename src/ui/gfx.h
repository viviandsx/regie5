/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The UI renderer: immediate-mode 2D on the GPU. Adapted from Jelly5's
 * gfx.h (https://github.com/02dnot/Jelly5, GPL-3.0-or-later), kept close to
 * it so Jelly5's PS5 backend (gfx_agc.cpp) can stand behind it too.
 *
 * Everything is laid out in a 1920x1080 logical space and drawn at the
 * output's resolution (scale() = 2 on a 4K panel), so geometry and text are
 * sharp. Every quad has anti-aliased rounded corners for free.
 *
 * Colours are 0xAARRGGBB with straight alpha; the renderer premultiplies.
 * Pixels handed to texture_from_pixels are premultiplied RGBA bytes.
 *
 * Implemented by platform/gl/gfx_gl.cpp (OpenGL 3.3 core / GLES 3.0; on the PS5 inside a 4.6 core context); the text
 * functions by ui/gfx_text.cpp on top of the rest.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace gfx {

constexpr float W = 1920.f, H = 1080.f;   /* logical canvas */

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

/* A GPU texture (RGBA8, premultiplied). Owned by the renderer. */
struct Texture;

/* Output pixels per logical pixel (2 on a 4K panel, 0.667 in a 1280x720 window). */
float scale();

void begin_frame();   /* clears to the background colour */
void end_frame();     /* presents */

/* Textures. rgba: premultiplied, 4 bytes a pixel, stride in pixels; null: the
 * pixels are undefined until texture_update writes them. */
Texture *texture_from_pixels(const uint8_t *rgba, int w, int h, int stride_px);
void texture_release(Texture *t);
int texture_width(const Texture *t);
int texture_height(const Texture *t);
/* Replaces a w x h region of t at (x, y) (the text atlas, a glyph at a time). */
void texture_update(Texture *t, int x, int y, int w, int h, const uint8_t *rgba, int stride_px);
/* Solid fills, rims and shadows read the opaque white texel at (u, v) of t
 * rather than a texture of their own, so they batch with the text drawn from
 * t (the atlas). Null: back to their own. */
void set_solid_source(const Texture *t, float u, float v);

/* Multiply the opacity of everything that follows (nests). */
void push_opacity(float a);
void pop_opacity();

/* Clip everything that follows to r (axis-aligned, intersected with the outer clip). */
void push_scissor(const Rect &r);
void pop_scissor();

/* A rectangle with rounded corners, a solid colour. */
void fill(const Rect &r, uint32_t color, float radius = 0);
/* A vertical gradient (top to bottom colours). */
void fill_vgradient(const Rect &r, uint32_t top, uint32_t bottom, float radius = 0);
/* A horizontal gradient (left to right colours). */
void fill_hgradient(const Rect &r, uint32_t left, uint32_t right, float radius = 0);
/* A texture into r: stretched, or cropped to fill it (cover). */
void image(const Rect &r, const Texture *t, float opacity = 1, float radius = 0, bool cover = true);
/* Part of a texture (u, v in 0..1). */
void image_uv(const Rect &r, const Texture *t, float u0, float v0, float u1, float v1, float opacity = 1,
              float radius = 0);
/* A texture used as a mask (text): its alpha in the colour given. */
void image_tint(const Rect &r, const Texture *t, uint32_t color);
/* Part of a mask texture (a glyph in the atlas). */
void image_tint_uv(const Rect &r, const Texture *t, float u0, float v0, float u1, float v1, uint32_t color);

/* Frosted glass: blurs what is already drawn under r and puts it back into r's
 * rounded shape. Returns 0 (nothing drawn), 1 (the blurred backdrop: draw a
 * tint and rim over it) or 2 (the whole liquid glass pane, by its shader).
 * lift: 0 a pane, 1 the brighter glass of the focus drop. */
int backdrop_blur(const Rect &r, float radius, float sigma = 24.f, float opacity = 1.f, float lift = 0.f);
/* Where the glass's light comes from (screen direction, x right, y down).
 * Returns true when it moved enough to redraw. */
bool set_glass_light(float x, float y);
/* The lit rim of a glass pane: a thin line of light around r's rounded shape. */
void rim(const Rect &r, float radius, float opacity = 1.f);
/* A soft drop shadow for a rounded box at r. */
void shadow(const Rect &r, float radius, float blur, float opacity, float dy = 0);

/* Text. Each glyph is rasterised once into an atlas; each (text, style) is
 * laid out once, then cached as quads into it. */
enum Weight { Regular, Medium, SemiBold, Bold };

struct TextStyle {
    Weight weight = Medium;
    float size = 26;     /* logical px */
    float max_w = 0;     /* single line: ellipsis past this */
    int max_lines = 1;   /* > 1: wrap within max_w */
    float line_h = 0;    /* wrapped line advance (0 = 1.4 x size) */
};

/* Draws text with its first baseline at (x, baseline). Returns the width. */
float text(float x, float baseline, const std::string &s, const TextStyle &st, uint32_t color,
           int align = 0 /* 0 left, 1 centre, 2 right */);
float text_width(const std::string &s, const TextStyle &st);
/* Texts kept laid out. */
size_t text_cache_size();
/* Bytes of glyph atlas pages (GPU) before it starts afresh: 48 MiB by default. */
void set_atlas_budget(size_t bytes);
/* Times the atlas started afresh (each one re-rasterises what is on screen). */
uint64_t atlas_resets();
/* Rasterises chars in that style ahead of time (at the current scale). */
void prewarm_glyphs(const std::string &chars, const TextStyle &st);

/* Drops cached texts that were not drawn for a while; starts the atlas afresh
 * when it has grown too big (call between frames). */
void collect();

/* The frame being drawn: counts begin_frame. */
uint64_t frame_index();

} // namespace gfx
