/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Never a crash at every start because of config.json. A crash (a signal, an
 * abort) in the first kSettle seconds after a configuration came into use
 * leaves a mark: the fingerprint of that file's text. kStrikes such starts in
 * a row with the same text, and the next start sets the file aside: it is
 * not loaded (nor changed), Settings says why, the welcome screens open. A
 * new text (the phone page, FTP) or a reread that holds kSettle seconds
 * clears it.
 *
 * Two files in the app's data folder: boot-guard (the strikes and their
 * fingerprint) and crash-mark (written by the crash handler, read and
 * removed at the next start).
 */
#pragma once

#include <string>

namespace boot_guard {

constexpr int kStrikes = 2;
constexpr double kSettle = 30.0;   /* seconds a configuration must hold */

/* 16 hexadecimal digits (FNV-1a, 64 bits); empty for an empty text. */
std::string fingerprint(const std::string &text);

struct State {
    int strikes = 0;
    std::string fingerprint;   /* the configuration those strikes are about */
};
State parse_state(const std::string &text);
std::string format_state(const State &s);

/* The state after a start: crashed_with is the fingerprint the last launch's
 * early crash left (empty: no crash, or a late one); current, the text
 * about to be loaded. A file already set aside stays so until it changes. */
State next(const State &before, const std::string &crashed_with, const std::string &current);
bool set_aside(const State &s, const std::string &current);

/* The files, in one folder (empty: none, nothing written: --mock). */
class Guard {
public:
    explicit Guard(std::string dir);
    /* At start, before the file at config_path is loaded: true if it is to
     * be set aside. *why: a line for the log. */
    bool start(const std::string &config_path, std::string *why);
    /* The file at config_path is in use from now (start, or a reread): a
     * crash in the next kSettle seconds counts against it. */
    void arm(const std::string &config_path, double now);
    /* Each frame: once the configuration has held kSettle seconds, its
     * strikes are cleared and a crash no longer counts. */
    void tick(double now);

private:
    std::string m_dir;
    std::string m_fingerprint;
    double m_since = 0;
    bool m_settled = true;
};

/* For a crash handler (plain C strings, async-signal-safe to read): where
 * to write the mark, and what (empty: nothing, no configuration is new). */
const char *crash_mark_path();
const char *crash_mark_text();

} // namespace boot_guard
