/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/boot_guard.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace boot_guard {

namespace {

/* Read by a crash handler: fixed buffers, never reallocated. */
char s_mark_path[512];
char s_mark_text[24];

bool read_file(const std::string &path, std::string *out, size_t most = 1u << 20)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    out->clear();
    char buf[256];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && out->size() < most)
        out->append(buf, n);
    std::fclose(f);
    return true;
}

void write_file(const std::string &path, const std::string &text)
{
    if (FILE *f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
    }
}

bool is_fingerprint(const std::string &s)
{
    if (s.size() != 16)
        return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}

std::string first_word(const std::string &text)
{
    const size_t end = text.find_first_of(" \t\r\n");
    return text.substr(0, end);
}

} // namespace

std::string fingerprint(const std::string &text)
{
    if (text.empty())
        return {};
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char out[17];
    std::snprintf(out, sizeof out, "%016llx", (unsigned long long)h);
    return out;
}

State parse_state(const std::string &text)
{
    State s;
    int strikes = 0;
    char fp[32] = {};
    if (std::sscanf(text.c_str(), "%d %31s", &strikes, fp) == 2 && strikes > 0 && is_fingerprint(fp)) {
        s.strikes = strikes;
        s.fingerprint = fp;
    }
    return s;
}

std::string format_state(const State &s) { return std::to_string(s.strikes) + " " + s.fingerprint + "\n"; }

State next(const State &before, const std::string &crashed_with, const std::string &current)
{
    if (set_aside(before, current))
        return before;
    State s;
    if (current.empty() || crashed_with != current)
        return s;   /* no crash, a late one, or another file: a fresh start */
    s.fingerprint = current;
    s.strikes = (before.fingerprint == current ? before.strikes : 0) + 1;
    return s;
}

bool set_aside(const State &s, const std::string &current)
{
    return !current.empty() && s.fingerprint == current && s.strikes >= kStrikes;
}

Guard::Guard(std::string dir) : m_dir(std::move(dir))
{
    s_mark_text[0] = '\0';
    s_mark_path[0] = '\0';
    if (!m_dir.empty())
        std::snprintf(s_mark_path, sizeof s_mark_path, "%s/crash-mark", m_dir.c_str());
}

bool Guard::start(const std::string &config_path, std::string *why)
{
    if (m_dir.empty())
        return false;
    std::string config_text;
    read_file(config_path, &config_text);
    const std::string state_path = m_dir + "/boot-guard";
    std::string text, mark;
    const State before = read_file(state_path, &text) ? parse_state(text) : State{};
    const std::string crashed_with = read_file(s_mark_path, &mark) ? first_word(mark) : std::string();
    std::remove(s_mark_path);
    const std::string current = fingerprint(config_text);
    const State after = next(before, crashed_with, current);
    if (after.strikes != before.strikes || after.fingerprint != before.fingerprint) {
        if (after.strikes > 0)
            write_file(state_path, format_state(after));
        else
            std::remove(state_path.c_str());
    }
    const bool aside = set_aside(after, current);
    if (why)
        *why = aside ? "set aside: " + std::to_string(after.strikes) + " crashes in a row at start with this config.json"
               : after.strikes > 0 ? "the last start crashed with this config.json (" + std::to_string(after.strikes) +
                                         " of " + std::to_string(kStrikes) + ")"
               : !crashed_with.empty() ? "the last start crashed, with another config.json"
                                       : "no crash at the last start";
    return aside;
}

void Guard::arm(const std::string &config_path, double now)
{
    if (m_dir.empty())
        return;
    std::string config_text;
    read_file(config_path, &config_text);
    m_fingerprint = fingerprint(config_text);
    m_since = now;
    m_settled = m_fingerprint.empty();
    s_mark_text[0] = '\0';
    if (!m_settled)
        std::snprintf(s_mark_text, sizeof s_mark_text, "%s\n", m_fingerprint.c_str());
}

void Guard::tick(double now)
{
    if (m_settled || now - m_since < kSettle)
        return;
    m_settled = true;
    s_mark_text[0] = '\0';
    /* It holds: whatever strikes it had are gone (a reread that works). */
    const std::string state_path = m_dir + "/boot-guard";
    std::string text;
    if (read_file(state_path, &text) && parse_state(text).fingerprint == m_fingerprint)
        std::remove(state_path.c_str());
}

const char *crash_mark_path() { return s_mark_path; }
const char *crash_mark_text() { return s_mark_text; }

} // namespace boot_guard
