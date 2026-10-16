/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/ps5/system_ps5.h"

#include "core/boot_guard.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <__verbose_abort>

extern "C" {
int sceKernelDebugOutText(int channel, const char *text);
int sceKernelUsleep(unsigned int microseconds);
int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *path, const char **arguments);
void regie5_release_splash(void) __attribute__((weak));
void regie5_heap_stats(size_t *size, size_t *live, size_t *peak, size_t *blocks, size_t *failures);
int regie5_heap_state(void);
int regie5_heap_attempts(size_t *sizes, int *results, int room);
}

namespace ps5 {

namespace {
int s_fd = -1;
sockaddr_in s_addr;

void send_raw(const char *text, size_t n)
{
    if (s_fd >= 0)
        sendto(s_fd, text, n, 0, (const sockaddr *)&s_addr, sizeof s_addr);
}

/* A line to the UDP log and to the log file, from a fixed buffer: usable
 * where allocating is not (a crash, a failed allocation). */
void emergency(const char *format, ...) __attribute__((format(printf, 1, 2)));
void emergency(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(line, sizeof line - 1, format, args);
    va_end(args);
    if (n <= 0)
        return;
    size_t used = std::strlen(line);
    line[used++] = '\n';
    send_raw(line, used);
    (void)!write(2, line, used);
}

/* How far up a crash report may read a stack: to the top noted for this
 * thread (note_thread_stack: main, the HTTP workers), else to the end of the
 * page sp is in. Reading past a stack's top faulted in the handler itself
 * (emby-test-1 on a 12.40 console, 16 October 2026): the console then showed
 * that second fault, and the crash itself was lost. A fixed table, not a
 * thread_local: the toolchain emulates those (a lookup that may allocate). */
struct StackTop {
    std::atomic<pthread_t> thread{};
    std::atomic<uintptr_t> top{0};
};
StackTop s_tops[32];
std::atomic<int> s_top_count{0};

uintptr_t stack_top_of_this_thread()
{
    const pthread_t self = pthread_self();
    const int n = std::min(s_top_count.load(), (int)(sizeof s_tops / sizeof s_tops[0]));
    for (int i = 0; i < n; i++)
        if (pthread_equal(s_tops[i].thread.load(), self))
            return s_tops[i].top.load();
    return 0;
}

int words_to_read(const uintptr_t *sp, int most)
{
    const uintptr_t from = (uintptr_t)sp;
    uintptr_t top = stack_top_of_this_thread();
    if (top <= from || top - from > (1u << 20))   /* not noted, or another stack */
        top = (from | 0xfff) + 1;
    const uintptr_t words = (top - from) / sizeof(uintptr_t);
    return words < (uintptr_t)most ? (int)words : most;
}

/* The boot guard's mark (core/boot_guard): a crash while config.json is
 * new counts against it. Plain open() and write(). */
void leave_crash_mark()
{
    const char *text = boot_guard::crash_mark_text();
    if (!text[0] || !boot_guard::crash_mark_path()[0])
        return;
    const int fd = open(boot_guard::crash_mark_path(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        (void)!write(fd, text, std::strlen(text));
        close(fd);
    }
}

/* What looks like return addresses on the stack, from sp up. */
void report_stack(const uintptr_t *sp)
{
    const uintptr_t anchor = (uintptr_t)&ps5::log;
    int found = 0;
    const int words = words_to_read(sp, 512);
    for (int i = 0; i < words && found < 24; i++) {
        const uintptr_t v = sp[i];
        if (v > anchor - (64u << 20) && v < anchor + (64u << 20)) {
            emergency("[regie5] stack +%d 0x%lx", i * 8, (unsigned long)v);
            found++;
        }
    }
}

/* Ends the title the way the system expects (exit() would end in its crash reporter). */
[[noreturn]] void end_title()
{
    sceSystemServiceLoadExec("exit", nullptr);
    for (;;)
        sceKernelUsleep(100000);
}

/* Crash report: plain buffers, write() and sendto(), nothing that allocates. */
void on_crash(int sig, siginfo_t *info, void *context)
{
    char line[256];
    const uintptr_t fault = info ? (uintptr_t)info->si_addr : 0;
    /* The console's ucontext_t seems not to match the SDK's header (probe-4
     * read a rip below the program's load address): for a bad instruction the
     * faulting address is the instruction's, otherwise the header's rip, for
     * what it is worth. */
    const ucontext_t *uc = static_cast<const ucontext_t *>(context);
    const bool at_instruction = sig == SIGILL || sig == SIGFPE || sig == SIGTRAP;
    const uintptr_t rip = at_instruction ? fault : uc ? (uintptr_t)uc->uc_mcontext.mc_rip : 0;
    /* A fixed point of the program, to find the faulting code from the linked
     * file (llvm-pie.elf, kept beside each build): there, ps5::log is at its
     * own address minus the load address. */
    const uintptr_t main_at = (uintptr_t)&ps5::log;
    int n = std::snprintf(line, sizeof line,
                          "[regie5] CRASH signal %d code %d fault address 0x%lx rip 0x%lx (ps5::log at 0x%lx)\n", sig,
                          info ? info->si_code : 0, (unsigned long)fault, (unsigned long)rip,
                          (unsigned long)main_at);
    if (n > 0) {
        send_raw(line, (size_t)n);
        (void)!write(2, line, (size_t)n);
    }
    leave_crash_mark();
    /* Words that point near the code, from this handler's frame upwards: the
     * signal frame (with the real rip, wherever the console keeps it), then
     * the stack of the code that crashed, so its likely return addresses. */
    const uintptr_t *sp = static_cast<const uintptr_t *>(__builtin_frame_address(0));
    int found = 0;
    const int words = words_to_read(sp, 1536);
    for (int i = 0; i < words && found < 32; i++) {
        const uintptr_t v = sp[i];
        if (v > main_at - (64u << 20) && v < main_at + (64u << 20)) {
            n = std::snprintf(line, sizeof line, "[regie5] CRASH stack +%d 0x%lx\n", i * 8, (unsigned long)v);
            if (n > 0) {
                send_raw(line, (size_t)n);
                (void)!write(2, line, (size_t)n);
            }
            found++;
        }
    }
    /* Then as before: the system's handling of the crash. */
    signal(sig, SIG_DFL);
    raise(sig);
}

} // namespace

void log_init(const char *host, int port)
{
    if (!host || !*host || s_fd >= 0)
        return;
    std::memset(&s_addr, 0, sizeof s_addr);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &s_addr.sin_addr) != 1)
        return;
    s_fd = socket(AF_INET, SOCK_DGRAM, 0);
}

void note_thread_stack()
{
    const int i = s_top_count.fetch_add(1);
    if (i >= (int)(sizeof s_tops / sizeof s_tops[0]))
        return;   /* a thread past the table: its report stops at the page's end */
    s_tops[i].top.store((uintptr_t)__builtin_frame_address(0));
    s_tops[i].thread.store(pthread_self());
}

int log_socket() { return s_fd; }
const void *log_address() { return &s_addr; }
unsigned log_address_size() { return sizeof s_addr; }

void log(const char *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(line, sizeof line - 1, format, args);
    va_end(args);
    if (n < 0)
        return;
    const size_t used = std::strlen(line);
    line[used] = '\n';
    line[used + 1] = '\0';
    send_raw(line, used + 1);
    std::fputs(line, stderr);
    sceKernelDebugOutText(0, line);
}

void install_crash_handler()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    int installed = 0;
    static const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGSYS, SIGTRAP};
    for (int sig : kSignals)
        installed += sigaction(sig, &sa, nullptr) == 0;
    log("[regie5] crash handler: %d of 7 signals", installed);
}

void sleep_us(uint32_t microseconds) { sceKernelUsleep(microseconds); }

bool hide_splash_screen()
{
    /* The runtime holds earlier requests back (runtime_shims.c): this one counts. */
    if (regie5_release_splash)
        regie5_release_splash();
    return sceSystemServiceHideSplashScreen() == 0;
}

void park()
{
    std::fflush(nullptr);
    for (;;)
        sceKernelUsleep(100000);
}

void quit()
{
    log("[regie5] quit requested");
    std::fflush(nullptr);
    const int refused = sceSystemServiceLoadExec("exit", nullptr);
    log("[regie5] quit refused rc=0x%x", (unsigned)refused);
    park();
}

} // namespace ps5

namespace ps5 {

void log_heap(const char *where)
{
    size_t size = 0, live = 0, peak = 0, blocks = 0, failures = 0;
    regie5_heap_stats(&size, &live, &peak, &blocks, &failures);
    if (size == 0) {
        log("[regie5] heap %s: NOT UP (the system's malloc is in use)", where);
        return;
    }
    log("[regie5] heap %s: %.1f MiB used, %.1f MiB free, peak %.1f MiB, %zu blocks, %zu failed", where,
        live / 1048576.0, (size - live) / 1048576.0, peak / 1048576.0, blocks, failures);
}

size_t heap_size()
{
    size_t size = 0, live = 0, peak = 0, blocks = 0, failures = 0;
    regie5_heap_stats(&size, &live, &peak, &blocks, &failures);
    return size;
}

bool log_heap_reservation()
{
    size_t sizes[8];
    int results[8];
    const int n = regie5_heap_attempts(sizes, results, 8);
    for (int i = 0; i < n; i++) {
        if (results[i] == 0)
            log("[regie5] heap: %zu MiB granted", sizes[i] >> 20);
        else if (results[i] < 0)
            log("[regie5] heap: %zu MiB refused (sceLibcMspaceCreate)", sizes[i] >> 20);
        else
            log("[regie5] heap: %zu MiB refused (mmap errno %d)", sizes[i] >> 20, results[i]);
    }
    return regie5_heap_state() == 2;
}

} // namespace ps5

/* The ways a title can stop without a signal, reported before they happen.
 * Defined here, they win over the libc's for calls from the title's code. */
extern "C" {

void regie5_alloc_failed(size_t asked, size_t live_bytes, int system)
{
    static int reported;   /* a few lines, not a flood */
    if (reported >= 20)
        return;
    reported++;
    if (system)
        ps5::emergency("[regie5] ALLOCATION FAILED: %zu bytes asked of the system's malloc (the heap is not up)",
                       asked);
    else
        ps5::emergency("[regie5] ALLOCATION FAILED: %zu bytes asked, the heap gave nothing (%.1f MiB in use)", asked,
                       live_bytes / 1048576.0);
}

[[noreturn]] void abort(void)
{
    ps5::emergency("[regie5] ABORT called from 0x%lx (ps5::log at 0x%lx)",
                   (unsigned long)(uintptr_t)__builtin_return_address(0), (unsigned long)(uintptr_t)&ps5::log);
    ps5::leave_crash_mark();
    ps5::report_stack(static_cast<const uintptr_t *>(__builtin_frame_address(0)));
    ps5::end_title();
}

[[noreturn]] void exit(int status)
{
    ps5::emergency("[regie5] exit(%d) called from 0x%lx", status,
                   (unsigned long)(uintptr_t)__builtin_return_address(0));
    ps5::end_title();
}

[[noreturn]] void _Exit(int status)
{
    /* ps5-opengl stops this way after a GPU submission error. */
    ps5::emergency("[regie5] _Exit(%d) called from 0x%lx (the GPU runtime stops so after a GPU error)", status,
                   (unsigned long)(uintptr_t)__builtin_return_address(0));
    ps5::end_title();
}

} // extern "C"

/* libc++ without exceptions (a failed new, a bad index under hardening...)
 * ends here, with a message. */
_LIBCPP_BEGIN_NAMESPACE_STD
void __libcpp_verbose_abort(const char *format, ...)
{
    char text[200];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ps5::emergency("[regie5] LIBC++ ABORT: %s", text);
    abort();
}
_LIBCPP_END_NAMESPACE_STD
