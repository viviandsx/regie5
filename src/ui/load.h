/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Where a screen's piece of data stands: not asked yet, on its way, there, or
 * failed (and why).
 */
#pragma once

#include "api/services.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace load {
enum State { Idle, Loading, Ready, Failed, Off };   /* Off: the service is turned off */
}

template <typename T> struct Load {
    load::State state = load::Idle;
    T value{};
    api::Error error = api::Error::None;
    int http_status = 0;
    double at = 0;   /* when it last finished */

    bool loading() const { return state == load::Loading; }
    bool ready() const { return state == load::Ready; }
    /* Takes a result. A failed refresh shows the failure (the old value stays, unused). */
    void take(api::Result<T> r, double now)
    {
        at = now;
        if (r.error == api::Error::Disabled) {
            state = load::Off;
            return;
        }
        error = r.error;
        http_status = r.http_status;
        if (r.ok()) {
            value = std::move(r.value);
            state = load::Ready;
        } else {
            state = load::Failed;
        }
    }
};

/* One instance's share of a piece of data (config.json may list several
 * Sonarr / Radarr). */
template <typename T> struct Part {
    config::Service service = config::Sonarr;
    int instance = 0;
    Load<T> load;
};

/* Several instances' answers seen as one: ready as soon as one is (the ready
 * ones joined into it by join(&value, part)), loading while one is still on
 * its way, failed when every one that is on failed (with the first failure),
 * off when all are off. The parts that failed beside ready ones are said
 * apart (a red chip): see failed_parts. */
template <typename T, typename Join> void merge_parts(const std::vector<Part<T>> &parts, Load<T> *out, Join join)
{
    Load<T> m;
    bool ready = false, waiting = false;
    const Load<T> *failed = nullptr;
    for (const auto &p : parts) {
        m.at = std::max(m.at, p.load.at);
        if (p.load.ready()) {
            join(&m.value, p.load.value);
            ready = true;
        } else if (p.load.state == load::Loading || p.load.state == load::Idle) {
            waiting = true;
        } else if (p.load.state == load::Failed && !failed) {
            failed = &p.load;
        }
    }
    m.state = ready ? load::Ready : waiting ? load::Loading : failed ? load::Failed : load::Off;
    if (!ready && failed)
        m.error = failed->error, m.http_status = failed->http_status;
    *out = std::move(m);
}

/* Vectors: one after the other. */
template <typename T> void merge_parts(const std::vector<Part<std::vector<T>>> &parts, Load<std::vector<T>> *out)
{
    merge_parts(parts, out, [](std::vector<T> *v, const std::vector<T> &p) { v->insert(v->end(), p.begin(), p.end()); });
}

} // namespace ui
