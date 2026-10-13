/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Réglages: each service's connection (a green or red dot and, when it fails,
 * why, in words), a test per service and for all, the app's version and where
 * its configuration file is.
 */
#pragma once

#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/screen.h"
#include "ui/widgets.h"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(Context &ctx);
    ~SettingsScreen() override { close_share(); }
    void activate() override;

    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override { m_focus = 0; }
    bool modal() const override { return m_sheet.is_open() || m_share_open; }
    void draw_overlay(float dt, bool *animating) override
    {
        m_sheet.draw(dt, animating);
        draw_share(dt, animating);
    }

    /* Tests every service that is on (at start, and on △). */
    void test_all();
    void test(int row);

private:
    /* Reads config.json again ("test all", △). A file that fails keeps the
     * configuration in use and says why. */
    void reload_config();
    /* A row per instance of each service (config.json may list several). */
    void build_rows();
    void toggle_cleanup();
    /* Configuring from a phone: the QR code of a page on the local network,
     * served while this panel is open (platform.h, share_*). */
    void open_share();
    void close_share();
    void draw_share(float dt, bool *animating);
    int button_count() const;

    struct Row {
        config::Service service;
        int instance;
        model::ServiceStatus status;
        unsigned gen = 0;   /* a newer test wins over an older answer */
    };
    bool m_tested = false;   /* the tests ran once (on the first opening) */
    Context &m_ctx;
    std::vector<Row> m_rows;
    int m_focus = 0;     /* the rows, then the buttons' row */
    int m_button = 0;    /* on the buttons' row: 0 test all, 1 cleanup, 2 from a phone */
    bool m_share_open = false;
    std::string m_share_error;
    int m_share_saves = 0;            /* configurations the phone saved, seen */
    std::vector<bool> m_qr;           /* the QR code's modules, row by row */
    int m_qr_size = 0;
    Anim m_share_show;
    Drop m_drop;
    Sheet m_sheet;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
