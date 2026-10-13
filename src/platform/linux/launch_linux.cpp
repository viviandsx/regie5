/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * No other app to start on the desktop: the launch is only told in the log,
 * as the PS5 would make it (the handoff file is not written).
 * REGIE5_NO_LAUNCH=1 plays a console that starts no other app: Options then
 * offers « Chercher dans Jelly5 ».
 */
#include "platform/platform.h"

#include <cstdio>
#include <cstdlib>

namespace platform {

bool can_launch_apps()
{
    static const bool none = [] {
        const char *v = std::getenv("REGIE5_NO_LAUNCH");
        return v && *v && *v != '0';
    }();
    return !none;
}

bool launch_app(const Launch &l, std::string *error)
{
    std::string args;
    for (const auto &a : l.args)
        args += " " + a;
    std::fprintf(stderr, "launch (simulated): %s%s\n", l.title_id.c_str(), args.c_str());
    if (!l.handoff_path.empty())
        std::fprintf(stderr, "launch (simulated): would leave %s: %s\n", l.handoff_path.c_str(), l.handoff.c_str());
    return true;
}

} // namespace platform
