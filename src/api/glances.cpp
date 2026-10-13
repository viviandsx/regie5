/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/glances.h"

#include "api/json.h"

namespace glances {

std::string quicklook_path() { return "/api/4/quicklook"; }

bool parse_quicklook(const std::string &body, model::ServerLoad *out)
{
    json::Doc d(body);
    const cJSON *r = d.root();
    if (!cJSON_IsObject(r) || !cJSON_IsNumber(json::get(r, "cpu")))
        return false;
    out->cpu = json::num(r, "cpu");
    out->mem = json::num(r, "mem");
    out->swap = json::num(r, "swap");
    out->load = json::num(r, "load");
    out->cores = json::integer(r, "cpu_log_core");
    out->cpu_name = json::str(r, "cpu_name");
    return true;
}

bool parse_test(const std::string &body, std::string *what)
{
    model::ServerLoad l;
    if (!parse_quicklook(body, &l))
        return false;
    *what = l.cpu_name;
    return true;
}

} // namespace glances
