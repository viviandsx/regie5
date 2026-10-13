/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ui/gfx.h on OpenGL 3.3 core (and GLES 3.0: nothing outside their common
 * subset). Quads are batched; each carries its shape (rect, corner radius) so
 * the fragment shader cuts anti-aliased rounded corners, a lit rim or a soft
 * shadow from the same signed distance. The frame is drawn into a texture
 * (the scene) so the glass can read back what lies under it: backdrop_blur
 * copies that region at half logical resolution, blurs it in two passes and draws the
 * liquid glass shader over the pane, as Jelly5 does on the PS5's GPU.
 */
#include "platform/gl/gfx_gl.h"

#include "core/perf.h"
#include "platform/gl/gl_shaders.h"
#include "ui/gfx.h"

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace gfx {

struct Texture {
    GLuint id = 0;
    int w = 0, h = 0;
};

} // namespace gfx

namespace {

using gfx::Rect;
using gfx::Texture;

constexpr uint32_t kClear = 0xff07070a;   /* the app's background (ui/style.h kBg) */

struct Vertex {
    float x, y, u, v;
    uint8_t c[4];   /* premultiplied RGBA */
    float rect[4];
    float param[4];   /* radius, mode, p1, p2 */
};
enum Mode { kFill = 0, kRim = 1, kShadow = 2 };

constexpr int kMaxQuads = 8192;

struct Program {
    GLuint id = 0;
    GLint u[16] = {};
};

bool s_es = false;
void (*s_present)() = nullptr;

int s_dw = 0, s_dh = 0;            /* drawable */
int s_pw = 0, s_ph = 0;            /* the canvas in pixels */
int s_ox = 0, s_oy = 0;            /* where it sits in the drawable (GL, bottom-left) */
int s_ow = 0, s_oh = 0;            /* its size there (the canvas's, unless forced) */
int s_force_w = 0, s_force_h = 0;  /* set_render_size */
float s_scale = 1.f;

GLuint s_scene_tex = 0, s_scene_fbo = 0;
GLuint s_tmp_tex[2] = {}, s_tmp_fbo[2] = {};
int s_tmp_w = 0, s_tmp_h = 0;

GLuint s_vao = 0, s_vbo = 0, s_ibo = 0, s_empty_vao = 0;
Texture s_white;

Program s_ui, s_blur, s_glass, s_copy;
bool s_bench = false;
enum { U_CANVAS, U_TEX, U_SCALE, U_LIGHT };
enum { B_QUAD, B_UVRECT, B_SRC, B_STEP, B_SIGMA, B_CLAMP };
enum { C_QUAD, C_UVRECT, C_SRC };
enum { G_QUAD, G_UVRECT, G_RECT, G_SHAPE, G_LIGHT, G_TONE, G_TINT, G_MORE, G_MAP, G_CLAMP, G_TARGETH, G_SOURCE };

std::vector<Vertex> s_verts;
const Texture *s_batch_tex = nullptr;
const Texture *s_solid = nullptr;   /* set_solid_source */
float s_solid_u = 0, s_solid_v = 0;

std::vector<float> s_opacity_stack;
float s_opacity = 1.f;
std::vector<Rect> s_scissors;

float s_light_x = -0.55f, s_light_y = -0.83f;
uint64_t s_frame = 0;

/* ---- GPU timings (REGIE5_FPS, desktop GL only) ---------------------------- */

/* Timestamps: the frame's start, a pair around each blur, the frame's end.
 * Read back a few frames later, when the GPU has long finished them. */
constexpr int kGpuSlots = 4, kGpuStamps = 2 + 2 * 24;
struct GpuSlot {
    GLuint q[kGpuStamps] = {};
    int n = 0;
    uint64_t frame = 0;
    bool pending = false;
};
GpuSlot s_gpu[kGpuSlots];
GpuSlot *s_gpu_cur = nullptr;   /* the frame being drawn's, while it records */

void gpu_stamp()
{
    if (s_gpu_cur && s_gpu_cur->n < kGpuStamps)
        glQueryCounter(s_gpu_cur->q[s_gpu_cur->n++], GL_TIMESTAMP);
}

void gpu_collect(GpuSlot &g)
{
    if (!g.pending)
        return;
    g.pending = false;
    GLint ready = 0;
    glGetQueryObjectiv(g.q[g.n - 1], GL_QUERY_RESULT_AVAILABLE, &ready);
    perf::Frame *f = perf::at(g.frame);
    if (!ready || !f || g.n < 2)
        return;   /* not done yet: dropped rather than waited for */
    GLuint64 t[kGpuStamps];
    for (int i = 0; i < g.n; i++)
        glGetQueryObjectui64v(g.q[i], GL_QUERY_RESULT, &t[i]);
    f->gpu_ms = (double)(t[g.n - 1] - t[0]) / 1e6;
    double blur = 0;
    for (int i = 1; i + 1 <= g.n - 2; i += 2)   /* the pairs, between start and end */
        blur += (double)(t[i + 1] - t[i]) / 1e6;
    f->blur_gpu_ms = blur;
}

void gpu_begin_frame()
{
    s_gpu_cur = nullptr;
    if (s_es || !perf::enabled())
        return;
    GpuSlot &g = s_gpu[s_frame % kGpuSlots];
    if (!g.q[0])
        glGenQueries(kGpuStamps, g.q);
    gpu_collect(g);
    g.n = 0;
    g.frame = s_frame;
    s_gpu_cur = &g;
    gpu_stamp();
}

void gpu_end_frame()
{
    if (!s_gpu_cur)
        return;
    if (s_gpu_cur->n >= kGpuStamps)   /* keep the last one for the end */
        s_gpu_cur->n = kGpuStamps - 1;
    gpu_stamp();
    s_gpu_cur->pending = true;
    s_gpu_cur = nullptr;
}

/* ---- setup ------------------------------------------------------------- */

GLuint compile(GLenum type, const char *body)
{
    const std::string src = std::string(s_es ? "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                                             : "#version 330 core\n") +
                            body;
    const char *p = src.c_str();
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &p, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "gfx: shader did not compile:\n%s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

bool link(Program *p, const char *vs, const char *fs, std::initializer_list<const char *> uniforms)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f)
        return false;
    p->id = glCreateProgram();
    glAttachShader(p->id, v);
    glAttachShader(p->id, f);
    glLinkProgram(p->id);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p->id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p->id, sizeof log, nullptr, log);
        std::fprintf(stderr, "gfx: program did not link:\n%s\n", log);
        return false;
    }
    int i = 0;
    for (const char *name : uniforms)
        p->u[i++] = glGetUniformLocation(p->id, name);
    return true;
}

GLuint make_texture(int w, int h, const void *pixels)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void make_target(GLuint *tex, GLuint *fbo, int w, int h)
{
    if (*fbo)
        glDeleteFramebuffers(1, fbo);
    if (*tex)
        glDeleteTextures(1, tex);
    *tex = make_texture(w, h, nullptr);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "gfx: a render target is incomplete (%dx%d)\n", w, h);
}

/* ---- drawing ----------------------------------------------------------- */

void premul(uint32_t argb, float opacity, uint8_t out[4])
{
    const float a = (float)(argb >> 24) / 255.f * opacity * s_opacity;
    out[0] = (uint8_t)std::lround(((argb >> 16) & 0xff) * a);
    out[1] = (uint8_t)std::lround(((argb >> 8) & 0xff) * a);
    out[2] = (uint8_t)std::lround((argb & 0xff) * a);
    out[3] = (uint8_t)std::lround(255.f * a);
}

void apply_scissor()
{
    if (s_scissors.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect &r = s_scissors.back();
    const int x0 = (int)std::floor(r.x * s_scale), y0 = (int)std::floor(r.y * s_scale);
    const int x1 = (int)std::ceil((r.x + r.w) * s_scale), y1 = (int)std::ceil((r.y + r.h) * s_scale);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, s_ph - y1, std::max(0, x1 - x0), std::max(0, y1 - y0));
}

void flush()
{
    if (s_verts.empty())
        return;
    glUseProgram(s_ui.id);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(s_verts.size() * sizeof(Vertex)), s_verts.data(), GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (s_batch_tex ? s_batch_tex : &s_white)->id);
    glUniform1f(s_ui.u[U_SCALE], s_scale);
    glUniform2f(s_ui.u[U_LIGHT], s_light_x, s_light_y);
    glDrawElements(GL_TRIANGLES, (GLsizei)(s_verts.size() / 4 * 6), GL_UNSIGNED_SHORT, nullptr);
    perf::cur().draws++;
    s_verts.clear();
}

/* One quad: geometry q (logical), uv, corner colours (tl tr bl br), the shape it is cut to. */
void quad(const Rect &q, const Texture *t, float u0, float v0, float u1, float v1, const uint32_t c[4],
          float opacity, const Rect &shape, float radius, Mode mode, float p1 = 0)
{
    if (q.w <= 0 || q.h <= 0 || opacity * s_opacity <= 0.f)
        return;
    if (!t && s_solid) {   /* a solid: the white texel of the atlas, to share its batch */
        t = s_solid;
        u0 = u1 = s_solid_u;
        v0 = v1 = s_solid_v;
    }
    if (t != s_batch_tex || s_verts.size() / 4 >= kMaxQuads) {
        flush();
        s_batch_tex = t;
    }
    const float xs[2] = {q.x, q.x + q.w}, ys[2] = {q.y, q.y + q.h};
    const float us[2] = {u0, u1}, vs[2] = {v0, v1};
    for (int k = 0; k < 4; k++) {
        Vertex v;
        v.x = xs[k & 1], v.y = ys[k >> 1], v.u = us[k & 1], v.v = vs[k >> 1];
        premul(c[k], opacity, v.c);
        v.rect[0] = shape.x, v.rect[1] = shape.y, v.rect[2] = shape.w, v.rect[3] = shape.h;
        v.param[0] = radius, v.param[1] = (float)mode, v.param[2] = p1, v.param[3] = 0;
        s_verts.push_back(v);
    }
}

void quad_solid(const Rect &r, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, float radius)
{
    const uint32_t c[4] = {tl, tr, bl, br};
    quad(r, nullptr, 0, 0, 1, 1, c, 1.f, r, radius, kFill);
}

} // namespace

/* A texture's region (uv) drawn over the whole viewport of the bound target,
 * as it is: blending and clipping off. */
void copy_quad(GLuint tex, float u0, float v0, float u1, float v1)
{
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glUseProgram(s_copy.id);
    glBindVertexArray(s_empty_vao);
    glUniform4f(s_copy.u[C_QUAD], -1, -1, 1, 1);
    glUniform4f(s_copy.u[C_UVRECT], u0, v0, u1, v1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* ---- the backend's own entry points --------------------------------------- */

namespace gfx_gl {

bool init(bool es)
{
    s_es = es;
    if (!link(&s_ui, gl_shaders::kUiVert, gl_shaders::kUiFrag, {"uCanvas", "uTex", "uScale", "uLight"}) ||
        !link(&s_blur, gl_shaders::kQuadVert, gl_shaders::kBlurFrag,
              {"uQuad", "uUVRect", "uSrc", "uStep", "uSigma", "uClampUV"}) ||
        !link(&s_glass, gl_shaders::kQuadVert, gl_shaders::kGlassFrag,
              {"uQuad", "uUVRect", "uRect", "uShape", "uLight", "uTone", "uTint", "uMore", "uMap", "uClampUV",
               "uTargetH", "uSource"}) ||
        !link(&s_copy, gl_shaders::kQuadVert, gl_shaders::kCopyFrag, {"uQuad", "uUVRect", "uSrc"}))
        return false;
    glUseProgram(s_ui.id);
    glUniform2f(s_ui.u[U_CANVAS], gfx::W, gfx::H);
    glUniform1i(s_ui.u[U_TEX], 0);
    glUseProgram(s_blur.id);
    glUniform1i(s_blur.u[B_SRC], 0);
    glUseProgram(s_glass.id);
    glUniform1i(s_glass.u[G_SOURCE], 0);
    glUseProgram(s_copy.id);
    glUniform1i(s_copy.u[C_SRC], 0);

    const uint8_t white[4] = {255, 255, 255, 255};
    s_white.id = make_texture(1, 1, white);
    s_white.w = s_white.h = 1;

    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    const GLsizei st = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void *)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void *)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void *)offsetof(Vertex, c));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void *)offsetof(Vertex, rect));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (void *)offsetof(Vertex, param));
    std::vector<uint16_t> idx;
    idx.reserve(kMaxQuads * 6);
    for (int i = 0; i < kMaxQuads; i++) {
        const uint16_t a = (uint16_t)(i * 4);
        for (uint16_t k : {a, (uint16_t)(a + 1), (uint16_t)(a + 2), (uint16_t)(a + 2), (uint16_t)(a + 1),
                           (uint16_t)(a + 3)})
            idx.push_back(k);
    }
    glGenBuffers(1, &s_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ibo);   /* part of the VAO */
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * 2), idx.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &s_empty_vao);
    glBindVertexArray(0);
    s_verts.reserve(4096);
    return true;
}

void shutdown()
{
    glDeleteFramebuffers(1, &s_scene_fbo);
    glDeleteTextures(1, &s_scene_tex);
    glDeleteFramebuffers(2, s_tmp_fbo);
    glDeleteTextures(2, s_tmp_tex);
    glDeleteTextures(1, &s_white.id);
    glDeleteBuffers(1, &s_vbo);
    glDeleteBuffers(1, &s_ibo);
    glDeleteVertexArrays(1, &s_vao);
    glDeleteVertexArrays(1, &s_empty_vao);
    glDeleteProgram(s_ui.id);
    glDeleteProgram(s_blur.id);
    glDeleteProgram(s_glass.id);
    glDeleteProgram(s_copy.id);
    for (GpuSlot &g : s_gpu)
        if (g.q[0])
            glDeleteQueries(kGpuStamps, g.q);
}

void resize(int dw, int dh)
{
    if (dw <= 0 || dh <= 0 || (dw == s_dw && dh == s_dh))
        return;
    s_dw = dw, s_dh = dh;
    const float fit = std::min(dw / gfx::W, dh / gfx::H);
    s_ow = std::max(1, (int)std::lround(gfx::W * fit));
    s_oh = std::max(1, (int)std::lround(gfx::H * fit));
    s_ox = (dw - s_ow) / 2, s_oy = (dh - s_oh) / 2;
    /* The canvas: the window's, or a forced size scaled into it when shown. */
    s_scale = s_force_w > 0 ? std::min(s_force_w / gfx::W, s_force_h / gfx::H) : fit;
    s_pw = std::max(1, (int)std::lround(gfx::W * s_scale));
    s_ph = std::max(1, (int)std::lround(gfx::H * s_scale));
    make_target(&s_scene_tex, &s_scene_fbo, s_pw, s_ph);
    s_tmp_w = s_pw / 2 + 2, s_tmp_h = s_ph / 2 + 2;
    for (int i = 0; i < 2; i++)
        make_target(&s_tmp_tex[i], &s_tmp_fbo[i], s_tmp_w, s_tmp_h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void set_render_size(int w, int h)
{
    s_force_w = std::max(0, w), s_force_h = std::max(0, h);
    const int dw = s_dw, dh = s_dh;
    s_dw = s_dh = 0;   /* redo the targets */
    if (dw > 0 && dh > 0)
        resize(dw, dh);
}

void set_present(void (*present)()) { s_present = present; }
void set_bench(bool on) { s_bench = on; }

bool read_canvas(std::vector<uint8_t> *rgb, int *w, int *h)
{
    if (!s_scene_fbo)
        return false;
    std::vector<uint8_t> rgba((size_t)s_pw * s_ph * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_scene_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, s_pw, s_ph, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    rgb->resize((size_t)s_pw * s_ph * 3);
    for (int y = 0; y < s_ph; y++)   /* GL's rows are bottom-up */
        for (int x = 0; x < s_pw; x++)
            for (int c = 0; c < 3; c++)
                (*rgb)[((size_t)y * s_pw + x) * 3 + c] = rgba[((size_t)(s_ph - 1 - y) * s_pw + x) * 4 + c];
    *w = s_pw, *h = s_ph;
    return true;
}

} // namespace gfx_gl

/* ---- ui/gfx.h -------------------------------------------------------------- */

namespace gfx {

float scale() { return s_scale; }
uint64_t frame_index() { return s_frame; }

void begin_frame()
{
    s_frame++;
    s_verts.clear();
    s_batch_tex = nullptr;
    s_opacity_stack.clear();
    s_opacity = 1.f;
    s_scissors.clear();
    glBindFramebuffer(GL_FRAMEBUFFER, s_scene_fbo);
    glViewport(0, 0, s_pw, s_ph);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   /* premultiplied */
    glClearColor(((kClear >> 16) & 0xff) / 255.f, ((kClear >> 8) & 0xff) / 255.f, (kClear & 0xff) / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    gpu_begin_frame();
}

void end_frame()
{
    perf::Frame &pf = perf::cur();
    perf::Timer submit(&pf.phase_ms[perf::Submit]);
    const double scene_t0 = s_bench ? perf::now_ms() : 0;
    flush();
    if (s_bench) {   /* the scene, blur included, finished on the GPU: no display wait in it */
        glFinish();
        pf.scene_ms = perf::now_ms() - scene_t0;
    }
    /* The scene to the screen by a drawn quad (a blit is done on the CPU by
     * ps5-gallium). Same size: texel centres fall on pixel centres. */
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (s_ow != s_dw || s_oh != s_dh) {   /* bars around a canvas that does not fill the screen */
        glViewport(0, 0, s_dw, s_dh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glViewport(s_ox, s_oy, s_ow, s_oh);
    copy_quad(s_scene_tex, 0, 0, 1, 1);
    glEnable(GL_BLEND);
    gpu_end_frame();
    if (s_present) {
        const double t0 = perf::enabled() ? perf::now_ms() : 0;
        s_present();
        if (perf::enabled()) {   /* the swap is not part of submit */
            const double ms = perf::now_ms() - t0;
            pf.phase_ms[perf::Present] += ms;
            pf.phase_ms[perf::Submit] -= ms;
        }
    }
}

Texture *texture_from_pixels(const uint8_t *rgba, int w, int h, int stride_px)
{
    if (w <= 0 || h <= 0)
        return nullptr;
    flush();
    Texture *t = new Texture;
    t->w = w, t->h = h;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_px);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    t->id = make_texture(w, h, rgba);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return t;
}

void texture_update(Texture *t, int x, int y, int w, int h, const uint8_t *rgba, int stride_px)
{
    if (!t || !rgba || w <= 0 || h <= 0)
        return;
    flush();
    glBindTexture(GL_TEXTURE_2D, t->id);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_px);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void set_solid_source(const Texture *t, float u, float v)
{
    if (t == s_solid && u == s_solid_u && v == s_solid_v)
        return;
    flush();
    s_solid = t;
    s_solid_u = u, s_solid_v = v;
}

void texture_release(Texture *t)
{
    if (!t)
        return;
    if (s_batch_tex == t || s_solid == t)
        flush();
    if (s_solid == t)
        s_solid = nullptr;
    glDeleteTextures(1, &t->id);
    delete t;
}

int texture_width(const Texture *t) { return t ? t->w : 0; }
int texture_height(const Texture *t) { return t ? t->h : 0; }

void push_opacity(float a)
{
    s_opacity_stack.push_back(s_opacity);
    s_opacity *= a;
}

void pop_opacity()
{
    if (s_opacity_stack.empty())
        return;
    s_opacity = s_opacity_stack.back();
    s_opacity_stack.pop_back();
}

void push_scissor(const Rect &r)
{
    flush();
    Rect c = r;
    if (!s_scissors.empty()) {
        const Rect &o = s_scissors.back();
        const float x0 = std::max(c.x, o.x), y0 = std::max(c.y, o.y);
        const float x1 = std::min(c.x + c.w, o.x + o.w), y1 = std::min(c.y + c.h, o.y + o.h);
        c = {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
    }
    s_scissors.push_back(c);
    apply_scissor();
}

void pop_scissor()
{
    flush();
    if (!s_scissors.empty())
        s_scissors.pop_back();
    apply_scissor();
}

void fill(const Rect &r, uint32_t color, float radius) { quad_solid(r, color, color, color, color, radius); }

void fill_vgradient(const Rect &r, uint32_t top, uint32_t bottom, float radius)
{
    quad_solid(r, top, top, bottom, bottom, radius);
}

void fill_hgradient(const Rect &r, uint32_t left, uint32_t right, float radius)
{
    quad_solid(r, left, right, left, right, radius);
}

void image_uv(const Rect &r, const Texture *t, float u0, float v0, float u1, float v1, float opacity, float radius)
{
    if (!t)
        return;
    const uint32_t c[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    quad(r, t, u0, v0, u1, v1, c, opacity, r, radius, kFill);
}

void image(const Rect &r, const Texture *t, float opacity, float radius, bool cover)
{
    if (!t || r.w <= 0 || r.h <= 0)
        return;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (cover) {   /* crop the texture to r's shape, centred */
        const float ta = (float)t->w / t->h, ra = r.w / r.h;
        if (ta > ra) {
            const float k = ra / ta;
            u0 = (1 - k) / 2, u1 = 1 - u0;
        } else {
            const float k = ta / ra;
            v0 = (1 - k) / 2, v1 = 1 - v0;
        }
    }
    image_uv(r, t, u0, v0, u1, v1, opacity, radius);
}

void image_tint(const Rect &r, const Texture *t, uint32_t color) { image_tint_uv(r, t, 0, 0, 1, 1, color); }

void image_tint_uv(const Rect &r, const Texture *t, float u0, float v0, float u1, float v1, uint32_t color)
{
    if (!t)
        return;
    const uint32_t c[4] = {color, color, color, color};
    quad(r, t, u0, v0, u1, v1, c, 1.f, r, 0, kFill);
}

void rim(const Rect &r, float radius, float opacity)
{
    const uint32_t c[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    quad(r, nullptr, 0, 0, 1, 1, c, 0.55f * opacity, r, radius, kRim, 1.5f);
}

void shadow(const Rect &r, float radius, float blur, float opacity, float dy)
{
    const float e = blur * 1.2f;
    const Rect shape{r.x, r.y + dy, r.w, r.h};
    const Rect q{r.x - e, r.y + dy - e, r.w + 2 * e, r.h + 2 * e};
    const uint32_t c[4] = {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u};
    quad(q, nullptr, 0, 0, 1, 1, c, opacity, shape, radius, kShadow, blur * 0.5f);
}

bool set_glass_light(float x, float y)
{
    const float l = std::sqrt(x * x + y * y);
    if (l < 1e-3f)
        return false;
    x /= l, y /= l;
    const bool moved = std::fabs(x - s_light_x) + std::fabs(y - s_light_y) > 0.02f;
    if (moved)
        s_light_x = x, s_light_y = y;
    return moved;
}

int backdrop_blur(const Rect &r, float radius, float sigma, float opacity, float lift)
{
    const float shown = opacity * s_opacity;
    if (shown <= 0.f || !s_scene_fbo)
        return 0;
    const float s = s_scale;
    /* The pane in pixels (top-left origin), grown for the refraction, which
     * samples from just outside the rim. */
    const int x0 = std::max(0, (int)std::floor(r.x * s)), y0 = std::max(0, (int)std::floor(r.y * s));
    const int x1 = std::min(s_pw, (int)std::ceil((r.x + r.w) * s)), y1 = std::min(s_ph, (int)std::ceil((r.y + r.h) * s));
    if (x1 <= x0 || y1 <= y0)
        return 0;
    if (!s_scissors.empty()) {   /* clipped away: nothing to blur */
        const Rect &c = s_scissors.back();
        if (c.w <= 0 || c.h <= 0 || r.x >= c.x + c.w || r.x + r.w <= c.x || r.y >= c.y + c.h || r.y + r.h <= c.y)
            return 0;
    }
    flush();
    perf::cur().blurs++;
    const bool stamped = s_gpu_cur && s_gpu_cur->n + 2 < kGpuStamps;   /* room for the pair and the end */
    if (stamped)
        gpu_stamp();
    const int m = (int)std::ceil(30.f * s);
    const int bx0 = std::max(0, x0 - m), by0 = std::max(0, y0 - m);
    const int bx1 = std::min(s_pw, x1 + m), by1 = std::min(s_ph, y1 + m);
    const int bw = bx1 - bx0, bh = by1 - by0;
    /* Blurred on a grid of half logical pixels, whatever the output: in 4K the
     * region is taken at a quarter of its size, so the cost does not grow
     * with the resolution (it went up eightfold: four times the pixels, a
     * kernel twice as wide). It is softened anyway; the 1080p look is kept. */
    const float down = std::max(2.f, 2.f * s);
    const int tw = std::max(1, (int)std::ceil(bw / down)), th = std::max(1, (int)std::ceil(bh / down));

    /* 1. The region, shrunk, into tmp 0 (rows bottom-up, as GL has them), by
     * a drawn quad (no blit: see kCopyFrag). */
    glBindFramebuffer(GL_FRAMEBUFFER, s_tmp_fbo[0]);
    glViewport(0, 0, tw, th);
    copy_quad(s_scene_tex, (float)bx0 / s_pw, (float)(s_ph - by1) / s_ph, (float)bx1 / s_pw,
              (float)(s_ph - by0) / s_ph);

    /* 2. Two passes of the Gaussian: 0 -> 1 across, 1 -> 0 down. The real glass
     * shows the picture only softened, so the bending at the rim can be seen. */
    const float sigma_tex = sigma * 0.3f * s / down;
    const float iw = 1.f / s_tmp_w, ih = 1.f / s_tmp_h;
    glDisable(GL_BLEND);
    glUseProgram(s_blur.id);
    glBindVertexArray(s_empty_vao);
    glViewport(0, 0, tw, th);
    glUniform4f(s_blur.u[B_QUAD], -1, -1, 1, 1);
    glUniform4f(s_blur.u[B_UVRECT], 0, 0, tw * iw, th * ih);
    glUniform1f(s_blur.u[B_SIGMA], sigma_tex);
    glUniform4f(s_blur.u[B_CLAMP], 0.5f * iw, 0.5f * ih, (tw - 0.5f) * iw, (th - 0.5f) * ih);
    glActiveTexture(GL_TEXTURE0);
    for (int pass = 0; pass < 2; pass++) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_tmp_fbo[pass == 0 ? 1 : 0]);
        glBindTexture(GL_TEXTURE_2D, s_tmp_tex[pass == 0 ? 0 : 1]);
        glUniform2f(s_blur.u[B_STEP], pass == 0 ? iw : 0.f, pass == 0 ? 0.f : ih);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /* 3. The glass over the pane, in the scene, within the current clip. */
    glBindFramebuffer(GL_FRAMEBUFFER, s_scene_fbo);
    glViewport(0, 0, s_pw, s_ph);
    glEnable(GL_BLEND);
    apply_scissor();
    glUseProgram(s_glass.id);
    glBindTexture(GL_TEXTURE_2D, s_tmp_tex[0]);
    const float nx0 = r.x / gfx::W * 2 - 1, nx1 = (r.x + r.w) / gfx::W * 2 - 1;
    const float ny0 = 1 - r.y / gfx::H * 2, ny1 = 1 - (r.y + r.h) / gfx::H * 2;
    glUniform4f(s_glass.u[G_QUAD], nx0, ny0, nx1, ny1);
    glUniform4f(s_glass.u[G_UVRECT], 0, 0, 1, 1);
    /* As Jelly5 tunes it: the lens is capped to what a 32 px half-height pane
     * bends (a large sheet pulled big text in from far inside). */
    const float half = std::max(1.f, std::min(r.w, r.h) / 2);
    const float bend = std::min(1.f, 32.f / half);
    glUniform4f(s_glass.u[G_RECT], r.x * s, r.y * s, (r.x + r.w) * s, (r.y + r.h) * s);
    glUniform4f(s_glass.u[G_SHAPE], radius * s, std::min(30.f, std::min(r.w, r.h) * 0.3f) * s, 0, 0);
    glUniform4f(s_glass.u[G_LIGHT], s_light_x, s_light_y, 1.f, (1.f - 0.55f * lift) * bend);
    glUniform4f(s_glass.u[G_TONE], 1.3f, 1.05f, 0.07f, shown);
    glUniform4f(s_glass.u[G_TINT], 0.92f, 0.94f, 1.f, 0.05f);
    glUniform4f(s_glass.u[G_MORE], lift, 0.28f * (1.f - lift), 0.f, 0.10f * (1.f - lift));
    glUniform4f(s_glass.u[G_MAP], (float)bx0, (float)by1, (float)tw / bw * iw, (float)th / bh * ih);
    glUniform4f(s_glass.u[G_CLAMP], 0.5f * iw, 0.5f * ih, (tw - 0.5f) * iw, (th - 0.5f) * ih);
    glUniform1f(s_glass.u[G_TARGETH], (float)s_ph);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    if (stamped)
        gpu_stamp();
    return 2;
}

} // namespace gfx
