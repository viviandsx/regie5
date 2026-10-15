/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The desktop shell: an SDL2 window with an OpenGL 3.3 core context, the
 * input, and the loop. Like Jelly5's, the loop draws only when something
 * changed or moves, and waits otherwise; while it draws, vsync paces it.
 *
 * Development aids, from the environment:
 *   REGIE5_FPS=1      frame timings on stderr once a second (CPU phases, text
 *                    rasterised, pictures uploaded, GPU time and its blur
 *                    part), and a "slow frame" line for any gap over 25 ms
 *   REGIE5_RES=WxH    draw at that size (e.g. 3840x2160) and scale it into the
 *                    window: the cost of 4K on a smaller screen
 *   REGIE5_HTTP=socket  the PS5's HTTP transport (plain sockets) instead of
 *                    libcurl, against the real servers (http:// only)
 *   REGIE5_SCRIPT=... presses and captures played in order, for screenshots:
 *                    button names (up down left right cross circle triangle
 *                    square l1 r1 l2 r2 options), a number (seconds to wait) and
 *                    shot:<file.ppm> (the next frame, saved). quit ends the app.
 *                    e.g. REGIE5_SCRIPT="1 right right 0.8 shot:/tmp/a.ppm quit"
 */
#include "api/mock_transport.h"
#include "api/services.h"
#include "core/config.h"
#include "core/i18n.h"
#include "core/welcome.h"
#include "core/whats_new.h"
#include "core/input.h"
#include "core/options.h"
#include "core/perf.h"
#include "platform/linux/curl_transport.h"
#include "platform/gl/gfx_gl.h"
#include "platform/linux/input_sdl.h"
#include "platform/linux/text_linux.h"
#include "platform/posix/socket_transport.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/font.h"
#include "ui/gfx.h"
#include "ui/frame_report.h"
#include "ui/script.h"
#include "ui/shell.h"
#include "ui/style.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <sys/stat.h>

namespace {

SDL_Window *s_window = nullptr;

void present() { SDL_GL_SwapWindow(s_window); }

void fit_drawable()
{
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(s_window, &w, &h);
    gfx_gl::resize(w, h);
}

} // namespace

/* The monitor decides the rate on the desktop: Régie5 only reads it.
 * REGIE5_DISPLAY=3840x2160@60/120 shows what a console would answer (size,
 * rate had, rate asked), to see Settings' line without one; REGIE5_NO_120=1,
 * a console build whose param.json does not allow 120 Hz. */
platform::DisplayMode platform::display_mode()
{
    DisplayMode m;
    m.can_120 = std::getenv("REGIE5_NO_120") == nullptr;
    if (const char *fake = std::getenv("REGIE5_DISPLAY"))
        if (std::sscanf(fake, "%dx%d@%d/%d", &m.width, &m.height, &m.hz, &m.asked_hz) == 4)
            return m;
    if (!s_window)
        return m;
    SDL_GL_GetDrawableSize(s_window, &m.width, &m.height);
    SDL_DisplayMode dm;
    if (SDL_GetWindowDisplayMode(s_window, &dm) == 0 && dm.refresh_rate > 0)
        m.hz = dm.refresh_rate;
    return m;
}

int main(int argc, char **argv)
{
    options::Options opt;
    std::string err;
    if (!options::parse(argc, argv, &opt, &err)) {
        std::fprintf(stderr, "regie5: %s\n\n%s", err.c_str(), options::usage());
        return 2;
    }
    if (opt.help) {
        std::fputs(options::usage(), stdout);
        return 0;
    }
    if (opt.version) {
        std::printf("Régie5 %s\n", REGIE5_VERSION);
        return 0;
    }
    i18n::set_lang(i18n::find(!opt.lang.empty() ? opt.lang : platform::system_lang()));
    const std::string config_path = !opt.config_path.empty() ? opt.config_path : platform::default_config_path();

    /* The configuration, or in --mock the fixtures' own. Never logged: it holds the keys. */
    config::Config cfg;
    config::LoadError cfg_error = config::LoadError::None;
    if (opt.mock) {
        const char *off = std::getenv("REGIE5_MOCK_OFF");
        const char *multi = std::getenv("REGIE5_MOCK_MULTI");
        const char *emby = std::getenv("REGIE5_MOCK_EMBY");
        cfg = config::mock(off ? off : "", multi && *multi && *multi != '0', emby && *emby && *emby != '0');
        if (const char *qc = std::getenv("REGIE5_MOCK_QC"); qc && *qc && *qc != '0') {
            cfg.services[config::Jellyfin].api_key.clear();   /* an address, no key: Quick Connect's case */
            cfg.services[config::Jellyfin].enabled = false;
        }
    } else {
        cfg_error = config::load(config_path, &cfg);
        if (cfg_error != config::LoadError::None)
            std::fprintf(stderr, "regie5: %s: %s\n", config_path.c_str(),
                         cfg_error == config::LoadError::Missing ? "not found" : "cannot be read or is not JSON");
    }
    i18n::set_emby(cfg.emby());

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "regie5: SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (opt.fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    s_window = SDL_CreateWindow("Régie5", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, opt.width, opt.height, flags);
    if (!s_window) {
        std::fprintf(stderr, "regie5: window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(s_window);
    if (!gl) {
        std::fprintf(stderr, "regie5: OpenGL 3.3: %s\n", SDL_GetError());
        return 1;
    }
    if (SDL_GL_SetSwapInterval(1) != 0)
        std::fprintf(stderr, "regie5: no vsync (%s)\n", SDL_GetError());
    if (!gfx_gl::init(false)) {
        std::fprintf(stderr, "regie5: the renderer did not start\n");
        return 1;
    }
    gfx_gl::set_present(present);
    if (const char *res = std::getenv("REGIE5_RES")) {
        int rw = 0, rh = 0;
        if (std::sscanf(res, "%dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0)
            gfx_gl::set_render_size(rw, rh);
    }
    fit_drawable();
    if (!font::init()) {
        std::fprintf(stderr, "regie5: fonts missing (assets/fonts)\n");
        return 1;
    }
    input_sdl::init();
    ui::prewarm_text();   /* the common glyphs, before the first screen asks for them */

    /* HTTP: libcurl, or the fixtures. A finished request wakes the loop. */
    const Uint32 wake_event = SDL_RegisterEvents(1);
    std::unique_ptr<http::Transport> transport;
    if (opt.mock) {
        std::string fixtures = opt.demo ? REGIE5_SOURCE_DEMO : REGIE5_SOURCE_FIXTURES;
        if (char *base = SDL_GetBasePath()) {   /* beside the binary when installed */
            struct stat st;
            const std::string beside = std::string(base) + (opt.demo ? "demo" : "fixtures");
            if (stat(beside.c_str(), &st) == 0)
                fixtures = beside;
            SDL_free(base);
        }
        if (const char *dir = std::getenv("REGIE5_MOCK_DIR"))   /* another copy (make-demo.py --out) */
            fixtures = dir;
        const char *delay = std::getenv("REGIE5_MOCK_DELAY_MS");
        const char *fail = std::getenv("REGIE5_MOCK_FAIL");
        const char *empty = std::getenv("REGIE5_MOCK_EMPTY");
        const char *old = std::getenv("REGIE5_MOCK_OLD");
        transport = std::make_unique<api::MockTransport>(fixtures, delay ? std::atoi(delay) : 250, fail ? fail : "",
                                                         empty ? empty : "", old ? old : "");
    } else if (const char *h = std::getenv("REGIE5_HTTP"); h && std::string(h) == "socket") {
        /* The PS5's transport, to try it here against the real servers. */
        const bool log = std::getenv("REGIE5_HTTP_LOG") != nullptr;
        auto sock = std::make_unique<platform_posix::SocketTransport>(
            log ? std::function<void(const std::string &)>([](const std::string &l) { std::fprintf(stderr, "%s\n", l.c_str()); })
                : nullptr);
        /* https: the system's authorities (REGIE5_CA: another file). */
        const char *ca = std::getenv("REGIE5_CA");
        std::string pem;
        if (FILE *f = std::fopen(ca ? ca : "/etc/ssl/certs/ca-certificates.crt", "rb")) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
                pem.append(buf, n);
            std::fclose(f);
        }
        const int authorities = sock->set_authorities(pem);
        if (log)
            std::fprintf(stderr, "http: sockets, TLS %s, %d certificate authorities\n",
                         platform_posix::SocketTransport::has_tls() ? "on" : "off", authorities);
        transport = std::move(sock);
    } else {
        transport = std::make_unique<platform_linux::CurlTransport>();
    }
    if (const char *kib = std::getenv("REGIE5_WORKER_STACK_KIB"))   /* e.g. 64, the PS5's default */
        http::set_worker_stack((size_t)std::atoi(kib) << 10);
    http::Pool pool(*transport, 4, [wake_event] {
        SDL_Event e{};
        e.type = wake_event;
        SDL_PushEvent(&e);
    });
    http::Pool images(*transport, 3, [wake_event] {
        SDL_Event e{};
        e.type = wake_event;
        SDL_PushEvent(&e);
    });
    api::Services services(cfg, pool, i18n::code());
    art::init(&services, &images);

    ui::Context ctx;
    ctx.version = REGIE5_VERSION;
    ctx.config_path = config_path;
    ctx.config_error = cfg_error;
    ctx.mock = opt.mock;
    ctx.services = &services;
    if (!opt.mock) {   /* --mock keeps its settings in memory: nothing of the user's is touched */
        ctx.settings_path = app_settings::path_beside(config_path);
        if (!app_settings::load(ctx.settings_path, &ctx.settings))
            std::fprintf(stderr, "regie5: %s: unreadable, defaults used\n", ctx.settings_path.c_str());
    } else if (const char *c = std::getenv("REGIE5_MOCK_CLEANUP")) {
        ctx.settings.cleanup_enabled = *c && *c != '0';   /* for captures: the Ménage already on */
    }
    {   /* the welcome screens at a first start; REGIE5_WELCOME=1 opens them anyway */
        welcome::Start ws;
        ws.error = cfg_error;
        ws.mock = opt.mock;
        ws.later = ctx.settings.welcome_later;
        const char *w = std::getenv("REGIE5_WELCOME");
        ws.forced = w && *w && *w != '0';
        ctx.welcome_request = welcome::should_show(ws, cfg);
    }
    {   /* "What's new", once per version; never on a first start (no settings.json yet) */
        whats_new::Start wn;
        wn.current = ctx.version;
        wn.seen = ctx.settings.seen_version;
        struct stat st;
        wn.ran_before = !ctx.settings_path.empty() && stat(ctx.settings_path.c_str(), &st) == 0;
        wn.welcome = ctx.welcome_request;
        wn.mock = opt.mock;
        const char *w = std::getenv("REGIE5_WHATSNEW");
        wn.forced = w && *w && *w != '0';
        ctx.whats_new_request = whats_new::should_show(wn);
        if (!opt.mock && ctx.settings.seen_version != ctx.version) {
            ctx.settings.seen_version = ctx.version;
            ctx.save_settings();
        }
    }
    {   /* Who Régie5 is to Jellyfin (Quick Connect): an id made once, kept in settings.json */
        if (ctx.settings.device_id.empty()) {
            unsigned char raw[16];
            if (platform::random_bytes(raw, sizeof raw)) {
                static const char *const kHex = "0123456789abcdef";
                for (unsigned char b : raw)
                    ctx.settings.device_id += std::string(1, kHex[b >> 4]) + kHex[b & 15];
                ctx.save_settings();
            }
        }
        services.set_client(ctx.settings.device_id.empty() ? "regie5" : ctx.settings.device_id, "Regie5 (Linux)", ctx.version);
    }
    ui::Shell shell(ctx);
    input::Repeater repeater;
    ui::Script script(std::getenv("REGIE5_SCRIPT"), gfx_gl::read_canvas);

    const bool show_fps = std::getenv("REGIE5_FPS") != nullptr;
    perf::enable(show_fps);
    ui::FrameReport report;

    bool quit = false, animating = true, drew_last = false;
    int idle_frames = 0;
    /* After a pause, the first frame steps one frame of the monitor's, not the wait. */
    const int frame_hz = platform::display_mode().hz > 0 ? platform::display_mode().hz : 60;
    report.set_hz(frame_hz);
    if (show_fps)
        std::fprintf(stderr, "display: %dx%d at %d Hz\n", platform::display_mode().width,
                     platform::display_mode().height, frame_hz);
    double last = platform::now();
    time_t last_minute = time(nullptr) / 60;
    uint64_t frames = 0;
    while (!quit) {
        /* Nothing moving and no key held: sleep until an event (or the clock's minute). */
        if (!animating && idle_frames >= 2 && !repeater.waiting()) {
            SDL_WaitEventTimeout(nullptr, script.active() ? 10 : 500);
            drew_last = false;
        }
        perf::begin(gfx::frame_index() + 1);   /* the frame this turn may draw */
        perf::Frame &pf = perf::cur();
        bool dirty = false;
        SDL_Event ev;
        double t_phase = show_fps ? perf::now_ms() : 0;
        auto phase_done = [&](perf::Phase p) {
            if (!show_fps)
                return;
            const double t = perf::now_ms();
            pf.phase_ms[p] += t - t_phase;
            t_phase = t;
        };
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                quit = true;
            else if (ev.type == SDL_WINDOWEVENT &&
                     (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                fit_drawable();
                dirty = true;
            }
            if (!text_linux::handle_event(ev))   /* an open text entry has the keyboard */
                input_sdl::handle_event(ev);
        }
        phase_done(perf::Events);
        if (pool.pump() + images.pump() > 0)
            dirty = true;   /* answers arrived */
        phase_done(perf::Pump);
        const double now = platform::now();
        const uint32_t scripted = script.step(now, &quit);
        if (script.wants_frame())
            dirty = true;
        const std::string typed = script.take_text();
        if (!typed.empty())
            text_linux::type(typed);
        uint32_t went_down = input_sdl::take_went_down() | scripted;
        if (platform::text_entry_active())
            went_down = 0;   /* the keyboard is typing, not steering */
        const input::State in = repeater.update(went_down, input_sdl::held(), now);
        if (shell.input(in, now))
            dirty = true;
        const time_t minute = time(nullptr) / 60;
        if (minute != last_minute) {
            last_minute = minute;
            dirty = true;
        }
        phase_done(perf::Input);
        if (dirty || animating || idle_frames < 2) {
            /* After a pause, the first frame steps one frame's worth, not the wait. */
            const double gap = now - last;
            const float dt = drew_last ? (float)std::min(0.1, gap) : 1.f / (float)frame_hz;
            last = now;
            gfx::begin_frame();
            animating = shell.draw(now, dt);
            phase_done(perf::Draw);
            gfx::end_frame();   /* submit and present: timed by the renderer */
            script.after_frame();
            idle_frames = (dirty || animating) ? 0 : idle_frames + 1;
            if ((++frames % 120) == 0)
                gfx::collect();
            if (show_fps)   /* a gap after a wait is not a hitch */
                report.drawn(gfx::frame_index(), drew_last ? gap * 1000.0 : 0.0, now);
            drew_last = true;
        } else {
            drew_last = false;
            SDL_Delay(4);   /* a key held, nothing to draw yet */
        }
    }

    input_sdl::shutdown();
    gfx_gl::shutdown();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    return 0;
}
