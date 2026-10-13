/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Demandes (Seerr): pending, approved or all requests, with the title's
 * poster (through Seerr's image proxy), who asked, when, film or series and
 * which seasons. ✕ on a pending one offers Approve or Decline, then asks again
 * before doing it.
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/widgets.h"

#include <map>
#include <memory>
#include <string>

namespace ui {

class RequestsScreen : public Screen {
public:
    explicit RequestsScreen(Context &ctx);

    void activate() override;
    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override { m_focus = -1; }
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_sheet.is_open(); }
    void draw_overlay(float dt, bool *animating) override { m_sheet.draw(dt, animating); }
    std::vector<PadHint> hints() const override;

private:
    void load(int filter);
    const model::MediaDetail *detail(const model::Request &r);
    std::string title_of(const model::Request &r);
    void act(const model::Request &r);
    void confirm(const model::Request &r, bool approve);
    /* Options: what can be done with the request with the focus. */
    enum class MenuItem { Approve, Decline, Jelly5 };
    const model::Request *focused() const;
    std::vector<MenuItem> menu(const model::Request &r) const;
    void open_menu(const model::Request &r);

    static constexpr int kFilters = 3;   /* pending, approved, all */
    Context &m_ctx;
    Load<model::RequestPage> m_pages[kFilters];
    std::map<std::string, Load<model::MediaDetail>> m_details;
    int m_filter = 0;
    int m_focus = -1;   /* -1: the filter bar */
    bool m_busy = false;   /* an action on its way */
    Anim m_scroll;
    Drop m_drop, m_bar_drop;
    Sheet m_sheet;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
