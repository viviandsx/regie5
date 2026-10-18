/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the tests' renderer counts (tests/gfx_stub.cpp).
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace gfx_stub {
extern uint64_t frames;
extern int made, live;   /* textures made, and still alive */
extern size_t live_bytes;
} // namespace gfx_stub
