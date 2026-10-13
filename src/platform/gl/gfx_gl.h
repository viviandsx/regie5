/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the desktop shell needs from the OpenGL backend besides ui/gfx.h.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace gfx_gl {

/* With a current OpenGL 3.3 core (or GLES 3.0) context. es: GLSL ES shaders. */
bool init(bool es);
void shutdown();

/* The drawable's size in pixels: the 1920x1080 canvas is fitted into it, 16:9,
 * centred (black bars around when the window is another shape). */
void resize(int drawable_w, int drawable_h);

/* Draws at w x h whatever the window's size, then scales it into the window
 * (REGIE5_RES: measuring the cost of 4K on a smaller screen). 0, 0: the
 * window's size. */
void set_render_size(int w, int h);

/* Called by gfx::end_frame once the frame is on the default framebuffer
 * (SDL_GL_SwapWindow on the desktop). */
void set_present(void (*present)());
/* The bench: the GPU is waited for once the scene is drawn, before the copy
 * to the screen, and perf::Frame::scene_ms says how long that took. It makes
 * frames slower; for measuring only. */
void set_bench(bool on);

/* The last frame drawn, as 8-bit RGB rows top to bottom (a development aid). */
bool read_canvas(std::vector<uint8_t> *rgb, int *w, int *h);

} // namespace gfx_gl
