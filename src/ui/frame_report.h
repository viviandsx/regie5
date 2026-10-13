/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * REGIE5_FPS (and the PS5 probe): a line a second on stderr, and the detail
 * of each slow frame, printed a few frames late so the GPU's timings are in.
 */
#pragma once

#include "core/perf.h"
#include "platform/platform.h"
#include "ui/gfx.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace ui {

class FrameReport {
public:
    /* A frame is slow past one and a half periods of the display (25 ms at
     * 60 Hz, 12.5 ms at 120 Hz). */
    void set_hz(int hz) { m_slow_ms = 1500.0 / (hz > 0 ? hz : 60); }

    void drawn(uint64_t index, double gap_ms, double now)
    {
        const perf::Frame &f = perf::cur();
        m_frames++;
        m_gap_worst = std::max(m_gap_worst, gap_ms);
        double cpu = 0;
        for (double ms : f.phase_ms)
            cpu += ms;
        m_cpu_worst = std::max(m_cpu_worst, cpu - f.phase_ms[perf::Present]);
        m_texts += f.texts;
        m_glyphs += f.glyphs;
        m_text_ms_worst = std::max(m_text_ms_worst, f.text_ms);
        m_uploads += f.uploads;
        m_draws_worst = std::max(m_draws_worst, f.draws);
        m_upload_ms_worst = std::max(m_upload_ms_worst, f.upload_ms);
        if (gap_ms > m_slow_ms)
            m_slow.push_back({index, gap_ms});
        /* The GPU's answer for a frame comes some frames after it. */
        if (const perf::Frame *old = perf::at(index - 5))
            if (old->gpu_ms >= 0) {
                m_gpu_n++;
                m_gpu_sum += old->gpu_ms;
                m_gpu_worst = std::max(m_gpu_worst, old->gpu_ms);
                m_blur_worst = std::max(m_blur_worst, old->blur_gpu_ms);
                m_blurs_worst = std::max(m_blurs_worst, old->blurs);
            }
        for (size_t i = 0; i < m_slow.size();) {
            if (m_slow[i].index + 5 > index) {
                i++;
                continue;
            }
            print_slow(m_slow[i]);
            m_slow.erase(m_slow.begin() + (long)i);
        }
        if (now - m_since < 1.0)
            return;
        std::fprintf(stderr,
                     "fps: %d frames | worst gap %.1f ms | cpu worst %.1f ms | gpu avg %.1f, worst %.1f ms "
                     "(blur %.1f ms, %d panes) | draws worst %d | texts +%d, glyphs +%d (worst %.1f ms) | uploads %d "
                     "(worst %.1f ms) | %zu texts cached\n",
                     m_frames, m_gap_worst, m_cpu_worst, m_gpu_n ? m_gpu_sum / m_gpu_n : 0.0, m_gpu_worst,
                     m_blur_worst, m_blurs_worst, m_draws_worst, m_texts, m_glyphs, m_text_ms_worst, m_uploads,
                     m_upload_ms_worst, gfx::text_cache_size());
        std::vector<Slow> slow = std::move(m_slow);   /* still to be printed */
        const double slow_ms = m_slow_ms;
        *this = FrameReport{};
        m_slow = std::move(slow);
        m_slow_ms = slow_ms;
        m_since = now;
    }

private:
    struct Slow {
        uint64_t index;
        double gap_ms;
    };

    static void print_frame(const char *label, const perf::Frame *f)
    {
        if (!f) {
            std::fprintf(stderr, "  %s: (no longer kept)\n", label);
            return;
        }
        std::fprintf(stderr, "  %s #%llu:", label, (unsigned long long)f->index);
        for (int p = 0; p < perf::PhaseCount; p++)
            std::fprintf(stderr, " %s %.1f", perf::phase_name((perf::Phase)p), f->phase_ms[p]);
        std::fprintf(stderr, " ms | texts %d, glyphs %d (%.1f ms) | uploads %d (%.1f ms) | blurs %d | draws %d | gpu ",
                     f->texts, f->glyphs, f->text_ms, f->uploads, f->upload_ms, f->blurs, f->draws);
        if (f->gpu_ms >= 0)
            std::fprintf(stderr, "%.1f ms (blur %.1f ms)\n", f->gpu_ms, f->blur_gpu_ms);
        else
            std::fprintf(stderr, "?\n");
    }

    static void print_slow(const Slow &s)
    {
        /* The gap runs from the frame before's start to this one's: its draw
         * and swap, then this one's events, answers and input. */
        std::fprintf(stderr, "slow frame #%llu: gap %.1f ms\n", (unsigned long long)s.index, s.gap_ms);
        print_frame("before", perf::at(s.index - 1));
        print_frame("this  ", perf::at(s.index));
    }

    int m_frames = 0, m_texts = 0, m_glyphs = 0, m_uploads = 0, m_gpu_n = 0, m_blurs_worst = 0, m_draws_worst = 0;
    double m_gap_worst = 0, m_cpu_worst = 0, m_text_ms_worst = 0, m_upload_ms_worst = 0;
    double m_gpu_sum = 0, m_gpu_worst = 0, m_blur_worst = 0;
    double m_since = platform::now();
    double m_slow_ms = 25;
    std::vector<Slow> m_slow;
};

} // namespace ui
