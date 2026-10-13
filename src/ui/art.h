/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pictures on the GPU, after Jelly5's art cache: posters and backdrops are
 * fetched from the services themselves (Sonarr/Radarr mediacover, Seerr's
 * image proxy, Jellyfin), decoded and scaled on worker threads, uploaded a
 * few per frame so scrolling never hitches, and kept in an LRU cache with a
 * memory budget. A picture that fails stays failed until a refresh: the
 * screens draw a generated card in its place, never a broken image.
 */
#pragma once

#include "api/http.h"
#include "api/services.h"
#include "core/config.h"
#include "ui/gfx.h"

#include <string>

namespace art {

/* A picture: a path on one of the services, or nothing (path empty). */
struct Ref {
    config::Service service = config::Sonarr;
    std::string path;
    int instance = 0;   /* which Sonarr / Radarr */
    bool empty() const { return path.empty(); }
};

/* Once, before the first frame. images: a pool of its own, so pictures never
 * hold up the API calls. */
void init(api::Services *services, http::Pool *images);
/* Bytes of pictures kept on the GPU before the least recent go (192 MiB by
 * default; less on a console). */
void set_budget(size_t bytes);

/* Call at the start of every frame: uploads what arrived, evicts what is old. */
void frame();

/* The texture once it is ready (asking for it as needed), else null. max_w,
 * max_h bound the decoded size (in pixels). blur > 0: a small, blurred copy
 * for the ambient background. */
const gfx::Texture *get(const Ref &ref, int max_w, int max_h, int blur = 0);
/* No picture will come for this one (missing, or the server failed). */
bool failed(const Ref &ref, int max_w, int max_h, int blur = 0);
/* 0..1: how far the picture has faded in since it was ready. */
float fade(const Ref &ref, int max_w, int max_h, int blur = 0, float seconds = 0.3f);
/* Whether anything is fading in or on its way (the loop keeps drawing). */
bool busy();

/* Forget failures (a refresh: the server may be back). */
void retry_failed();
/* Forget everything from these services (bit i: service i): config.json gave
 * them another address or key. Pictures still on their way are ignored. */
void forget(unsigned services);

/* Draws ref into r: the picture fading in over a placeholder, or the
 * generated card (title on a gradient) when there is none. */
void draw_poster(const gfx::Rect &r, const Ref &ref, const std::string &title, uint32_t seed, float radius,
                 float opacity, int px_w = 360);

} // namespace art
