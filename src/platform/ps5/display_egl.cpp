/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/ps5/display_egl.h"

#include "platform/ps5/system_ps5.h"

#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <ps5_opengl_display.h>
#if __has_include(<ps5_opengl_display_modes.h>)
#include <ps5_opengl_display_modes.h>
#endif

namespace ps5 {

namespace {

const EGLint kConfig[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                          EGL_RED_SIZE,     8,              EGL_GREEN_SIZE,      8,
                          EGL_BLUE_SIZE,    8,              EGL_ALPHA_SIZE,      8,
                          /* As validated on consoles by ps5-homebrew-ui, though nothing here uses them. */
                          EGL_DEPTH_SIZE,   24,             EGL_STENCIL_SIZE,    8,
                          EGL_NONE};

const EGLint kContext[] = {EGL_CONTEXT_MAJOR_VERSION_KHR,       4,
                           EGL_CONTEXT_MINOR_VERSION_KHR,       6,
                           EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                           EGL_NONE};

const char *gl_string(GLenum name)
{
    const char *v = reinterpret_cast<const char *>(glGetString(name));
    return v ? v : "(null)";
}

} // namespace

void Display::fail(const char *what)
{
    const EGLint e = eglGetError();
    log("[regie5] egl %s failed: 0x%04x", what, (unsigned)e);
}

bool Display::open(int width, int height)
{
    EGLint major = 0, minor = 0, count = 0;
    EGLConfig config = nullptr;
    log("[regie5] egl: eglGetDisplay (%dx%d asked)", width, height);
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) {
        fail("eglGetDisplay");
        return false;
    }
#ifdef PS5_OPENGL_DYNAMIC_DISPLAY
    /* The mode is fixed while EGL runs: chosen before eglInitialize. */
    log("[regie5] egl: eglSetDisplayModePS5 %dx%d, 60 Hz", width, height);
    if (!eglSetDisplayModePS5(m_display, width, height))
        fail("eglSetDisplayModePS5");
    eglSetDisplayRefreshPS5(m_display, 60);
#else
    (void)width;
    (void)height;
#endif
    log("[regie5] egl: eglInitialize");
    if (!eglInitialize(m_display, &major, &minor)) {
        fail("eglInitialize");
        close();
        return false;
    }
    log("[regie5] egl: eglBindAPI, eglChooseConfig (EGL %d.%d)", major, minor);
    if (!eglBindAPI(EGL_OPENGL_API) || !eglChooseConfig(m_display, kConfig, &config, 1, &count) || count != 1) {
        fail("eglBindAPI / eglChooseConfig");
        close();
        return false;
    }
    /* The PS5 backend owns the window: native handle 0. */
    log("[regie5] egl: eglCreateWindowSurface");
    m_surface = eglCreateWindowSurface(m_display, config, static_cast<EGLNativeWindowType>(0), nullptr);
    if (m_surface == EGL_NO_SURFACE) {
        fail("eglCreateWindowSurface");
        close();
        return false;
    }
    log("[regie5] egl: eglCreateContext (OpenGL 4.6 core), eglMakeCurrent");
    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, kContext);
    if (m_context == EGL_NO_CONTEXT || !eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
        fail("eglCreateContext / eglMakeCurrent");
        close();
        return false;
    }
    if (!eglQuerySurface(m_display, m_surface, EGL_WIDTH, &m_w) ||
        !eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &m_h) || m_w <= 0 || m_h <= 0) {
        fail("eglQuerySurface");
        close();
        return false;
    }
    if (!eglSwapInterval(m_display, 1))   /* vsync paces the loop */
        fail("eglSwapInterval");
    GLint gl_major = 0, gl_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
    log("[regie5] egl %d.%d surface %dx%d (asked %dx%d)", major, minor, m_w, m_h, width, height);
    log("[regie5] gl %d.%d version=\"%s\" renderer=\"%s\" glsl=\"%s\"", gl_major, gl_minor, gl_string(GL_VERSION),
        gl_string(GL_RENDERER), gl_string(GL_SHADING_LANGUAGE_VERSION));
    return true;
}

bool Display::swap()
{
    if (m_display == EGL_NO_DISPLAY || m_surface == EGL_NO_SURFACE)
        return false;
    if (!eglSwapBuffers(m_display, m_surface)) {
        fail("eglSwapBuffers");
        return false;
    }
    return true;
}

void Display::close()
{
    if (m_display == EGL_NO_DISPLAY)
        return;
    if (m_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_display, m_context);
    }
    if (m_surface != EGL_NO_SURFACE)
        eglDestroySurface(m_display, m_surface);
    eglTerminate(m_display);
    m_display = EGL_NO_DISPLAY;
    m_surface = EGL_NO_SURFACE;
    m_context = EGL_NO_CONTEXT;
    m_w = m_h = 0;
}

} // namespace ps5
