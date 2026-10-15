/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the portable code asks of the platform, besides drawing (ui/gfx.h) and
 * input (core/input.h). Implemented in platform/<name>/.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace platform {

/* Monotonic seconds. */
double now();

/* One of the app's files (assets/fonts/Inter-Bold.ttf ...), whole. On the
 * desktop it is read from assets/; on the PS5 it may be built into the binary. */
bool read_asset(const std::string &rel, std::vector<uint8_t> *out);

/* Where config.json is when no path was given (Linux: ~/.config/regie5/config.json). */
std::string default_config_path();

/* The system's language code ("fr_FR", "en-US" ...), empty when unknown. */
std::string system_lang();

/* The picture as it goes out: its size and refresh rate. asked_hz: what the
 * app asked for at start (0: it cannot choose, as on the desktop, where the
 * monitor decides). hz is what the display accepted (a TV without 120 Hz
 * stays at 60). */
struct DisplayMode {
    int width = 0, height = 0;
    int hz = 0, asked_hz = 0;
    /* false: this build cannot ask for 120 Hz at all (on the PS5, its
     * param.json does not declare it: 0.3.1 went back to 0.2.0's) */
    bool can_120 = true;
};
DisplayMode display_mode();

/* Starting another app (Jelly5 on a title's page): its Title ID, its
 * arguments, and a file left for it first where both apps can read it
 * (handoff_path empty: none). */
struct Launch {
    std::string title_id;
    std::vector<std::string> args;
    std::string handoff_path, handoff;
};
/* Whether this platform can start other apps at all (else the menu entry is hidden). */
bool can_launch_apps();
/* false, with *error, when it could not. Linux: simulated, one line in the log. */
bool launch_app(const Launch &l, std::string *error);

/* Random bytes from the system (a one-time code, TLS). false if it has none. */
bool random_bytes(void *out, size_t n);

/* Configuring from a phone (phase 6.5, core/config_web.h): a page served on
 * the local network while Réglages shows its QR code. One server at a time;
 * the page writes config_path. Implemented once for both platforms
 * (platform/posix/config_server.cpp). */
struct ShareInfo {
    bool running = false;
    std::string address;   /* http://<console>:<port>/?c=<code>, for the QR code */
    std::string shown;     /* the same without the code, to type by hand */
    std::string code;      /* six digits, shown on the TV */
    std::string error;     /* why it stopped or could not start (English, for the log) */
    int saves = 0;         /* configurations written since it started */
    bool phone_seen = false;
    std::string kept_invalid;   /* where an unreadable config.json was kept before the first save */
};
/* Whether a page can be served here at all (else the button is hidden). */
bool can_share_config();
/* hint_host: a server of the user's (the console's address facing it is the
 * one the phone can reach). The page is in the interface's language. */
bool share_start(const std::string &config_path, const std::string &hint_host, std::string *error);
void share_stop();
ShareInfo share_info();

} // namespace platform
