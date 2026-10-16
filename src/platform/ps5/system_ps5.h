/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PS5 system services the shell uses: the log, sleeping, the splash picture
 * and closing the title. After ps5-homebrew-ui's platform/ps5/system.cpp
 * (Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later).
 *
 * log() writes a line to stderr (which the runtime, runtime/runtime_shims.c,
 * sends unbuffered to /download0/regie5/dev/app.log), to the kernel log and,
 * in development builds, at once over UDP: a line logged just before a crash
 * still reaches the laptop.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace ps5 {

/* First thing in main: where the UDP log goes (host empty: nowhere). */
void log_init(const char *host, int port);
void log(const char *format, ...) __attribute__((format(printf, 1, 2)));
/* The runtime heap's use, as a log line ("heap <where>: ..."). Abort,
 * exit, _Exit, libc++'s own aborts and failed allocations are reported too. */
void log_heap(const char *where);
/* The heap reservation: each size tried, logged; true if one was granted. */
bool log_heap_reservation();
/* The heap's size (0: not up). */
size_t heap_size();
/* A crash (a signal) is reported over UDP and to the log, with the faulting
 * address and what looks like return addresses on the stack (read from
 * the handler's own frame upwards, whatever the console's ucontext layout), then the
 * system's own crash handling runs as before. */
void install_crash_handler();
/* First thing in a thread (main, the HTTP workers): how far up its stack a
 * crash report may read. Elsewhere it stops at the end of the page. */
[[gnu::noinline]] void note_thread_stack();

/* Which launch functions the console binds (Open in Jelly5), as a log line. */
void log_launch_support();
/* Where random_bytes found its randomness ("/dev/urandom", "the CPU (RDRAND)"). */
const char *random_source();

void sleep_us(uint32_t microseconds);
/* The console's start picture goes once the first frame is up. */
bool hide_splash_screen();
/* A title never returns from main nor calls exit(): it asks the system to
 * close it, and waits. */
[[noreturn]] void quit();
[[noreturn]] void park();

/* For log_udp.cpp: the UDP log's socket and address (fd < 0: none). */
int log_socket();
const void *log_address();
unsigned log_address_size();

} // namespace ps5
