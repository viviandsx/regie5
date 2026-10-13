/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/linux/input_sdl.h"

#include "core/input.h"

#include <cstdio>
#include <map>

namespace input_sdl {

namespace {

uint32_t s_went_down = 0;
uint32_t s_keys = 0, s_pad = 0, s_stick = 0;
std::map<SDL_JoystickID, SDL_GameController *> s_controllers;

/* Letters by what is printed on the key (an AZERTY Q is Q). */
uint32_t key_button(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: return input::Up;
    case SDLK_DOWN: return input::Down;
    case SDLK_LEFT: return input::Left;
    case SDLK_RIGHT: return input::Right;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return input::Cross;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE: return input::Circle;
    case SDLK_q: return input::L1;
    case SDLK_e: return input::R1;
    case SDLK_r: return input::Triangle;
    case SDLK_m: return input::Options;
    case SDLK_z: return input::L2;
    case SDLK_c: return input::R2;
    default: return 0;
    }
}

/* SDL names the face buttons by position (A is the bottom one: Cross). */
uint32_t pad_button(Uint8 b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return input::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return input::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return input::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return input::Right;
    case SDL_CONTROLLER_BUTTON_A: return input::Cross;
    case SDL_CONTROLLER_BUTTON_B: return input::Circle;
    case SDL_CONTROLLER_BUTTON_Y: return input::Triangle;
    case SDL_CONTROLLER_BUTTON_X: return input::Square;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return input::L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return input::R1;
    case SDL_CONTROLLER_BUTTON_START: return input::Options;
    default: return 0;
    }
}

void open(int index)
{
    if (!SDL_IsGameController(index))
        return;
    SDL_GameController *c = SDL_GameControllerOpen(index);
    if (!c)
        return;
    const SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
    if (s_controllers.count(id)) {
        SDL_GameControllerClose(c);
        return;
    }
    s_controllers[id] = c;
    std::fprintf(stderr, "input: controller \"%s\"\n", SDL_GameControllerName(c));
}

/* L2 and R2 are axes in SDL: pressed past half way, released under a third. */
void trigger(const SDL_ControllerAxisEvent &e)
{
    const uint32_t bit = e.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? input::L2 : input::R2;
    const float v = e.value / 32767.f;
    const bool was = s_pad & bit;
    if (!was && v > 0.5f) {
        s_pad |= bit;
        s_went_down |= bit;
    } else if (was && v < 0.33f) {
        s_pad &= ~bit;
    }
}

/* The left stick as a D-pad: past half way, with a little hysteresis. */
void stick(const SDL_ControllerAxisEvent &e)
{
    const float v = e.value / 32767.f;
    uint32_t neg = 0, pos = 0;
    if (e.axis == SDL_CONTROLLER_AXIS_LEFTX)
        neg = input::Left, pos = input::Right;
    else if (e.axis == SDL_CONTROLLER_AXIS_LEFTY)
        neg = input::Up, pos = input::Down;
    else
        return;
    auto set = [](uint32_t bit, bool on, float mag) {
        const bool was = s_stick & bit;
        if (!was && on && mag > 0.55f) {
            s_stick |= bit;
            s_went_down |= bit;
        } else if (was && (!on || mag < 0.35f)) {
            s_stick &= ~bit;
        }
    };
    set(neg, v < 0, -v);
    set(pos, v > 0, v);
}

} // namespace

void init()
{
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        open(i);
}

void shutdown()
{
    for (auto &c : s_controllers)
        SDL_GameControllerClose(c.second);
    s_controllers.clear();
}

void handle_event(const SDL_Event &ev)
{
    switch (ev.type) {
    case SDL_KEYDOWN:
        if (!ev.key.repeat) {   /* our own repeat (core/input) */
            const uint32_t b = key_button(ev.key.keysym.sym);
            s_keys |= b;
            s_went_down |= b;
        }
        break;
    case SDL_KEYUP:
        s_keys &= ~key_button(ev.key.keysym.sym);
        break;
    case SDL_CONTROLLERBUTTONDOWN: {
        const uint32_t b = pad_button(ev.cbutton.button);
        s_pad |= b;
        s_went_down |= b;
        break;
    }
    case SDL_CONTROLLERBUTTONUP:
        s_pad &= ~pad_button(ev.cbutton.button);
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            trigger(ev.caxis);
        else
            stick(ev.caxis);
        break;
    case SDL_CONTROLLERDEVICEADDED:
        open(ev.cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED: {
        auto it = s_controllers.find(ev.cdevice.which);
        if (it != s_controllers.end()) {
            SDL_GameControllerClose(it->second);
            s_controllers.erase(it);
            s_pad = s_stick = 0;
        }
        break;
    }
    case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            s_keys = 0;   /* a key released in another window never comes back up here */
        break;
    default:
        break;
    }
}

uint32_t take_went_down()
{
    const uint32_t b = s_went_down;
    s_went_down = 0;
    return b;
}

uint32_t held() { return s_keys | s_pad | s_stick; }

} // namespace input_sdl
