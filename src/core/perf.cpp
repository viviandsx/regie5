/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/perf.h"

#include <chrono>

namespace perf {

namespace {

constexpr int kKept = 8;   /* frames kept for the GPU's late answers */
bool s_on = false;
Frame s_frames[kKept];
Frame s_off;   /* written to when off: nobody reads it */
uint64_t s_index = 0;

} // namespace

const char *phase_name(Phase p)
{
    static const char *const kNames[PhaseCount] = {"events", "pump", "input", "draw", "submit", "present"};
    return p >= 0 && p < PhaseCount ? kNames[p] : "";
}

void enable(bool on) { s_on = on; }
bool enabled() { return s_on; }

double now_ms()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

void begin(uint64_t index)
{
    s_index = index;
    s_frames[index % kKept] = Frame{};
    s_frames[index % kKept].index = index;
}

Frame &cur()
{
    if (!s_on)
        return s_off;
    return s_frames[s_index % kKept];
}

Frame *at(uint64_t index)
{
    Frame &f = s_frames[index % kKept];
    return s_on && f.index == index && index + kKept > s_index ? &f : nullptr;
}

} // namespace perf
