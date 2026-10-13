/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * settings.json, beside config.json: what the app itself remembers (the user
 * never needs to edit it). The cleanup is off until turned on in Settings;
 * its criteria; the titles kept out of it.
 */
#pragma once

#include "core/config.h"

#include <string>
#include <vector>

namespace app_settings {

/* A title the user chose to keep: never offered for deletion again. */
struct Kept {
    config::Service service = config::Radarr;
    std::string instance;   /* the instance's name, as in config.json */
    int id = 0;             /* Radarr's movie id, Sonarr's series id */
    std::string title;
};

struct Settings {
    bool cleanup_enabled = false;
    bool cleanup_everyone = true;   /* watched by every active user (else by at least one) */
    int cleanup_months = 6;         /* last watched more than this long ago */
    std::vector<Kept> kept;

    bool is_kept(config::Service s, const std::string &instance, int id) const;
};

/* Where it lives: settings.json in config.json's folder. */
std::string path_beside(const std::string &config_path);

/* A missing file gives the defaults (and true); an unreadable one, false and
 * the defaults. */
bool load(const std::string &path, Settings *out);
/* Written whole, then renamed over the old one (never half a file). */
bool save(const std::string &path, const Settings &s);

std::string to_json(const Settings &s);
bool from_json(const std::string &text, Settings *out);

} // namespace app_settings
