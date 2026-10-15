/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The PS5 shell: the screen through ps5-opengl (EGL, OpenGL 4.6 Core, which
 * runs the GL 3.3 core renderer unchanged), the DualSense, and the same loop
 * as on Linux. A title never returns from main nor calls exit(): it asks the
 * system to close it (ps5::quit).
 *
 * The probe build (REGIE5_PROBE, phase 6.1) runs the demo data (the fixtures,
 * in the title's folder): no network, no config. It tours every screen by
 * itself for about a minute, measuring each frame, then hands over to the
 * controller, and closes itself four minutes later (or at once when Options
 * and Circle are held together for two seconds). If the previous launch did
 * not get to the end of its tour and a file named safe-mode is in the title's
 * folder, it starts in a safe mode that draws nothing and stays up five
 * minutes, so that its log can be read over FTP. Every step of the start is
 * logged over UDP before it runs, and a crash is reported there.
 */
#include "api/http.h"
#include "api/mock_transport.h"
#include "core/app_settings.h"
#include "api/services.h"
#include "core/config.h"
#include "core/i18n.h"
#include "core/welcome.h"
#include "core/input.h"
#include "core/perf.h"
#include "platform/gl/gfx_gl.h"
#include "platform/platform.h"
#include "platform/ps5/display_egl.h"
#include "platform/ps5/log_udp.h"
#include "platform/ps5/pad_ps5.h"
#include "platform/ps5/system_ps5.h"
#include "platform/posix/socket_transport.h"
#include "ui/art.h"
#include "ui/font.h"
#include "ui/frame_report.h"
#include "ui/gfx.h"
#include "ui/script.h"
#include "ui/shell.h"
#include "ui/style.h"

#include "cJSON.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#if __has_include("platform/ps5/build_info.h")
#include "platform/ps5/build_info.h"   /* written by scripts/build-ps5.sh */
#endif
#ifndef REGIE5_VERSION
#define REGIE5_VERSION "dev"
#endif
/* 1 when the title's param.json allows asking for 120 Hz (attribute3 bit
 * 0x40, written by build-ps5.sh): without it the console refuses the mode. */
#ifndef REGIE5_HZ120
#define REGIE5_HZ120 1
#endif
#ifndef REGIE5_LOG_HOST
#define REGIE5_LOG_HOST ""
#endif
#ifndef REGIE5_TITLE_ID
#define REGIE5_TITLE_ID "PPSA99520"
#endif
#ifndef REGIE5_LOG_PORT
#define REGIE5_LOG_PORT 5555
#endif

namespace {

constexpr const char *kDir = "/download0/regie5/dev";
constexpr const char *kLog = "/download0/regie5/dev/app.log";
constexpr const char *kPrevLog = "/download0/regie5/dev/app.prev.log";
constexpr const char *kRunning = "/download0/regie5/dev/running";
[[maybe_unused]] constexpr const char *kSettings = "/download0/regie5/settings.json";
[[maybe_unused]] constexpr const char *kConfigCopy = "/download0/regie5/config.json";
[[maybe_unused]] constexpr const char *kLaunches = "/download0/regie5/launches";
constexpr const char *kFirstFrame = "[regie5] first frame presented";
#ifdef REGIE5_PROBE
constexpr bool kProbe = true;
/* Safe mode: what a launch that went far enough wrote in its log. */
constexpr const char *kTourDone = "[regie5] probe: tour finished";
#else
constexpr bool kProbe = false;
constexpr const char *kTourDone = kFirstFrame;
#endif
/* Whether host names can be looked up (getaddrinfo exported by the console). */
bool s_resolve_names = true;
constexpr double kSafeModeStay = 300;
#ifdef REGIE5_PROBE
constexpr double kStayAfterTour = 240;   /* seconds the probe stays up after its tour */
#endif

/* Every screen in turn, the sheets and menus opened and closed with Circle
 * (nothing is ever confirmed; it is demo data anyway), a fast scroll through
 * the films. About 70 seconds. */
constexpr const char *kTour =
    "4 up cross 2 circle 1 down down options 2 circle 1 up "
    "r1 3 down cross 2 options 2 circle 1 circle 1 up "
    "right cross 3 down r2 0.4 r2 0.4 r2 0.4 r2 0.4 r2 0.4 r2 0.4 down down down down down 1 "
    "l2 0.3 l2 0.3 l2 0.3 l2 0.3 l2 0.3 l2 0.3 l2 0.3 1 up "
    "right cross 3 down 2 up right cross 3 "
    "right cross 3 right cross 1 right cross 3 down options 2 circle 1 "
    "r1 3 right cross 4 "
    "r1 3 down cross 2 circle 1 right right 3 "
    "r1 3 cross 3 circle 1 "
    "r1 4";

/* The 120 Hz bench (a file named bench in the title's folder): the tour above
 * runs by itself, and each frame is timed in parts: the drawing (the
 * screens, on the CPU), then the scene sent and finished by the GPU, waited
 * for before the copy to the screen (gfx_gl::set_bench), then that copy with
 * the wait for a free screen buffer, then the swap. A frame's cost is the
 * first two. Every sheet the tour opens is closed with Circle: nothing is
 * ever confirmed. */
constexpr const char *kBenchFile = "/app0/bench";
bool s_bench = false;

/* The 120 Hz endurance test (a file named stress in the title's folder; a
 * number of minutes in it, 10 if none): 120 Hz asked whatever Settings say,
 * the tour above run again and again for that long, the heap logged every
 * 10 s and a line per tour. Frames are not waited for (unlike the bench): the
 * GPU queue behaves as in normal use. A crash, an abort or the GPU runtime's
 * _Exit is reported as usual. Every sheet is closed with Circle: nothing is
 * ever confirmed, only GET requests reach the servers. */
constexpr const char *kStressFile = "/app0/stress";
double s_stress_minutes = 0;   /* 0: off */
/* Each tour after the first: back to Home first (the tour ends on Settings,
 * and R1 / L1 do not wrap around). */
constexpr const char *kStressAgain = "circle 1 circle 1 l1 0.5 l1 0.5 l1 0.5 l1 0.5 l1 0.5 l1 2 ";

/* The endurance test reaches the real servers for a long time: anything but
 * GET is refused before it leaves (the tour never confirms; this makes sure). */
class GetOnly : public http::Transport {
public:
    explicit GetOnly(http::Transport &inner) : m_inner(inner) {}
    http::Response perform(const http::Request &req, const std::atomic<bool> *cancel) override
    {
        if (req.method == "GET")
            return m_inner.perform(req, cancel);
        ps5::log("[regie5] stress: a %s request refused (GET only during the test)", req.method.c_str());
        http::Response r;
        r.fail = http::Response::Other;
        r.error = "refused during the stress test";
        return r;
    }

private:
    http::Transport &m_inner;
};

ps5::Display *s_display = nullptr;

void present() { s_display->swap(); }

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool file_contains(const char *path, const char *needle)
{
    FILE *f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::string text;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        text.append(buf, n);
    std::fclose(f);
    return text.find(needle) != std::string::npos;
}

void touch(const char *path)
{
    if (FILE *f = std::fopen(path, "wb"))
        std::fclose(f);
}

std::string read_all(const char *path, bool *ok)
{
    std::string text;
    *ok = false;
    FILE *f = std::fopen(path, "rb");
    if (!f)
        return text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        text.append(buf, n);
    *ok = !std::ferror(f);
    std::fclose(f);
    return text;
}

[[maybe_unused]] bool same_file(const char *a, const char *b)
{
    bool ok_a, ok_b;
    const std::string x = read_all(a, &ok_a), y = read_all(b, &ok_b);
    return ok_a && ok_b && x == y;
}

/* Written beside, then renamed over: never half a file. */
[[maybe_unused]] bool copy_file(const char *from, const char *to)
{
    bool ok;
    const std::string text = read_all(from, &ok);
    if (!ok)
        return false;
    const std::string tmp = std::string(to) + ".new";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !written)
        return false;
    return std::rename(tmp.c_str(), to) == 0;
}

/* Risk R2: does the app's data folder keep its files across restarts and
 * updates? A counter there tells. */
[[maybe_unused]] void log_launches()
{
    bool ok;
    const long before = std::atol(read_all(kLaunches, &ok).c_str());
    const long now = before + 1;
    if (FILE *f = std::fopen(kLaunches, "wb")) {
        std::fprintf(f, "%ld\n", now);
        std::fclose(f);
    }
    ps5::log("[regie5] launch #%ld of this app (counted in /download0/regie5: %s)", now,
             now == 1 ? "first launch, or the folder was emptied" : "kept since the last one");
}

/* Phase 6.5 (configuring from a phone through a local page): can a title
 * open a listening TCP port? Opened and closed at once, nothing served.
 * bind and listen are weak: a console without them answers "no".
 * 0.3.0 (Jelly5's param.json) is refused: each call is reported on its own,
 * for port 8085 and for a port the system picks, so the log says which one
 * the console stops. */
#pragma weak bind
#pragma weak listen
[[maybe_unused]] std::string try_listen(int port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return std::string("socket refused: ") + std::strerror(errno);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    std::string result;
    if (bind(fd, (const sockaddr *)&a, sizeof a) != 0)
        result = std::string("bind refused: ") + std::strerror(errno) + " (errno " + std::to_string(errno) + ")";
    else if (listen(fd, 1) != 0)
        result = std::string("bind OK, listen refused: ") + std::strerror(errno) + " (errno " + std::to_string(errno) + ")";
    else
        result = "POSSIBLE (opened and closed at once)";
    close(fd);
    return result;
}

[[maybe_unused]] void log_can_listen()
{
    if (!&bind || !&listen) {
        ps5::log("[regie5] listening port: bind/listen not exported to titles");
        return;
    }
    ps5::log("[regie5] listening port 8085: %s", try_listen(8085).c_str());
    ps5::log("[regie5] listening port chosen by the system: %s", try_listen(0).c_str());
}

/* The fields of the installed param.json that decide the tile's area, the
 * start, 120 Hz and (0.3.0) listening: the log says which profile ran. */
[[maybe_unused]] void log_param_profile()
{
    bool ok;
    const std::string text = read_all("/app0/sce_sys/param.json", &ok);
    cJSON *p = ok ? cJSON_Parse(text.c_str()) : nullptr;
    if (!p) {
        ps5::log("[regie5] param.json: unreadable");
        return;
    }
    auto num = [p](const char *key) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(p, key);
        return cJSON_IsNumber(v) ? (long long)v->valuedouble : -1LL;
    };
    ps5::log("[regie5] param.json: category %lld, badge %lld, attribute 0x%llx, attribute2 0x%llx, attribute3 0x%llx, "
             "amm %s, kernel %s, gameIntent %s, contentVersion %s",
             num("applicationCategoryType"), num("contentBadgeType"), num("attribute"), num("attribute2"),
             num("attribute3"), cJSON_HasObjectItem(p, "amm") ? "yes" : "no",
             cJSON_HasObjectItem(p, "kernel") ? "yes" : "no", cJSON_HasObjectItem(p, "gameIntent") ? "yes" : "no",
             cJSON_IsString(cJSON_GetObjectItem(p, "contentVersion"))
                 ? cJSON_GetObjectItem(p, "contentVersion")->valuestring
                 : "?");
    cJSON_Delete(p);
}

/* Whether the title's folder can be written by the app (for phase 6.5). */
[[maybe_unused]] void log_title_folder_writable()
{
    const char *probe = "/app0/.regie5-write-test";
    FILE *f = std::fopen(probe, "wb");
    if (f) {
        std::fclose(f);
        std::remove(probe);
    }
    ps5::log("[regie5] the title's folder (/app0) is %s by the app", f ? "WRITABLE" : "read-only");
}

/* The loader binds an import the console does not export to NULL, silently
 * (Jelly5 met it with strcasestr). The imports Regie5 uses beyond those of a
 * title already run on consoles (ps5-homebrew-ui) are checked first. */
extern "C" {
double atof(const char *);
double modf(double, double *);
long lround(double);
int tolower(int);
int toupper(int);
int ungetc(int, FILE *);
int socket(int, int, int);
int sceSystemServiceParamGetInt(int, int *);
int regie5_note_server_utc(int64_t server_utc, int64_t *zone_part, int64_t *drift_part);   /* time_ps5.c */
void regie5_time_report(int64_t *raw, int64_t *kernel_offset, int64_t *dst, int64_t *skew);
}

bool imports_present()
{
    static const struct {
        const char *name;
        const void *addr;
    } kImports[] = {{"atof", (const void *)&atof},
                    {"modf", (const void *)&modf},
                    {"lround", (const void *)&lround},
                    {"tolower", (const void *)&tolower},
                    {"toupper", (const void *)&toupper},
                    {"ungetc", (const void *)&ungetc},
                    {"socket", (const void *)&socket},
                    {"pthread_attr_setstacksize", (const void *)&pthread_attr_setstacksize},
                    /* HTTP over sockets (platform/posix/socket_transport.cpp) */
                    {"connect", (const void *)&connect},
                    {"send", (const void *)&send},
                    {"recv", (const void *)&recv},
                    {"poll", (const void *)&poll},
                    {"fcntl", (const void *)&fcntl},
                    {"getsockopt", (const void *)&getsockopt},
                    {"setsockopt", (const void *)&setsockopt},
                    {"sceSystemServiceParamGetInt", (const void *)&sceSystemServiceParamGetInt}};
    bool ok = true;
    for (const auto &i : kImports)
        if (!i.addr) {
            ps5::log("[regie5] import missing on this console: %s", i.name);
            ok = false;
        }
    ps5::log("[regie5] imports checked: %s", ok ? "all present" : "SOME MISSING");
    /* Looking up a host name: without it, numeric addresses still work. */
    s_resolve_names = (const void *)&getaddrinfo != nullptr && (const void *)&freeaddrinfo != nullptr;
    if (!s_resolve_names)
        ps5::log("[regie5] getaddrinfo missing on this console: only numeric addresses (192.168.x.y) will work");
    return ok;
}

/* A step of the start: logged, with the heap, before it runs. */
void step(const char *what)
{
    ps5::log("[regie5] step: %s", what);
    ps5::log_heap(what);
}

void trace(const char *what) { ps5::log_heap(what); }

/* The stack new threads get, and what the HTTP workers (which parse and
 * decode on theirs) ask for instead. */
void log_thread_stack()
{
    pthread_attr_t attr;
    size_t size = 0;
    if (pthread_attr_init(&attr) == 0) {
        pthread_attr_getstacksize(&attr, &size);
        pthread_attr_destroy(&attr);
    }
    ps5::log("[regie5] default thread stack: %zu KiB; the HTTP workers ask for %zu KiB", size / 1024,
             http::kWorkerStack / 1024);
}

/* Draws nothing; leaves the log readable for a while, then closes. */
[[noreturn]] void stay_then_quit(double seconds, const char *why)
{
    ps5::log("[regie5] %s: staying up %.0f s so the log can be read, then closing", why, seconds);
    const double until = platform::now() + seconds;
    int last = -1;
    while (platform::now() < until) {
        const int left = (int)(until - platform::now());
        if (left / 30 != last) {
            last = left / 30;
            ps5::log("[regie5] closing in %d s", left);
        }
        ps5::sleep_us(200000);
    }
    std::remove(kRunning);
    ps5::quit();
}

/* What the tour measured: every frame drawn while something moved. */
struct TourStats {
    long frames = 0, over_17 = 0, over_25 = 0, over_50 = 0;
    double worst = 0, sum = 0;
    void add(double gap_ms)
    {
        frames++;
        sum += gap_ms;
        worst = std::max(worst, gap_ms);
        over_17 += gap_ms > 17.5;
        over_25 += gap_ms > 25;
        over_50 += gap_ms > 50;
    }
};

/* The bench's frames, in parts (ms), against the period of the display. */
struct BenchStats {
    std::vector<float> draw, scene, copy, swap, cost;
    void add(double d, double sc, double cp, double sw)
    {
        draw.push_back((float)d), scene.push_back((float)sc), copy.push_back((float)cp), swap.push_back((float)sw);
        cost.push_back((float)(d + sc));
    }
    static std::string summary(std::vector<float> v)
    {
        std::sort(v.begin(), v.end());
        double sum = 0;
        for (float x : v)
            sum += x;
        char buf[96];
        std::snprintf(buf, sizeof buf, "avg %.2f p95 %.2f worst %.2f", sum / v.size(),
                      v[std::min(v.size() - 1, (size_t)(0.95 * (double)v.size()))], v.back());
        return buf;
    }
    void log(const char *what, int width, int height, int hz)
    {
        if (cost.empty())
            return;
        long over60 = 0, over120 = 0;
        for (float c : cost)
            over60 += c > 1000.0 / 60, over120 += c > 1000.0 / 120;
        ps5::log("[regie5] bench %s: %dx%d at %d Hz, %zu frames | cost (drawing + scene) %s ms | over 16.67 ms: "
                 "%.1f %%, over 8.33 ms: %.1f %% | drawing %s | scene on the GPU %s | copy + wait for the screen %s | "
                 "swap %s",
                 what, width, height, hz, cost.size(), summary(cost).c_str(), 100.0 * over60 / cost.size(),
                 100.0 * over120 / cost.size(), summary(draw).c_str(), summary(scene).c_str(), summary(copy).c_str(),
                 summary(swap).c_str());
        *this = BenchStats{};
    }
};

} // namespace

/* What the display took: the size, and the rate the TV accepted. */
platform::DisplayMode platform::display_mode()
{
    DisplayMode m;
    if (!s_display)
        return m;
    m.width = s_display->width();
    m.height = s_display->height();
    m.hz = s_display->refresh();
    m.asked_hz = s_display->asked_hz();
    m.can_120 = REGIE5_HZ120 != 0;
    return m;
}

int main()
{
    mkdir("/download0/regie5", 0755);
    mkdir(kDir, 0755);
    /* The log first, over UDP at once, then the crash report: everything
     * after this line reaches the laptop even if the title dies. */
    ps5::log_init(REGIE5_LOG_HOST, REGIE5_LOG_PORT);
    ps5::log("[regie5] Régie5 %s (PS5%s), log to udp %s:%d", REGIE5_VERSION,
#ifdef REGIE5_PROBE
             " probe",
#else
             "",
#endif
             *REGIE5_LOG_HOST ? REGIE5_LOG_HOST : "(none)", REGIE5_LOG_PORT);
    ps5::install_crash_handler();
    signal(SIGPIPE, SIG_IGN);   /* a server that closes early: an error, not the end of the title */
    ps5::start_udp_mirror(kLog);
    ps5::log("[regie5] step: heap");
    const bool heap_up = ps5::log_heap_reservation();
    ps5::log_heap("at start");
    log_thread_stack();
    if (!heap_up) {
        /* The system's malloc is far too small for the title (probe-4 died
         * of it in the fonts): stop here, plainly, rather than crash later. */
        ps5::log("[regie5] STOP: the console refused the title's memory (every heap size above); "
                 "nothing is drawn. Close other apps and launch again.");
        std::remove(kRunning);
        ps5::sleep_us(2000000);   /* time for the lines to leave */
        ps5::quit();
    }

    /* Safe mode is opt-in: a file named safe-mode in the title's folder
     * (/data/homebrew/PPSA99521/safe-mode). With it, a launch that follows
     * one that did not finish its tour draws nothing and stays up a while. */
    const bool safe_mode_on = file_exists("/app0/safe-mode");
    ps5::log("[regie5] step: safe mode %s", safe_mode_on ? "on (/app0/safe-mode)" : "off");
    if (safe_mode_on && file_exists(kRunning) && !file_contains(kPrevLog, kTourDone))
        stay_then_quit(kSafeModeStay, "safe mode: the previous launch did not finish (its log is app.prev.log)");
    touch(kRunning);
    ps5::log("[regie5] step: imports");
    if (!imports_present())
        stay_then_quit(kSafeModeStay, "a system function is missing");

    {   /* the console's time zone (risk R9: the calendar's days) */
        ps5::log("[regie5] step: time");
        const time_t t = time(nullptr);
        struct tm local, utc;
        localtime_r(&t, &local);
        gmtime_r(&t, &utc);
        ps5::log("[regie5] time: local %04d-%02d-%02d %02d:%02d, UTC %04d-%02d-%02d %02d:%02d (offset %ld s)",
                 local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                 utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, (long)local.tm_gmtoff);
        int64_t raw, koff, dst, skew;
        regie5_time_report(&raw, &koff, &dst, &skew);
        ps5::log("[regie5] time: clock %lld, kernel offset %lld s, summer time %lld s (checked against the servers "
                 "once one answers)",
                 (long long)raw, (long long)koff, (long long)dst);
        (void)skew;
    }
    ps5::log("[regie5] step: language");
    i18n::set_lang(i18n::find(platform::system_lang()));
    ps5::log("[regie5] language %s (system: %s)", i18n::code(), platform::system_lang().c_str());

    step("display (EGL, OpenGL 4.6)");
    ps5::Display display;
    s_display = &display;
    /* The refresh rate the user chose (Réglages, settings.json): read before
     * the display opens, the mode being fixed while it runs. */
    app_settings::Settings early;
#ifndef REGIE5_PROBE
    app_settings::load(kSettings, &early);
    if (file_exists(kStressFile)) {
        bool ok;
        const double minutes = std::atof(read_all(kStressFile, &ok).c_str());
        s_stress_minutes = minutes > 0 ? minutes : 10;
    }
#endif
    /* Never 120 Hz when the param.json does not declare it (0.3.1): the
     * console would refuse it anyway, and a 120 Hz kept in settings.json from
     * 0.3.0 is shown as unavailable in Settings. */
    const int asked_hz = !REGIE5_HZ120         ? 60
                         : s_stress_minutes > 0 ? 120
                                                : app_settings::refresh_asked(early.refresh);
    /* 4K, unless a file named output-1080p or output-1440p is in the title's
     * folder (/data/homebrew/PPSA99520/output-1440p): to compare. */
    const bool want_1080p = file_exists("/app0/output-1080p"), want_1440p = file_exists("/app0/output-1440p");
    const int ow = want_1080p ? 1920 : want_1440p ? 2560 : 3840, oh = want_1080p ? 1080 : want_1440p ? 1440 : 2160;
    ps5::log("[regie5] output asked: %dx%d%s, %d Hz (setting: %s%s)", ow, oh,
             want_1080p ? " (/app0/output-1080p)" : want_1440p ? " (/app0/output-1440p)" : "", asked_hz,
             app_settings::refresh_key(early.refresh), s_stress_minutes > 0 ? ", forced by /app0/stress" : "");
    if (!(display.open(ow, oh, asked_hz) || (ow != 1920 && display.open(1920, 1080, asked_hz))))
        stay_then_quit(kSafeModeStay, "the display did not open");
    step("renderer (shaders)");
    if (!gfx_gl::init(false))
        stay_then_quit(kSafeModeStay, "the renderer did not start (shaders)");
    gfx_gl::set_present(present);
    gfx_gl::resize(display.width(), display.height());
    step("fonts");
    if (!font::init())
        stay_then_quit(kSafeModeStay, "fonts missing (/app0/assets/fonts)");
    ps5::log("[regie5] renderer ready at %dx%d", display.width(), display.height());
    {   /* Console budgets: the SDK's render area (256 MiB) also holds the 4K
         * frame and its copies, so 192 MiB of pictures (Linux's) would not
         * fit; a picture is decoded in the heap before it goes to the GPU, and
         * each glyph page (16 MiB in 4K) keeps a 4 MiB copy there. 24 MiB of
         * pages held one 4K page: a second one started the atlas afresh. */
        const size_t heap = ps5::heap_size();
        const size_t pictures = heap >= (256u << 20) ? 64u << 20 : heap >= (192u << 20) ? 48u << 20 : 32u << 20;
        const size_t glyphs = heap >= (192u << 20) ? 48u << 20 : 32u << 20;
        art::set_budget(pictures);
        gfx::set_atlas_budget(glyphs);
        ps5::log("[regie5] budgets for a %zu MiB heap: pictures %zu MiB, glyph pages %zu MiB", heap >> 20,
                 pictures >> 20, glyphs >> 20);
    }
    step("glyphs");
    {
        const double t0 = platform::now();
        perf::begin(0);
        ui::prewarm_text();
        ps5::log("[regie5] glyphs ready in advance: %d in %.0f ms", perf::cur().glyphs,
                 (platform::now() - t0) * 1000.0);
    }

#ifdef REGIE5_PROBE
    /* The demo data, from the title's folder: no network at all. */
    step("demo data and threads");
    api::MockTransport transport("/app0/assets/fixtures", 0, "", "");
    const config::Config cfg = config::mock();
    [[maybe_unused]] const config::LoadError cfg_error = config::LoadError::None;
    http::Transport *net = &transport;
#else
    /* The real servers: config.json in the title's folder, HTTP over plain
     * sockets. Never logged: the file holds the keys. */
    step("config");
    /* Development builds only: a file named test-config in the title's
     * folder runs on config-test.json and settings-test.json (absent at first:
     * a first start, the welcome screens), never on the app's copy: the
     * user's own config.json and settings stay as they are. */
    const bool test_config = *REGIE5_LOG_HOST != '\0' && file_exists("/app0/test-config");
    const std::string config_path = test_config ? "/app0/config-test.json" : platform::default_config_path();
    if (test_config)
        ps5::log("[regie5] config: TEST MODE (/app0/test-config): config-test.json, settings-test.json, no copy");
    config::Config cfg;
    config::LoadError cfg_error = config::load(config_path, &cfg);
    ps5::log("[regie5] config %s: %s", config_path.c_str(),
             cfg_error == config::LoadError::None      ? "read"
             : cfg_error == config::LoadError::Missing ? "NOT FOUND (put config.json in /data/homebrew/" REGIE5_TITLE_ID "/)"
                                                       : "unreadable or not JSON");
    /* The app keeps a copy of the last config.json it could read: an update
     * that replaced the title's folder whole does not lose the servers. */
    bool from_backup = false;
    if (test_config) {
        /* neither the copy nor its update */
    } else if (cfg_error == config::LoadError::None) {
        if (!same_file(config_path.c_str(), kConfigCopy))
            ps5::log("[regie5] config: the app's copy %s", copy_file(config_path.c_str(), kConfigCopy) ? "updated" : "NOT written");
    } else if (cfg_error == config::LoadError::Missing && config::load(kConfigCopy, &cfg) == config::LoadError::None) {
        cfg_error = config::LoadError::None;
        from_backup = true;
        ps5::log("[regie5] config: the app's copy is used (%s)", kConfigCopy);
    }
    if (cfg.emby())
        ps5::log("[regie5] config: the media server is Emby");
    log_launches();
    log_title_folder_writable();
    log_param_profile();
    log_can_listen();
    ps5::log_launch_support();
    step("network and threads");
    platform_posix::SocketTransport transport(
        [](const std::string &line) { ps5::log("[regie5] %s", line.c_str()); }, s_resolve_names);
    {   /* where randomness comes from (the phone page's code, TLS) */
        unsigned char probe[16];
        const bool ok = platform::random_bytes(probe, sizeof probe);
        ps5::log("[regie5] random: %s", ok ? ps5::random_source() : "NONE (https off, the phone code weak)");
    }
    {   /* https: the authorities shipped with the title (Mozilla's) */
        std::vector<uint8_t> pem;
        platform::read_asset("certs/cacert.pem", &pem);
        const int n = transport.set_authorities(std::string(pem.begin(), pem.end()));
        ps5::log("[regie5] https: %s, %d certificate authorities",
                 platform_posix::SocketTransport::has_tls() ? "on" : "off", n);
    }
    transport.on_server_date([](int64_t utc) {
        int64_t zone = 0, drift = 0;
        if (!regie5_note_server_utc(utc, &zone, &drift))
            return;
        time_t now = time(nullptr);
        struct tm local, u;
        localtime_r(&now, &local);
        gmtime_r(&now, &u);
        ps5::log("[regie5] time: the console's clock runs %+lld s from the servers' UTC (zone %+lld s, drift "
                 "%+lld s): corrected; now local %02d:%02d:%02d, UTC %02d:%02d",
                 (long long)(zone + drift), (long long)zone, (long long)drift, local.tm_hour, local.tm_min,
                 local.tm_sec, u.tm_hour, u.tm_min);
    });
    GetOnly get_only(transport);
    http::Transport *net = &transport;
    if (s_stress_minutes > 0) {
        net = &get_only;
        ps5::log("[regie5] stress: on (%s), %.0f min at 120 Hz, the tour again and again; GET requests only",
                 kStressFile, s_stress_minutes);
    }
#endif
    i18n::set_emby(cfg.emby());
    http::Pool pool(*net, 4, nullptr);
    http::Pool images(*net, 3, nullptr);
    api::Services services(cfg, pool, i18n::code());
    art::init(&services, &images);
    step("screens");

    ui::Context ctx;
    ctx.version = REGIE5_VERSION;
    ctx.trace = trace;
    ctx.services = &services;
#ifdef REGIE5_PROBE
    ctx.config_path = "(demo)";
    ctx.mock = true;
    ctx.settings.cleanup_enabled = true;   /* the Ménage too, in memory only */
#else
    ctx.config_path = config_path;
    ctx.config_shown = std::string("/data/homebrew/" REGIE5_TITLE_ID "/") + (test_config ? "config-test.json" : "config.json");
    if (from_backup)
        ctx.config_shown += std::string(" (") + i18n::tr(i18n::ConfigFromBackup) + ")";
    ctx.config_error = cfg_error;
    /* settings.json: /app0 is the title's folder, written from a PC; the
     * app's own choices go to its data folder, which it can write. */
    ctx.settings_path = test_config ? "/download0/regie5/settings-test.json" : kSettings;
    if (!app_settings::load(ctx.settings_path, &ctx.settings))
        ps5::log("[regie5] %s: unreadable, defaults used", ctx.settings_path.c_str());
    {
        welcome::Start ws;
        ws.error = cfg_error;
        ws.from_backup = from_backup;
        ws.later = ctx.settings.welcome_later;
        ctx.welcome_request = welcome::should_show(ws, cfg);
        ps5::log("[regie5] welcome screens at start: %s (config %s, %s%s)", ctx.welcome_request ? "yes" : "no",
                 cfg_error == config::LoadError::None ? "read" : "not usable",
                 welcome::configured(cfg) ? "a service on" : "no service on",
                 ctx.settings.welcome_later ? ", \"later\" chosen before" : "");
    }
#endif
    ui::Shell shell(ctx);

    step("controller");
    ps5::Pad pad;
    pad.open();
    step("first frame");
    input::Repeater repeater;
#ifdef REGIE5_PROBE
    ui::Script tour(kTour);
#else
    s_bench = file_exists(kBenchFile);
    gfx_gl::set_bench(s_bench);
    ui::Script tour(s_bench || s_stress_minutes > 0 ? kTour : nullptr);
#endif
    const double stress_start = platform::now();
    int stress_tours = 0;
    bool stress_over = s_stress_minutes <= 0;
    BenchStats bench, bench_second;
    double bench_since = platform::now();
    bool bench_tour_logged = false;
    /* Frame timings once a second: development builds only (they send their
     * log to a laptop); a release would fill its log file with them. */
    perf::enable(*REGIE5_LOG_HOST != '\0');
    ui::FrameReport report;
    TourStats stats;

    bool quit = false, animating = true, drew_last = false, tour_over = !tour.active(), splash_hidden = false;
    int idle_frames = 0;
    double last = platform::now(), tour_end = 0, quit_hold_since = 0, last_heap_log = platform::now();
    time_t last_minute = time(nullptr) / 60;
    uint64_t frames = 0, last_resets = 0;
    for (;;) {
        perf::begin(gfx::frame_index() + 1);
        bool dirty = false;
        if (pool.pump() + images.pump() > 0)
            dirty = true;
        const double now = platform::now();
        uint32_t went_down = 0, held = 0;
        pad.read(&went_down, &held);
        const uint32_t scripted = tour.step(now, &quit);
        const input::State in = repeater.update(went_down | scripted, held, now);
        if (shell.input(in, now))
            dirty = true;
        if (time(nullptr) / 60 != last_minute) {
            last_minute = time(nullptr) / 60;
            dirty = true;
        }

        if (!stress_over && !tour.active()) {   /* one more tour, or the end of the test */
            stress_tours++;
            ps5::log("[regie5] stress: tour %d over at %.0f s of %.0f, %llu frames drawn at %d Hz | frame gaps: avg "
                     "%.2f ms, worst %.1f ms, over 17.5 ms: %ld, over 25 ms: %ld, over 50 ms: %ld",
                     stress_tours, now - stress_start, s_stress_minutes * 60, (unsigned long long)gfx::frame_index(),
                     display.refresh(), stats.frames ? stats.sum / stats.frames : 0.0, stats.worst, stats.over_17,
                     stats.over_25, stats.over_50);
            ps5::log_heap("stress");
            stats = TourStats{};
            if (now - stress_start >= s_stress_minutes * 60) {
                stress_over = true;
                ps5::log("[regie5] stress: FINISHED, %d tours in %.1f min, still running; the controller is yours",
                         stress_tours, (now - stress_start) / 60);
            } else {
                tour = ui::Script((std::string(kStressAgain) + kTour).c_str());
            }
        }
        if (now - last_heap_log > (kProbe || !stress_over ? 10 : 60)) {   /* the heap, and the glyph atlas's fresh starts */
            last_heap_log = now;
            ps5::log_heap(!kProbe ? "running" : tour_over ? "running (controller)" : "running (tour)");
            if (gfx::atlas_resets() != last_resets) {
                ps5::log("[regie5] glyph atlas started afresh %llu time(s) since the last line",
                         (unsigned long long)(gfx::atlas_resets() - last_resets));
                last_resets = gfx::atlas_resets();
            }
        }
#ifdef REGIE5_PROBE
        if (!tour_over && !tour.active()) {
            tour_over = true;
            tour_end = now;
            ps5::log("%s frames=%ld avg_ms=%.2f worst_ms=%.1f over_17ms=%ld over_25ms=%ld over_50ms=%ld", kTourDone,
                     stats.frames, stats.frames ? stats.sum / stats.frames : 0.0, stats.worst, stats.over_17,
                     stats.over_25, stats.over_50);
            ps5::log("[regie5] probe: the controller is yours; closing by itself in %.0f s "
                     "(or hold Options + Circle for 2 s)",
                     kStayAfterTour);
        }
        /* Options + Circle held together: close now. */
        const uint32_t both = input::Options | input::Circle;
        if ((held & both) == both) {
            if (quit_hold_since == 0)
                quit_hold_since = now;
            else if (now - quit_hold_since > 2)
                quit = true;
        } else {
            quit_hold_since = 0;
        }
        if (tour_over && now - tour_end > kStayAfterTour)
            quit = true;
#endif
        if (quit)
            break;

        if (dirty || animating || idle_frames < 2) {
            const double gap = now - last;
            /* After a pause, the first frame steps one period of the display, not the wait. */
            const float dt = drew_last ? (float)std::min(0.1, gap) : 1.f / (float)display.refresh();
            last = now;
            gfx::begin_frame();
            const double draw_t0 = perf::now_ms();
            animating = shell.draw(now, dt);
            const double draw_ms = perf::now_ms() - draw_t0;
            gfx::end_frame();   /* submit and present (vsync) */
            if (!splash_hidden) {
                splash_hidden = true;
                ps5::log("%s; splash %s", kFirstFrame, ps5::hide_splash_screen() ? "hidden" : "hide refused");
                ps5::log_heap("after the first frame");
                const int hz = display.refresh();
                ps5::log("[regie5] display: %dx%d at %d Hz (setting %s, %d Hz asked%s)", display.width(),
                         display.height(), hz, app_settings::refresh_key(early.refresh), display.asked_hz(),
                         display.asked_hz() > hz ? ": refused by the display (the driver's [ps5-output-mode] line has the code), 60 Hz kept"
                                                 : hz == 120 ? ", accepted" : "");
                report.set_hz(hz);
                if (s_bench)
                    ps5::log("[regie5] bench: on (%s), the tour runs by itself; each frame is timed in parts "
                             "(the GPU is waited for after the scene)",
                             kBenchFile);
            }
            if (s_bench && drew_last && perf::cur().scene_ms >= 0) {   /* after a pause: not part of a movement */
                const perf::Frame &pf = perf::cur();
                const double copy = pf.phase_ms[perf::Submit] - pf.scene_ms;
                if (!bench_tour_logged)
                    bench.add(draw_ms, pf.scene_ms, copy, pf.phase_ms[perf::Present]);
                bench_second.add(draw_ms, pf.scene_ms, copy, pf.phase_ms[perf::Present]);
            }
            if (s_bench && now - bench_since >= 5.0) {
                bench_second.log("5 s", display.width(), display.height(), display.refresh());
                bench_since = now;
            }
            if (s_bench && !tour.active() && !bench_tour_logged) {
                bench_tour_logged = true;
                bench.log("tour", display.width(), display.height(), display.refresh());
                ps5::log("[regie5] bench: tour over; the controller is yours (measures go on every 5 s)");
            }
            idle_frames = (dirty || animating) ? 0 : idle_frames + 1;
            if ((++frames % 120) == 0)
                gfx::collect();
            if (drew_last && !tour_over)
                stats.add(gap * 1000.0);
            report.drawn(gfx::frame_index(), drew_last ? gap * 1000.0 : 0.0, now);
            drew_last = true;
        } else {
            drew_last = false;
            ps5::sleep_us(4000);
        }
    }
    (void)tour_end;
    (void)quit_hold_since;

    ps5::log("[regie5] probe: closing (%s)", tour_over ? "time is up or asked" : "asked before the end of the tour");   /* the app never gets here: the system closes it */
    std::remove(kRunning);
    pad.close();
    ps5::quit();
}
