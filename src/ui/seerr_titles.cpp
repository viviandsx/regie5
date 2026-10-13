/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/seerr_titles.h"

#include "api/seerr.h"
#include "core/i18n.h"
#include "platform/platform.h"

namespace ui {

const model::MediaDetail *SeerrTitles::get(bool tv, int tmdb_id)
{
    if (tmdb_id <= 0 || !m_ctx.services)
        return nullptr;
    const std::string k = key(tv, tmdb_id);
    auto it = m_details.find(k);
    if (it == m_details.end()) {
        m_details[k].state = load::Loading;
        std::weak_ptr<bool> alive = m_alive;
        m_ctx.services->detail(tv, tmdb_id, [this, alive, k](api::Result<model::MediaDetail> res) {
            if (!alive.expired())
                m_details[k].take(std::move(res), platform::now());
        });
        return nullptr;
    }
    return it->second.ready() ? &it->second.value : nullptr;
}

bool SeerrTitles::failed(bool tv, int tmdb_id) const
{
    auto it = m_details.find(key(tv, tmdb_id));
    return tmdb_id <= 0 || (it != m_details.end() && it->second.state == load::Failed);
}

std::string SeerrTitles::title(bool tv, int tmdb_id)
{
    if (const model::MediaDetail *d = get(tv, tmdb_id))
        return d->title;
    return std::string(i18n::tr(tv ? i18n::KindSeries : i18n::KindMovie)) + " #" + std::to_string(tmdb_id);
}

bool SeerrTitles::backdrop(bool tv, int tmdb_id, std::string *k, art::Ref *ref, int *seed) const
{
    auto it = m_details.find(key(tv, tmdb_id));
    if (it == m_details.end() || !it->second.ready())
        return false;
    *ref = {config::Seerr, seerr::image_path(it->second.value.backdrop_path, "w300")};
    *k = "seerr:" + key(tv, tmdb_id);
    *seed = tmdb_id % 997;
    return true;
}

} // namespace ui
