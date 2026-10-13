/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Titles, years and pictures of what Seerr names only by its TMDB id (a
 * request, an issue): asked once each, through Seerr, and kept.
 */
#pragma once

#include "core/model.h"
#include "ui/art.h"
#include "ui/context.h"
#include "ui/load.h"

#include <map>
#include <memory>
#include <string>

namespace ui {

class SeerrTitles {
public:
    explicit SeerrTitles(Context &ctx) : m_ctx(ctx) {}

    /* The details once there (nullptr meanwhile: they are asked for). */
    const model::MediaDetail *get(bool tv, int tmdb_id);
    /* They could not be had: a generated card instead of a poster. */
    bool failed(bool tv, int tmdb_id) const;
    /* The title, else "Film #123" while it comes or when it cannot. */
    std::string title(bool tv, int tmdb_id);
    /* The backdrop for the ambient background, when the details are there. */
    bool backdrop(bool tv, int tmdb_id, std::string *key, art::Ref *ref, int *seed) const;

private:
    static std::string key(bool tv, int tmdb_id) { return (tv ? "t:" : "m:") + std::to_string(tmdb_id); }

    Context &m_ctx;
    std::map<std::string, Load<model::MediaDetail>> m_details;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
