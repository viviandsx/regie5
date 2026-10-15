/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Demandes (Seerr), three views in pills like Activité: the requests (pending,
 * approved or all, with the title's poster through Seerr's image proxy, who
 * asked, when, film or series and which seasons; ✕ on a pending one offers
 * Approve or Decline, then asks again before doing it), the issues
 * (IssuesView) and Discover (DiscoverView: search, rows of titles, new
 * requests). Each view's filters on the right of the same row.
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/screens/discover.h"
#include "ui/screens/issues.h"
#include "ui/seerr_titles.h"
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
    void enter_from_top() override
    {
        m_bar = true;
        m_bar_focus = m_view;
    }
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_sheet.is_open() || m_issues.modal() || m_discover.modal(); }
    void draw_overlay(float dt, bool *animating) override
    {
        m_sheet.draw(dt, animating);
        m_issues.draw_overlay(dt, animating);
        m_discover.draw_overlay(dt, animating);
    }
    std::vector<PadHint> hints() const override;

    enum View { Requests = 0, Issues, Discover, ViewCount };

private:
    void load(int filter);
    std::string title_of(const model::Request &r) { return m_titles.title(r.tv, r.tmdb_id); }
    void draw_requests(double now, float x0, float dt, bool *animating);
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
    SeerrTitles m_titles{m_ctx};
    IssuesView m_issues{m_ctx, m_titles};
    DiscoverView m_discover{m_ctx};
    /* The pills' slots: the views, then the view's filters (Discover has none). */
    int filter_count() const { return m_view == Discover ? 0 : kFilters; }
    int m_filter = 0;
    int m_focus = 0;
    View m_view = Requests;
    bool m_bar = true;     /* the focus is on the row of pills */
    int m_bar_focus = 0;   /* the views, then the view's filters */
    bool m_busy = false;   /* an action on its way */
    Anim m_scroll;
    Drop m_drop, m_view_drop, m_filter_drop;
    Sheet m_sheet;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
