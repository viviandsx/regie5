/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the platform's shell hands the interface at start.
 */
#pragma once

#include "api/services.h"
#include "core/app_settings.h"
#include "core/config.h"
#include "core/i18n.h"
#include "ui/jelly5_link.h"

#include <string>

namespace ui {

struct Context {
    std::string version;
    /* Optional: told before and after each screen is made and opened (the
     * PS5 probe logs the memory there). */
    void (*trace)(const char *what) = nullptr;
    std::string config_path;
    /* The same file as the user knows it, when that differs (PS5: the title's
     * folder as seen from a PC, not /app0). Empty: config_path. */
    std::string config_shown;
    config::LoadError config_error = config::LoadError::None;
    /* The file was read again and failed: the previous configuration stays. */
    config::LoadError reload_error = config::LoadError::None;
    /* Goes up each time a reread config.json changes a service: the shell
     * then starts the other screens afresh. */
    unsigned config_gen = 0;
    bool mock = false;
    api::Services *services = nullptr;
    /* Health alerts seen come and go during this session (the journal's
     * "Santé"): kept by the Home screen, which reads the servers' health. */
    struct HealthEvent {
        time_t when = 0;
        config::Service from = config::Sonarr;
        int instance = 0;
        bool gone = false;
        model::Health health;
    };
    std::vector<HealthEvent> health_log;
    /* A screen asks to send the user to another tab (one of its views, at a
     * time): the shell does it after the press. tab < 0: nothing asked. */
    struct Goto {
        int tab = -1, view = -1;
        time_t when = 0;
    } go;
    /* Options › « Ouvrir dans Jelly5 », for every screen. */
    Jelly5Link jelly5;
    /* settings.json (the app's own); settings_path empty: not written (--mock). */
    app_settings::Settings settings;
    std::string settings_path;
    /* Bibliothèque › Ménage: off until the user turns it on in Settings. */
    bool cleanup_enabled() const { return settings.cleanup_enabled; }
    void save_settings() const
    {
        if (!settings_path.empty())
            app_settings::save(settings_path, settings);
    }

    /* Why a service is off: turned off in config.json, or no config.json
     * that could be read at all (then Settings says why). */
    const char *off_text(i18n::S turned_off) const
    {
        return config_error != config::LoadError::None ? i18n::tr(i18n::NoConfig) : i18n::tr(turned_off);
    }
};

} // namespace ui
