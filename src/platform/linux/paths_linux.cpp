/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * platform.h on Linux: the clock, assets/ next to the binary (or in the
 * source tree), XDG paths and the locale.
 */
#include "platform/platform.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace platform {

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

namespace {

bool read_file(const std::string &path, std::vector<uint8_t> *out)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    out->clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out->insert(out->end(), buf, buf + n);
    std::fclose(f);
    return true;
}

} // namespace

bool read_asset(const std::string &rel, std::vector<uint8_t> *out)
{
    /* REGIE5_ASSETS, then assets/ beside the binary, then the source tree's. */
    std::vector<std::string> dirs;
    if (const char *env = std::getenv("REGIE5_ASSETS"))
        dirs.push_back(env);
    if (char *base = SDL_GetBasePath()) {
        dirs.push_back(std::string(base) + "assets");
        SDL_free(base);
    }
    dirs.push_back(REGIE5_SOURCE_ASSETS);
    for (const std::string &d : dirs)
        if (read_file(d + "/" + rel, out))
            return true;
    return false;
}

std::string default_config_path()
{
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/regie5/config.json";
    const char *home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/regie5/config.json";
}

std::string system_lang()
{
    for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char *v = std::getenv(var);
        if (v && *v && std::string(v) != "C" && std::string(v) != "POSIX")
            return v;
    }
    return {};
}

bool random_bytes(void *out, size_t n)
{
    FILE *f = std::fopen("/dev/urandom", "rb");
    if (!f)
        return false;
    const bool ok = std::fread(out, 1, n, f) == n;
    std::fclose(f);
    return ok;
}

} // namespace platform
