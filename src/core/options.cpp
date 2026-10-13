/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/options.h"

#include "core/i18n.h"

#include <cstdio>
#include <cstring>

namespace options {

bool parse(int argc, char **argv, Options *out, std::string *error)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        auto value = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                *error = std::string(name) + " needs a value";
                return nullptr;
            }
            return argv[++i];
        };
        if (!std::strcmp(a, "--config")) {
            const char *v = value(a);
            if (!v)
                return false;
            out->config_path = v;
        } else if (!std::strcmp(a, "--lang")) {
            const char *v = value(a);
            if (!v)
                return false;
            if (!i18n::known(v)) {
                *error = std::string("--lang: no language ") + v + " (src/core/lang/)";
                return false;
            }
            out->lang = v;
        } else if (!std::strcmp(a, "--size")) {
            const char *v = value(a);
            if (!v)
                return false;
            int w = 0, h = 0;
            if (std::sscanf(v, "%dx%d", &w, &h) != 2 || w < 320 || h < 180) {
                *error = "--size takes WIDTHxHEIGHT, e.g. 1920x1080";
                return false;
            }
            out->width = w, out->height = h;
        } else if (!std::strcmp(a, "--mock")) {
            out->mock = true;
        } else if (!std::strcmp(a, "--demo")) {
            out->mock = out->demo = true;
        } else if (!std::strcmp(a, "--fullscreen")) {
            out->fullscreen = true;
        } else if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            out->help = true;
        } else if (!std::strcmp(a, "--version")) {
            out->version = true;
        } else {
            *error = std::string("unknown option: ") + a;
            return false;
        }
    }
    return true;
}

const char *usage()
{
    return "Usage: regie5 [options]\n"
           "  --config <path>   configuration file (default: ~/.config/regie5/config.json)\n"
           "  --lang <code>     interface language: en, fr… (default: the system's)\n"
           "  --mock            run on fixtures/, without any server\n"
           "  --demo            run on demo/: free films with their posters, no server\n"
           "  --size WxH        window size (default 1280x720)\n"
           "  --fullscreen      fullscreen window\n"
           "  --version         print the version\n"
           "\n"
           "Keys: arrows move, Enter selects, Esc/Backspace goes back, Q/E change tab,\n"
           "R refreshes, M opens the menu. A DualSense works over USB or Bluetooth.\n";
}

} // namespace options
