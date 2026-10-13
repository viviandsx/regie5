/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/ps5/pad_ps5.h"

#include "core/input.h"
#include "platform/ps5/system_ps5.h"

#include <cstddef>

namespace {

/* scePadRead's sample (120 bytes). */
struct RawPad {
    uint32_t buttons;
    uint8_t left_x, left_y, right_x, right_y;
    uint8_t l2, r2;
    uint8_t reserved0[66];
    int32_t connected;
    uint64_t timestamp_us;
    uint8_t extension[16];
    uint8_t connected_count;
    uint8_t reserved1[15];
};
static_assert(sizeof(RawPad) == 120, "scePadRead sample");
static_assert(offsetof(RawPad, connected) == 0x4c, "scePadRead sample");

constexpr int kAlreadyInitialized = (int)0x80960003;

/* The pad's own bits. */
constexpr uint32_t kOptions = 0x0008, kUp = 0x0010, kRight = 0x0020, kDown = 0x0040, kLeft = 0x0080, kL2 = 0x0100,
                   kR2 = 0x0200, kL1 = 0x0400, kR1 = 0x0800, kTriangle = 0x1000, kCircle = 0x2000, kCross = 0x4000,
                   kSquare = 0x8000;

uint32_t to_input(const RawPad &p)
{
    static const struct {
        uint32_t pad, bit;
    } kMap[] = {{kUp, input::Up},       {kDown, input::Down},         {kLeft, input::Left},
                {kRight, input::Right}, {kCross, input::Cross},       {kCircle, input::Circle},
                {kTriangle, input::Triangle}, {kSquare, input::Square}, {kL1, input::L1},
                {kR1, input::R1},       {kOptions, input::Options},   {kL2, input::L2},
                {kR2, input::R2}};
    uint32_t bits = 0;
    for (const auto &m : kMap)
        if (p.buttons & m.pad)
            bits |= m.bit;
    /* The left stick, past half its travel, is the D-pad (the larger axis). */
    const int dx = (int)p.left_x - 128, dy = (int)p.left_y - 128;
    if (dx * dx + dy * dy > 64 * 64) {
        if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy))
            bits |= dx < 0 ? input::Left : input::Right;
        else
            bits |= dy < 0 ? input::Up : input::Down;
    }
    return bits;
}

} // namespace

extern "C" {
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int *user);
int scePadInit(void);
int scePadOpen(int user, int type, int index, const void *parameters);
int scePadRead(int handle, RawPad *samples, int count);
int scePadClose(int handle);
}

namespace ps5 {

bool Pad::open()
{
    const int init = sceUserServiceInitialize(nullptr);
    if (init != 0 && init != kAlreadyInitialized)
        log("[regie5] pad sceUserServiceInitialize=0x%08x", (unsigned)init);
    if (sceUserServiceGetInitialUser(&m_user) != 0) {
        log("[regie5] pad: no initial user");
        return false;
    }
    scePadInit();
    /* The pad service can lag behind the title's start. */
    for (int attempt = 0; attempt < 20; attempt++) {
        m_handle = scePadOpen(m_user, 0, 0, nullptr);
        if (m_handle >= 0) {
            log("[regie5] pad open handle=%d attempts=%d", m_handle, attempt + 1);
            return true;
        }
        sleep_us(50000);
    }
    log("[regie5] pad scePadOpen failed=0x%08x", (unsigned)m_handle);
    m_handle = -1;
    return false;
}

void Pad::close()
{
    if (m_handle >= 0)
        scePadClose(m_handle);
    m_handle = -1;
}

void Pad::read(uint32_t *went_down, uint32_t *held)
{
    *went_down = 0;
    *held = m_held;
    if (m_handle < 0)
        return;
    RawPad samples[8];
    const int n = scePadRead(m_handle, samples, 8);
    for (int i = 0; i < n; i++) {   /* oldest first: a quick tap between two frames still counts */
        if (!samples[i].connected)
            continue;
        const uint32_t now = to_input(samples[i]);
        *went_down |= now & ~m_held;
        m_held = now;
    }
    *held = m_held;
}

} // namespace ps5
