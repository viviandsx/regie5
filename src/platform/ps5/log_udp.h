/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Development builds: what other code writes to stderr (the frame report,
 * the mock transport, a library's messages) lands only in the log file. A
 * thread follows that file and sends its complete lines over UDP too, except
 * those of ps5::log, which went out at once.
 */
#pragma once

namespace ps5 {

/* Needs ps5::log_init first; does nothing without a UDP log. */
void start_udp_mirror(const char *log_path);

} // namespace ps5
