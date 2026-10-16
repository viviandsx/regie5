/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/welcome.h"

namespace welcome {

bool configured(const config::Config &cfg)
{
    for (config::Service s : {config::Sonarr, config::Radarr, config::Seerr, config::Jellyfin})
        for (int i = 0; i < cfg.count(s); i++)
            if (cfg.at(s, i).enabled)
                return true;
    return false;
}

bool should_show(const Start &s, const config::Config &cfg)
{
    if (s.forced)
        return true;
    if (s.error == config::LoadError::SetAside && !s.mock)
        return true;   /* the file to fix: whatever was chosen before */
    if (s.mock || s.later || s.from_backup)
        return false;
    return s.error != config::LoadError::None || !configured(cfg);
}

} // namespace welcome
