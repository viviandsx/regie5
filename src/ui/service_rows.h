/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The services' connection, a row per instance: a dot (green, red, orange
 * for a version too old), its name and address, and in words how the test
 * went. Settings and the welcome screens show the same rows.
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"

#include <memory>
#include <vector>

namespace ui {

class ServiceRows {
public:
    /* A row per instance of each service (config.json may list several). */
    void build(const Context &ctx);
    /* Tests one row, or every row (the services that are off say so at once). */
    void test(Context &ctx, int row);
    void test_all(Context &ctx);
    int size() const { return (int)m_rows.size(); }
    /* Every row has an answer (none still testing). */
    bool settled() const;

    /* The row height for this many rows in a pane: compact past five. */
    static float row_height(int n);
    /* The rows inside a pane whose inner area starts at (x, y), w wide; the
     * pane itself and the focus drop are the caller's (focus: the row it is
     * on, -1 none). */
    void draw(const Context &ctx, float x, float y, float w, int focus, double now, bool *animating) const;

private:
    struct Row {
        config::Service service;
        int instance;
        model::ServiceStatus status;
        unsigned gen = 0;   /* a newer test wins over an older answer */
    };
    std::vector<Row> m_rows;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
