/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Typing a text on the PS5: the system keyboard (sceImeDialog), with its
 * predictive text and the PlayStation app's phone keyboard (platform.h).
 * Ported from Jelly5's ime.cpp (02dnot, GPL-3.0-or-later), itself based on
 * ProsperoTV's iptv_ime.c (BlackBearReloaded, GPL-3.0-or-later): the
 * parameter layout, the UTF-8 / UTF-16 conversion and the status protocol.
 */
#include "platform/platform.h"
#include "platform/ps5/system_ps5.h"

#include <cstdint>
#include <cstring>
#include <ctime>

namespace {

struct ImeParam {
    int32_t user_id;
    int32_t type;
    uint64_t supported_languages;
    int32_t enter_label;
    int32_t input_method;
    void *filter;
    uint32_t option;
    uint32_t max_text_length;
    uint16_t *input_text_buffer;
    float pos_x, pos_y;
    int32_t horizontal_alignment, vertical_alignment;
    const uint16_t *placeholder;
    const uint16_t *title;
    int8_t reserved[16];
};
struct ImeResult {
    int32_t outcome;
    int8_t reserved[12];
};
static_assert(sizeof(ImeParam) == 96, "unexpected PS5 IME parameter layout");
static_assert(sizeof(ImeResult) == 16, "unexpected PS5 IME result layout");

constexpr uint16_t kSysmoduleImeDialog = 0x0096;
constexpr uint32_t kCommonDialogAlreadyInitialised = 0x80B80002u;
constexpr int32_t kTypeDefault = 0;
constexpr int32_t kEnterDefault = 0, kEnterSearch = 2;
constexpr unsigned kMaxChars = 256;

extern "C" {
int sceCommonDialogInitialize(void);
int sceImeDialogAbort(void);
int sceImeDialogGetResult(ImeResult *result);
int sceImeDialogGetStatus(void);
int sceImeDialogInit(const ImeParam *param, const void *extended);
int sceImeDialogTerm(void);
int sceSysmoduleLoadModule(uint16_t module_id);
int sceUserServiceGetForegroundUser(int32_t *user_id);
}

bool s_loaded = false, s_active = false, s_over = false, s_confirmed = false;
bool s_abandoned = false;   /* cancelled: its end is nobody's */
double s_started = 0;
uint16_t s_buffer[kMaxChars + 1];
uint16_t s_title[48];
std::string s_initial, s_text;

double now_s()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void to_utf16(const std::string &in, uint16_t *out, size_t cap)
{
    size_t w = 0;
    const unsigned char *t = (const unsigned char *)in.c_str();
    while (*t && w + 1 < cap) {
        uint32_t cp = *t;
        unsigned n = 1;
        if ((t[0] & 0xe0) == 0xc0)
            cp = t[0] & 0x1f, n = 2;
        else if ((t[0] & 0xf0) == 0xe0)
            cp = t[0] & 0x0f, n = 3;
        else if ((t[0] & 0xf8) == 0xf0)
            cp = t[0] & 0x07, n = 4;
        bool ok = !(n == 1 && (t[0] & 0x80));
        for (unsigned i = 1; i < n && ok; i++) {
            if ((t[i] & 0xc0) != 0x80)
                ok = false;
            else
                cp = (cp << 6) | (t[i] & 0x3f);
        }
        if (!ok)
            cp = 0xfffd, n = 1;
        t += n;
        if (cp >= 0x10000) {
            if (w + 2 >= cap)
                break;
            cp -= 0x10000;
            out[w++] = (uint16_t)(0xd800 | (cp >> 10));
            out[w++] = (uint16_t)(0xdc00 | (cp & 0x3ff));
        } else {
            out[w++] = (uint16_t)cp;
        }
    }
    out[w] = 0;
}

std::string to_utf8(const uint16_t *in)
{
    std::string out;
    for (size_t i = 0; in[i]; i++) {
        uint32_t cp = in[i];
        if (cp >= 0xd800 && cp <= 0xdbff && in[i + 1] >= 0xdc00 && in[i + 1] <= 0xdfff)
            cp = 0x10000 + ((cp - 0xd800) << 10) + (in[++i] - 0xdc00);
        else if (cp >= 0xd800 && cp <= 0xdfff)
            cp = 0xfffd;
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xc0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3f));
        } else if (cp < 0x10000) {
            out += (char)(0xe0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3f));
            out += (char)(0x80 | (cp & 0x3f));
        } else {
            out += (char)(0xf0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3f));
            out += (char)(0x80 | ((cp >> 6) & 0x3f));
            out += (char)(0x80 | (cp & 0x3f));
        }
    }
    return out;
}

bool init()
{
    if (s_loaded)
        return true;
    const int cd = sceCommonDialogInitialize();
    if (cd < 0 && (uint32_t)cd != kCommonDialogAlreadyInitialised) {
        ps5::log("[regie5] keyboard: common dialog init 0x%08x", (unsigned)cd);
        return false;
    }
    const int rc = sceSysmoduleLoadModule(kSysmoduleImeDialog);
    if (rc < 0) {
        ps5::log("[regie5] keyboard: load module 0x%08x", (unsigned)rc);
        return false;
    }
    s_loaded = true;
    return true;
}

/* Called each frame through text_entry_active(): the dialog's state. */
void poll()
{
    if (!s_active)
        return;
    const int status = sceImeDialogGetStatus();
    if (status == 1 || (status == 0 && now_s() - s_started < 1.0))
        return;   /* open, or not up yet */
    s_confirmed = false;
    if (status == 2) {
        ImeResult r;
        std::memset(&r, 0, sizeof r);
        if (sceImeDialogGetResult(&r) >= 0 && r.outcome == 0) {
            s_text = to_utf8(s_buffer);
            s_confirmed = true;
        }
        sceImeDialogTerm();
    }
    std::memset(s_buffer, 0, sizeof s_buffer);
    s_active = false;
    s_over = !s_abandoned;
    s_abandoned = false;
    ps5::log("[regie5] keyboard: %s", s_confirmed ? "text given" : "closed");
}

} // namespace

namespace platform {

bool text_entry_start(TextKind kind, const std::string &title, const std::string &initial)
{
    if (s_active || !init())
        return false;
    int32_t user = -1;
    if (sceUserServiceGetForegroundUser(&user) < 0)
        return false;
    s_initial = s_text = initial;
    s_over = false;
    to_utf16(initial, s_buffer, kMaxChars + 1);
    to_utf16(title, s_title, sizeof s_title / sizeof s_title[0]);
    static const uint16_t empty[1] = {0};
    ImeParam p;
    std::memset(&p, 0, sizeof p);
    p.user_id = user;
    p.type = kTypeDefault;
    p.enter_label = kind == TextKind::Search ? kEnterSearch : kEnterDefault;
    p.max_text_length = kMaxChars;
    p.input_text_buffer = s_buffer;
    p.horizontal_alignment = 1;
    p.vertical_alignment = 1;
    p.placeholder = empty;
    p.title = s_title;
    const int rc = sceImeDialogInit(&p, nullptr);
    s_active = rc == 0;
    if (!s_active)
        ps5::log("[regie5] keyboard: dialog init 0x%08x", (unsigned)rc);
    s_started = now_s();
    return s_active;
}

bool text_entry_active()
{
    poll();
    return s_active;
}

std::string text_entry_text() { return s_active ? s_initial : s_text; }

bool text_entry_finished(bool *confirmed, std::string *text)
{
    poll();
    if (!s_over)
        return false;
    s_over = false;
    *confirmed = s_confirmed;
    *text = s_confirmed ? s_text : s_initial;
    return true;
}

void text_entry_cancel()
{
    if (s_active) {
        s_abandoned = true;
        sceImeDialogAbort();
    }
    s_over = false;
}

} // namespace platform
