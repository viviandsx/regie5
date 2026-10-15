/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/whats_new.h"

#include <cctype>

namespace whats_new {

std::string series(const std::string &version)
{
    /* digits '.' digits, then the end, a '.' or a '-' */
    size_t i = 0;
    while (i < version.size() && std::isdigit((unsigned char)version[i]))
        i++;
    if (i == 0 || i >= version.size() || version[i] != '.')
        return "";
    size_t j = i + 1;
    while (j < version.size() && std::isdigit((unsigned char)version[j]))
        j++;
    if (j == i + 1 || (j < version.size() && version[j] != '.' && version[j] != '-'))
        return "";
    return version.substr(0, j);
}

bool should_show(const Start &s)
{
    if (s.forced)
        return true;
    if (s.mock || s.welcome || !s.ran_before)
        return false;
    const std::string now = series(s.current);
    return !now.empty() && series(s.seen) != now;
}

} // namespace whats_new
