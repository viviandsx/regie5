/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Glances (optional): the server's load, read-only, from its REST API v4.
 * https://glances.readthedocs.io/en/latest/api/restful.html
 */
#pragma once

#include "core/model.h"

#include <string>

namespace glances {

/* GET: CPU, memory, swap and load in one answer (percentages). */
std::string quicklook_path();
bool parse_quicklook(const std::string &body, model::ServerLoad *out);
/* The connection test: the CPU's name stands for a version. */
bool parse_test(const std::string &body, std::string *what);

} // namespace glances
