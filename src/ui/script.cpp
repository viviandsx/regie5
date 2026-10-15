/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/script.h"

#include "core/input.h"

#include <cstdio>
#include <cstdlib>

namespace ui {

Script::Script(const char *text, Grab grab) : m_grab(grab)
{
    if (!text)
        return;
    std::string tok;
    for (const char *p = text;; p++) {
        if (*p && *p != ' ' && *p != ',') {
            tok += *p;
            continue;
        }
        if (!tok.empty())
            m_steps.push_back(tok);
        tok.clear();
        if (!*p)
            break;
    }
}

uint32_t Script::step(double now, bool *quit)
{
    uint32_t pressed = 0;
    while (m_next < m_steps.size() && now >= m_wait_until && m_shot.empty()) {
        const std::string &s = m_steps[m_next++];
        char *end = nullptr;
        const double secs = std::strtod(s.c_str(), &end);
        if (end && *end == '\0' && end != s.c_str()) {
            m_wait_until = now + secs;
        } else if (s.rfind("shot:", 0) == 0) {
            m_shot = s.substr(5);
        } else if (s.rfind("text:", 0) == 0) {
            m_text = s.substr(5);
            for (char &c : m_text)
                if (c == '_')
                    c = ' ';
            m_wait_until = now + 0.05;
            break;
        } else if (s == "quit") {
            *quit = true;
        } else if (const uint32_t b = button(s)) {
            pressed |= b;
            m_wait_until = now + 0.05;   /* one press a frame or so */
            break;
        } else {
            std::fprintf(stderr, "script: unknown step \"%s\"\n", s.c_str());
        }
    }
    return pressed;
}

void Script::after_frame()
{
    if (m_shot.empty())
        return;
    std::vector<uint8_t> rgb;
    int w = 0, h = 0;
    if (m_grab && m_grab(&rgb, &w, &h)) {
        if (FILE *f = std::fopen(m_shot.c_str(), "wb")) {
            std::fprintf(f, "P6\n%d %d\n255\n", w, h);
            std::fwrite(rgb.data(), 1, rgb.size(), f);
            std::fclose(f);
            std::fprintf(stderr, "script: saved %s\n", m_shot.c_str());
        }
    }
    m_shot.clear();
}

uint32_t Script::button(const std::string &s)
{
    static const struct {
        const char *name;
        uint32_t bit;
    } kNames[] = {{"up", input::Up},         {"down", input::Down},   {"left", input::Left},
                  {"right", input::Right},   {"cross", input::Cross}, {"circle", input::Circle},
                  {"triangle", input::Triangle}, {"square", input::Square}, {"l1", input::L1},
                  {"r1", input::R1},         {"options", input::Options}, {"l2", input::L2},
                  {"r2", input::R2}};
    for (const auto &n : kNames)
        if (s == n.name)
            return n.bit;
    return 0;
}

} // namespace ui
