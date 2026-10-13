/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The ambient background: a big soft picture (the focused title's backdrop,
 * small and blurred, then stretched), dimmed, cross-faded as focus moves.
 * After Jelly5's ui::Ambient (app/src/ui/screen.cpp, GPL-3.0-or-later), which
 * shows a BlurHash; Sonarr, Radarr and Seerr have none, so this one fetches a
 * small backdrop from the service and blurs it.
 */
#pragma once

#include "ui/anim.h"
#include "ui/art.h"
#include "ui/gfx.h"

#include <string>

namespace ui {

class Ambient {
public:
    /* key names the picture: ref (a backdrop on a service, fetched small and
     * blurred), or the generated one for seed when there is none or it fails.
     * Changes only once focus has rested (0.35 s) and the picture is there,
     * then fades slowly: fast scrolling never makes the background flicker. */
    void set(const std::string &key, const art::Ref &ref, int seed, double now);
    /* dim: 0..1, how much of the background colour goes over it. */
    void draw(float dt, float dim, bool *animating);

private:
    struct Pic {
        std::string key;
        art::Ref ref;
        int seed = 0;
        bool empty() const { return key.empty(); }
    };
    /* The texture for a picture; null while it is still coming. */
    static const gfx::Texture *texture(const Pic &p);
    Pic m_cur, m_next, m_pending;
    double m_pending_since = 0, m_now = 0;
    Anim m_mix;
};

/* A made-up backdrop for when there is no picture (and for the empty screens of
 * the first phase): soft glows of a few colours picked by seed. Cached. */
const gfx::Texture *generated_backdrop(int seed);

} // namespace ui
