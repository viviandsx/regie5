/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Runs every TEST. No network: everything comes from fixtures/.
 */
#include "check.h"

int main(int argc, char **argv)
{
    const std::string only = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (const check::Test &t : check::registry()) {
        if (!only.empty() && std::string(t.name).find(only) == std::string::npos)
            continue;
        const int before = check::failures();
        t.fn();
        std::printf("%s %s\n", check::failures() == before ? "ok  " : "FAIL", t.name);
        ran++;
    }
    std::printf("\n%d tests, %d failed checks\n", ran, check::failures());
    return check::failures() == 0 ? 0 : 1;
}
