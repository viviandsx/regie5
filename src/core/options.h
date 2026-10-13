/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Command-line options. On the PS5 there is no command line: the platform
 * fills the same struct with its defaults.
 */
#pragma once

#include <string>

namespace options {

struct Options {
    std::string config_path;   /* --config; empty: the platform's default */
    std::string lang;          /* --lang fr|en; empty: the system's */
    bool mock = false;         /* --mock: data from fixtures/, no server */
    bool fullscreen = false;   /* --fullscreen (desktop) */
    int width = 1280, height = 720;   /* --size WxH (desktop window) */
    bool help = false, version = false;
};

/* Parses argv. Returns false (with *error set) on an unknown or malformed option. */
bool parse(int argc, char **argv, Options *out, std::string *error);

const char *usage();

} // namespace options
