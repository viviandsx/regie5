/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The few platform.h functions the tests' POSIX parts call (the socket
 * transport, the phone page's server), without SDL.
 */
#include "platform/platform.h"

#include <cstdio>
#include <ctime>

namespace platform {

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
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
