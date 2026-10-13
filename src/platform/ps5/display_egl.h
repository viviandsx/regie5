/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The PS5's screen through ps5-opengl: EGL, an OpenGL 4.6 Core context (the
 * GL 3.3 core renderer runs on it unchanged), vsync. After ps5-homebrew-ui's
 * platform/ps5/display_egl.cpp (Copyright (C) 2026 BlackBearReloaded,
 * GPL-3.0-or-later), whose bring-up order presents reliably on ps5-opengl.
 */
#pragma once

#include <EGL/egl.h>

namespace ps5 {

class Display {
public:
    Display() = default;
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;
    ~Display() { close(); }

    /* 1920x1080, 2560x1440 or 3840x2160 (the SDK's runtime display modes),
     * at 60 or 120 Hz (a TV without 120 Hz keeps 60: see refresh()). */
    bool open(int width, int height, int hz = 60);
    bool swap();
    void close();
    int width() const { return m_w; }
    int height() const { return m_h; }
    /* The rate asked, and the one the output accepted: final once a frame
     * has been presented (until then, what was asked). */
    int asked_hz() const { return m_asked_hz; }
    int refresh();

private:
    void fail(const char *what);

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    int m_w = 0, m_h = 0;
    int m_asked_hz = 60, m_hz = 0;
    bool m_presented = false;   /* the accepted rate is known from the first frame on */
};

} // namespace ps5
