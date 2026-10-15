/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Typing a text on Linux: SDL's text input while an entry is open
 * (platform.h). input_sdl hands it the keyboard's events.
 */
#include "platform/linux/text_linux.h"

#include "platform/platform.h"

#include <SDL.h>

namespace {

bool s_active = false, s_over = false, s_confirmed = false;
std::string s_text;

/* The last UTF-8 character off the end. */
void erase_last(std::string *s)
{
    while (!s->empty() && ((unsigned char)s->back() & 0xc0) == 0x80)
        s->pop_back();
    if (!s->empty())
        s->pop_back();
}

void finish(bool confirmed)
{
    s_active = false;
    s_over = true;
    s_confirmed = confirmed;
    SDL_StopTextInput();
}

} // namespace

namespace text_linux {

bool handle_event(const SDL_Event &ev)
{
    if (!s_active)
        return false;
    if (ev.type == SDL_TEXTINPUT) {
        if (s_text.size() < 256)
            s_text += ev.text.text;
        return true;
    }
    if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
        case SDLK_BACKSPACE: erase_last(&s_text); break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: finish(true); break;
        case SDLK_ESCAPE: finish(false); break;
        default: break;
        }
        return true;
    }
    return ev.type == SDL_KEYUP;
}

void type(const std::string &text)
{
    if (!s_active)
        return;
    s_text = text;
    finish(true);
}

} // namespace text_linux

namespace platform {

bool text_entry_start(TextKind, const std::string &, const std::string &initial)
{
    if (s_active)
        return false;
    s_active = true;
    s_over = false;
    s_text = initial;
    SDL_StartTextInput();
    return true;
}

bool text_entry_active() { return s_active; }
std::string text_entry_text() { return s_text; }

bool text_entry_finished(bool *confirmed, std::string *text)
{
    if (!s_over)
        return false;
    s_over = false;
    *confirmed = s_confirmed;
    *text = s_text;
    return true;
}

void text_entry_cancel()
{
    if (s_active)
        finish(false);
    s_over = false;
}

} // namespace platform
