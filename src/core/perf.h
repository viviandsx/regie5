/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Frame timings, a development aid (REGIE5_FPS=1). The loop times its phases;
 * the text cache, the picture cache and the renderer add what they cost.
 * Portable: a steady clock, nothing else. Off, every call returns at once.
 */
#pragma once

#include <cstdint>

namespace perf {

/* The loop's phases, on the CPU. */
enum Phase { Events, Pump, Input, Draw, Submit, Present, PhaseCount };
const char *phase_name(Phase p);

struct Frame {
    uint64_t index = 0;
    double phase_ms[PhaseCount] = {};
    int texts = 0;          /* texts laid out (new to the cache) */
    int glyphs = 0;         /* of which, glyphs rasterised into the atlas */
    double text_ms = 0;
    int uploads = 0;        /* pictures sent to the GPU */
    double upload_ms = 0;
    int blurs = 0;          /* glass panes blurred */
    int draws = 0;          /* draw calls (a batch ends at each change of texture) */
    /* On the GPU, known a few frames later (filled in then): < 0 until then. */
    double gpu_ms = -1, blur_gpu_ms = -1;
};

void enable(bool on);
bool enabled();

double now_ms();

/* The frame being drawn: the loop starts it, the others add to it. */
void begin(uint64_t index);
Frame &cur();
/* An earlier frame, while it is still kept (the GPU's answers come late), or null. */
Frame *at(uint64_t index);

/* Adds the time from its making to its end to *ms (when enabled). */
class Timer {
public:
    explicit Timer(double *ms) : m_ms(enabled() ? ms : nullptr), m_t0(m_ms ? now_ms() : 0) {}
    ~Timer()
    {
        if (m_ms)
            *m_ms += now_ms() - m_t0;
    }
    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

private:
    double *m_ms;
    double m_t0;
};

} // namespace perf
