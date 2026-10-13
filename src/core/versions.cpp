/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/versions.h"

#include <cctype>
#include <cstdlib>

namespace versions {

const char *minimum(config::Service s)
{
    switch (s) {
    case config::Sonarr: return "4.0";     /* v3 is no longer maintained; its API differs in places */
    case config::Radarr: return "5.0";
    case config::Seerr: return "2.0";      /* Jellyseerr 2 and Seerr 3; not Overseerr 1 (no Jellyfin links) */
    case config::Jellyfin: return "10.9";  /* /Items?userId=, the MediaBrowser Token header */
    default: return "";
    }
}

bool compare(const std::string &a, const std::string &b, int *order)
{
    if (a.empty() || b.empty() || !std::isdigit((unsigned char)a[0]) || !std::isdigit((unsigned char)b[0]))
        return false;
    const char *pa = a.c_str(), *pb = b.c_str();
    while (*pa || *pb) {
        char *ea = nullptr, *eb = nullptr;
        const long na = std::isdigit((unsigned char)*pa) ? std::strtol(pa, &ea, 10) : 0;
        const long nb = std::isdigit((unsigned char)*pb) ? std::strtol(pb, &eb, 10) : 0;
        if (na != nb) {
            *order = na < nb ? -1 : 1;
            return true;
        }
        /* the next number: after a dot; anything else ends the version ("4.0.1-beta") */
        pa = ea && *ea == '.' ? ea + 1 : "";
        pb = eb && *eb == '.' ? eb + 1 : "";
    }
    *order = 0;
    return true;
}

bool supported(config::Service s, const std::string &version)
{
    int order = 0;
    return !compare(version, minimum(s), &order) || order >= 0;
}

} // namespace versions
