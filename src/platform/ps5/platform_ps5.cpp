/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * platform.h on the PS5. The title's folder (/data/homebrew/<TITLE_ID>,
 * written from a PC) is /app0 inside the title: the assets are there, and so
 * is config.json, which survives restarts and needs no access to /data (a
 * title's sandbox has none).
 *
 * Opening an item in Jelly5 (phase 6.4): sceSystemServiceLaunchApp, looked
 * up by name, with the handoff file docs/JELLY5-PR.md describes. Without a
 * launch function, Options says which title to look for in Jelly5.
 */
#include "platform/platform.h"

#include "platform/ps5/system_ps5.h"

#include <sys/stat.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

extern "C" {
int sceSystemServiceParamGetInt(int param_id, int *value);
int sceUserServiceGetForegroundUser(int *user) __attribute__((weak));
int sceUserServiceGetInitialUser(int *user);
/* The launch parameters, as the PS5 homebrew tools that start titles pass them. */
struct LaunchParam {
    uint32_t size;
    int32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
};
/* Two names for the launch in libSceSystemService. Linked by name, both
 * came out NULL on a 13.60 console (app-7c): the build tools file every
 * function of the module under one library, "libSceSystemService", and the
 * console publishes these under another library of the same module. So
 * they are also looked up by name while running (sceKernelDlsym, as the
 * payloads that start titles do), which searches the whole module. */
int sceSystemServiceLaunchApp(const char *title_id, const char **argv, LaunchParam *param) __attribute__((weak));
int sceLncUtilLaunchApp(const char *title_id, const char **argv, LaunchParam *param) __attribute__((weak));
int sceKernelLoadStartModule(const char *path, size_t argc, const void *argv, uint32_t flags, void *opt,
                             int *result) __attribute__((weak));
int sceKernelDlsym(int handle, const char *symbol, void **address) __attribute__((weak));
}

namespace {
std::atomic<int> s_random_source{0};   /* 0 not yet, 1 /dev/urandom, 2 RDRAND, -1 none */
using LaunchFn = int (*)(const char *, const char **, LaunchParam *);

struct Launcher {
    LaunchFn fn = nullptr;
    const char *how = "none";
    char detail[200] = "";
};

/* Looked for once, at start (the main thread), then only read. */
const Launcher &launcher()
{
    static const Launcher l = [] {
        Launcher r;
        int handle = -1;
        if (sceKernelLoadStartModule) {
            /* Already loaded by the app: its handle comes back. */
            handle = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, nullptr, 0, nullptr,
                                              nullptr);
            if (handle < 0)
                handle = sceKernelLoadStartModule("libSceSystemService.sprx", 0, nullptr, 0, nullptr, nullptr);
        }
        void *by_name[2] = {nullptr, nullptr};
        static const char *const kNames[2] = {"sceSystemServiceLaunchApp", "sceLncUtilLaunchApp"};
        int rc[2] = {-1, -1};
        if (handle >= 0 && sceKernelDlsym)
            for (int i = 0; i < 2; i++)
                rc[i] = sceKernelDlsym(handle, kNames[i], &by_name[i]);
        std::snprintf(r.detail, sizeof r.detail,
                      "module handle 0x%x%s; by name: %s %s (0x%x), %s %s (0x%x); linked: %s %s, %s %s",
                      (unsigned)handle, sceKernelDlsym ? "" : " (no sceKernelDlsym)", kNames[0],
                      by_name[0] ? "found" : "NOT found", (unsigned)rc[0], kNames[1],
                      by_name[1] ? "found" : "NOT found", (unsigned)rc[1], kNames[0],
                      &sceSystemServiceLaunchApp ? "bound" : "NOT bound", kNames[1],
                      &sceLncUtilLaunchApp ? "bound" : "NOT bound");
        if (by_name[0]) {
            r.fn = reinterpret_cast<LaunchFn>(by_name[0]);
            r.how = "sceSystemServiceLaunchApp (by name)";
        } else if (by_name[1]) {
            r.fn = reinterpret_cast<LaunchFn>(by_name[1]);
            r.how = "sceLncUtilLaunchApp (by name)";
        } else if (&sceSystemServiceLaunchApp) {
            r.fn = &sceSystemServiceLaunchApp;
            r.how = "sceSystemServiceLaunchApp (linked)";
        } else if (&sceLncUtilLaunchApp) {
            r.fn = &sceLncUtilLaunchApp;
            r.how = "sceLncUtilLaunchApp (linked)";
        }
        return r;
    }();
    return l;
}

LaunchFn launch_fn() { return launcher().fn; }
} // namespace

namespace platform {

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

bool read_asset(const std::string &rel, std::vector<uint8_t> *out)
{
    FILE *f = std::fopen(("/app0/assets/" + rel).c_str(), "rb");
    if (!f)
        return false;
    /* Straight into the vector: threads on the PS5 may have small stacks. */
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out->assign(size > 0 ? (size_t)size : 0, 0);
    if (size > 0)
        out->resize(std::fread(out->data(), 1, (size_t)size, f));
    std::fclose(f);
    return true;
}

std::string default_config_path() { return "/app0/config.json"; }

std::string system_lang()
{
    /* SCE_SYSTEM_SERVICE_PARAM_ID_LANG; 2 French (France), 22 French (Canada). */
    int lang = -1;
    if (sceSystemServiceParamGetInt(1, &lang) != 0)
        return {};
    return lang == 2 || lang == 22 ? "fr_FR" : "en_US";
}

namespace {
/* Set once the system has refused a launch: the menu entry goes. */
std::atomic<bool> s_launch_refused{false};
} // namespace

bool can_launch_apps() { return launch_fn() != nullptr && !s_launch_refused.load(); }

} // namespace platform

namespace ps5 {
const char *random_source()
{
    const int s = s_random_source.load();
    return s == 1 ? "/dev/urandom" : s == 2 ? "the CPU (RDRAND)" : s < 0 ? "none" : "not asked yet";
}

void log_launch_support()
{
    const Launcher &l = launcher();
    ps5::log("[regie5] open in Jelly5: %s", l.detail);
    ps5::log("[regie5] open in Jelly5: %s", l.fn ? l.how : "no launch function: Options offers \"Find in Jelly5\"");
}
} // namespace ps5

namespace platform {

/* The handoff file first (Jelly5 reads it, then deletes it), then the title
 * with its arguments. The file may not be writable from a title's sandbox:
 * the arguments say the same. */
bool launch_app(const Launch &l, std::string *error)
{
    if (!l.handoff_path.empty()) {
        const size_t slash = l.handoff_path.rfind('/');
        if (slash != std::string::npos && slash > 0)
            mkdir(l.handoff_path.substr(0, slash).c_str(), 0755);
        FILE *f = std::fopen(l.handoff_path.c_str(), "wb");
        const bool written = f && std::fwrite(l.handoff.data(), 1, l.handoff.size(), f) == l.handoff.size();
        if (f)
            std::fclose(f);
        ps5::log("[regie5] launch: handoff %s %s", l.handoff_path.c_str(), written ? "written" : "NOT written");
    }
    std::vector<const char *> argv;
    for (const std::string &a : l.args)
        argv.push_back(a.c_str());
    argv.push_back(nullptr);
    LaunchParam p{};
    p.size = sizeof p;
    int user = -1;
    if (!sceUserServiceGetForegroundUser || sceUserServiceGetForegroundUser(&user) != 0)
        sceUserServiceGetInitialUser(&user);
    p.user_id = user;
    const LaunchFn fn = launch_fn();
    if (!fn) {
        *error = "no launch function on this console";
        return false;
    }
    const int rc = fn(l.title_id.c_str(), argv.data(), &p);
    ps5::log("[regie5] launch: %s by %s (%zu argument(s)) for user 0x%x -> 0x%08x", l.title_id.c_str(),
             launcher().how, l.args.size(), (unsigned)user, (unsigned)rc);
    if (rc == 0)
        return true;
    if ((unsigned)rc == 0x80940005u)   /* refused to a title (as some homebrew tools report it) */
        s_launch_refused = true;
    char buf[64];
    std::snprintf(buf, sizeof buf, "0x%08x", (unsigned)rc);
    *error = buf;
    return false;
}

namespace {

/* RDRAND: the CPU's own random source (the PS5's Zen 2 has it), no system
 * import needed. Some Zen 2 firmware once answered all ones forever: such
 * an answer is refused. */
bool rdrand64(uint64_t *v)
{
    for (int tries = 0; tries < 10; tries++) {
        unsigned char ok = 0;
        uint64_t x = 0;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(x), "=qm"(ok));
        if (ok && x != ~0ull && x != 0) {
            *v = x;
            return true;
        }
    }
    return false;
}


} // namespace

bool random_bytes(void *out, size_t n)
{
    if (s_random_source.load() != 2) {
        if (FILE *f = std::fopen("/dev/urandom", "rb")) {
            const bool ok = std::fread(out, 1, n, f) == n;
            std::fclose(f);
            if (ok) {
                s_random_source = 1;
                return true;
            }
        }
    }
    auto *p = static_cast<unsigned char *>(out);
    while (n > 0) {
        uint64_t v;
        if (!rdrand64(&v)) {
            s_random_source = -1;
            return false;
        }
        const size_t part = n < 8 ? n : 8;
        std::memcpy(p, &v, part);
        p += part;
        n -= part;
    }
    s_random_source = 2;
    return true;
}

} // namespace platform
