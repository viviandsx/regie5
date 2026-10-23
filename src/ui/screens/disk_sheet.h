/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A disk's sheet (Scrutiny, read-only): its state as Scrutiny's page says it,
 * its temperature against Régie5's thresholds and the last 7 days', how long
 * it has run, when it was last read, and its SMART attributes that are not
 * fine, named by the app. Nothing to do from here: a Close button.
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/drop.h"
#include "ui/load.h"

#include <string>

namespace ui {

class DiskSheet {
public:
    void open(const std::string &key);
    void close() { m_open = false; }
    bool is_open() const { return m_open; }
    const std::string &key() const { return m_key; }
    /* ○ or ✕: closed. */
    void input(const input::State &in);
    /* d: the disk (from the last summary); threshold: Scrutiny's settings';
     * detail: its last week, on its way or there. */
    void draw(const model::Disk &d, int threshold, const Load<model::DiskDetail> &detail, double now, float dt,
              bool *animating);

private:
    bool m_open = false;
    std::string m_key;
    Anim m_show;
    Drop m_drop;
};

} // namespace ui
