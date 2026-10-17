/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/config_reload.h"

#include "platform/platform.h"
#include "ui/art.h"
#include "ui/widgets.h"

namespace ui {

bool reload_config(Context &ctx)
{
    if (ctx.mock || !ctx.services)
        return false;
    config::Config next;
    const config::LoadError e = config::load(ctx.config_path, &next);
    if (e != config::LoadError::None) {
        if (ctx.config_error != config::LoadError::None)
            ctx.config_error = e;    /* never had one that worked: same as at start */
        else
            ctx.reload_error = e;    /* keep the one in use */
        return false;
    }
    const bool was_missing = ctx.config_error != config::LoadError::None;
    ctx.config_error = ctx.reload_error = config::LoadError::None;
    const unsigned changed = config::changed(ctx.services->config(), next);
    if (!changed && !was_missing)
        return false;
    ctx.services->reconfigure(next);
    if (ctx.on_config_read)
        ctx.on_config_read();
    i18n::set_media(next.media);
    art::forget(changed);
    ctx.config_gen++;
    notify(i18n::tr(i18n::ConfigReloaded), true, platform::now());
    return true;
}

} // namespace ui
