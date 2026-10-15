/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/requests.h"

#include "api/seerr.h"
#include "core/i18n.h"
#include "core/jelly5.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"
#include "ui/tabbar.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kBarY = 250;
constexpr float kListTop = 360, kRowH = 132, kRowGap = 14;
constexpr float kListW = gfx::W - 2 * kPad;

const i18n::S kFilterLabels[3] = {i18n::FilterPending, i18n::FilterApproved, i18n::FilterAll};
const api::Services::RequestFilter kFilterApi[3] = {api::Services::RequestFilter::Pending,
                                                     api::Services::RequestFilter::Approved,
                                                     api::Services::RequestFilter::All};

struct Look {
    i18n::S label;
    uint32_t color;
};

Look look(model::Request::Status s)
{
    switch (s) {
    case model::Request::Pending: return {i18n::ReqPending, kOrange};
    case model::Request::Approved: return {i18n::ReqApproved, 0xff0a84ffu};
    case model::Request::Declined: return {i18n::ReqDeclined, kRed};
    case model::Request::Failed: return {i18n::ReqFailed, kRed};
    case model::Request::Completed: return {i18n::ReqCompleted, kGreen};
    }
    return {i18n::ReqPending, kOrange};
}

std::string seasons_text(const std::vector<int> &seasons)
{
    if (seasons.size() == 1)
        return fmt::f(i18n::tr(i18n::SeasonOne), seasons[0]);
    std::string list;
    for (size_t i = 0; i < seasons.size(); i++)
        list += (i ? ", " : "") + std::to_string(seasons[i]);
    return fmt::f(i18n::tr(i18n::SeasonsList), list.c_str());
}

} // namespace

RequestsScreen::RequestsScreen(Context &ctx) : m_ctx(ctx) {}

void RequestsScreen::activate()
{
    if (m_view == Issues)
        m_issues.load(false);
    else if (m_view == Discover)
        m_discover.load(false);
    else if (m_pages[m_filter].state == load::Idle)
        load(m_filter);
}

void RequestsScreen::load(int filter)
{
    if (!m_ctx.services || m_pages[filter].loading())
        return;
    std::weak_ptr<bool> alive = m_alive;
    if (!m_pages[filter].ready())
        m_pages[filter].state = load::Loading;
    m_ctx.services->requests(kFilterApi[filter], [this, alive, filter](api::Result<model::RequestPage> r) {
        if (alive.expired())
            return;
        m_pages[filter].take(std::move(r), platform::now());
        if (kFilterApi[filter] == api::Services::RequestFilter::Pending)   /* the tab's counter */
            m_ctx.badges[TabRequests] =
                m_pages[filter].ready() ? badges::requests(m_pages[filter].value) : badges::Badge{};
        if (filter == m_filter)
            m_focus = std::min(m_focus, std::max(0, (int)m_pages[filter].value.requests.size() - 1));
    });
}

void RequestsScreen::act(const model::Request &r)
{
    if (r.status != model::Request::Pending) {
        notify(i18n::tr(i18n::NotPending), false, platform::now());
        return;
    }
    const model::Request req = r;
    const std::string body = fmt::f(i18n::tr(i18n::RequestedBy), r.requested_by.c_str());
    m_sheet.open(title_of(r), body, {i18n::tr(i18n::Approve), i18n::tr(i18n::Decline), i18n::tr(i18n::Cancel)}, 2,
                 [this, req](int choice) {
                     if (choice == 0 || choice == 1)
                         confirm(req, choice == 0);
                 });
}

void RequestsScreen::confirm(const model::Request &r, bool approve)
{
    const model::Request req = r;
    const std::string body = title_of(r) + " · " + fmt::f(i18n::tr(i18n::RequestedBy), r.requested_by.c_str());
    m_sheet.open(i18n::tr(approve ? i18n::ConfirmApprove : i18n::ConfirmDecline), body,
                 {i18n::tr(i18n::Cancel), i18n::tr(approve ? i18n::Approve : i18n::Decline)}, 0,
                 [this, req, approve](int choice) {
                     if (choice != 1 || m_busy || !m_ctx.services)
                         return;
                     m_busy = true;
                     std::weak_ptr<bool> alive = m_alive;
                     auto done = [this, alive, approve](api::Result<bool> res) {
                         if (alive.expired())
                             return;
                         m_busy = false;
                         const double now = platform::now();
                         if (res.ok())
                             notify(i18n::tr(approve ? i18n::DoneApproved : i18n::DoneDeclined), true, now);
                         else
                             notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(res.error, res.http_status).c_str()),
                                    false, now);
                         for (int f = 0; f < kFilters; f++)   /* every list may have changed */
                             if (m_pages[f].state != load::Idle)
                                 load(f);
                     };
                     if (approve)
                         m_ctx.services->approve(req.id, done);
                     else
                         m_ctx.services->decline(req.id, done);
                 },
                 !approve);
}

const model::Request *RequestsScreen::focused() const
{
    if (m_bar || m_view != Requests || !m_pages[m_filter].ready())
        return nullptr;
    const auto &reqs = m_pages[m_filter].value.requests;
    return m_focus < (int)reqs.size() ? &reqs[m_focus] : nullptr;
}

std::vector<RequestsScreen::MenuItem> RequestsScreen::menu(const model::Request &r) const
{
    std::vector<MenuItem> items;
    if (r.status == model::Request::Pending)
        items.insert(items.end(), {MenuItem::Approve, MenuItem::Decline});
    if (m_ctx.jelly5.available(m_ctx.services) && !jelly5::item_for(r).empty())
        items.push_back(MenuItem::Jelly5);
    return items;
}

void RequestsScreen::open_menu(const model::Request &r)
{
    const std::vector<MenuItem> items = menu(r);
    if (items.empty())
        return;
    std::vector<std::string> labels;
    for (MenuItem m : items)
        labels.push_back(m == MenuItem::Approve   ? i18n::tr(i18n::Approve)
                         : m == MenuItem::Decline ? i18n::tr(i18n::Decline)
                                                  : m_ctx.jelly5.label(m_ctx.services));
    labels.push_back(i18n::tr(i18n::Cancel));
    const model::Request req = r;
    const std::string title = title_of(r);
    m_sheet.open(title, fmt::f(i18n::tr(i18n::RequestedBy), r.requested_by.c_str()), labels, (int)items.size(),
                 [this, req, items, title](int c) {
                     if (c < 0 || c >= (int)items.size())
                         return;
                     if (items[c] == MenuItem::Jelly5)
                         m_ctx.jelly5.open(m_ctx.services, jelly5::item_for(req), title);
                     else
                         confirm(req, items[c] == MenuItem::Approve);
                 });
}

std::vector<PadHint> RequestsScreen::hints() const
{
    if (m_view == Issues)
        return m_issues.hints(m_focused && !m_bar);
    if (m_view == Discover)
        return m_discover.hints(m_focused && !m_bar);
    const model::Request *r = focused();
    if (!m_focused || !r || menu(*r).empty())
        return {};
    return {{PadButton::Options, i18n::tr(i18n::HintOptions)}};
}

Action RequestsScreen::input(const input::State &in)
{
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    if (m_issues.modal()) {
        m_issues.input(in);
        return Action::None;
    }
    if (m_discover.modal()) {
        m_discover.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        art::retry_failed();
        if (m_view == Issues)
            m_issues.load(true);
        else if (m_view == Discover)
            m_discover.load(true);
        else
            load(m_filter);
        return Action::None;
    }
    if (m_bar) {   /* the views, then the view's filters; ✕ chooses */
        const int slots = ViewCount + filter_count();
        if (p & input::Up)
            return Action::ToTabs;
        if (p & (input::Left | input::Right)) {
            const int f = m_bar_focus + ((p & input::Left) ? -1 : 1);
            if (f < 0 || f >= slots)
                return Action::Bump;
            m_bar_focus = f;
        } else if (p & input::Cross) {
            if (m_bar_focus < ViewCount) {
                if (m_bar_focus != m_view) {
                    m_view = (View)m_bar_focus;
                    activate();
                } else {
                    m_bar = false;
                }
            } else if (m_view == Issues) {
                m_issues.set_filter(m_bar_focus - ViewCount);
            } else {
                const int f = m_bar_focus - ViewCount;
                m_filter = f;
                m_focus = 0;
                m_scroll.snap(0);
                if (m_pages[f].state == load::Idle || platform::now() - m_pages[f].at > 30)
                    load(f);
            }
        } else if (p & input::Down) {
            m_bar = false;
            if (m_view == Issues)
                m_issues.enter();
            else if (m_view == Discover)
                m_discover.enter();
            else
                m_focus = 0;
        }
        return Action::None;
    }
    if (m_view == Issues) {
        if (m_issues.input(in) == Action::ToTabs)
            m_bar = true, m_bar_focus = ViewCount + m_issues.filter();
        return Action::None;
    }
    if (m_view == Discover) {
        const Action a = m_discover.input(in);
        if (a == Action::ToTabs)
            m_bar = true, m_bar_focus = Discover;
        return a == Action::Bump ? Action::Bump : Action::None;
    }
    const auto &reqs = m_pages[m_filter].value.requests;
    const int n = m_pages[m_filter].ready() ? (int)reqs.size() : 0;
    if (p & input::Up) {
        if (m_focus <= 0) {
            m_bar = true, m_bar_focus = ViewCount + m_filter;
            return Action::None;
        }
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    } else if ((p & input::Cross) && m_focus < n) {
        act(reqs[m_focus]);
    } else if ((p & input::Options) && m_focus < n) {
        open_menu(reqs[m_focus]);
    }
    return Action::None;
}

bool RequestsScreen::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (!m_focused || m_bar)
        return false;
    if (m_view == Issues)
        return m_issues.backdrop(key, ref, seed);
    if (m_view == Discover)
        return m_discover.backdrop(key, ref, seed);
    const model::Request *r = focused();
    return r && m_titles.backdrop(r->tv, r->tmdb_id, key, ref, seed);
}

void RequestsScreen::draw(double now, float dt, float ox, bool *animating)
{
    const float x0 = kPad + ox;
    gfx::text(x0, 214, tab_label(TabRequests), {gfx::Bold, 60}, kText);

    /* The views on the left, the view's filters on the right. */
    const std::vector<std::string> views = {i18n::tr(i18n::ViewRequests), i18n::tr(i18n::ViewIssues),
                                            i18n::tr(i18n::ViewDiscover)};
    if (m_discover.take_requested())   /* a new request: the lists are stale */
        for (int f = 0; f < kFilters; f++)
            if (m_pages[f].state != load::Idle)
                load(f);
    const bool on_bar = m_focused && m_bar;
    draw_pills(x0, kBarY, views, m_view, on_bar && m_bar_focus < ViewCount ? m_bar_focus : m_view,
               on_bar && m_bar_focus < ViewCount, m_view_drop, dt, animating);
    std::vector<std::string> filters;
    if (m_view == Issues)
        filters = m_issues.filter_labels();
    else if (m_view == Requests)
        for (i18n::S s : kFilterLabels)
            filters.push_back(i18n::tr(s));
    const int chosen = m_view == Issues ? m_issues.filter() : m_filter;
    const bool on_filters = on_bar && m_bar_focus >= ViewCount;
    if (!filters.empty())
        draw_pills(x0 + kListW, kBarY, filters, chosen, on_filters ? m_bar_focus - ViewCount : chosen, on_filters,
                   m_filter_drop, dt, animating, true);

    if (m_view == Discover) {
        m_drop.hide();
        m_discover.draw(now, x0, kListTop, m_focused && !m_bar, dt, animating);
        return;
    }

    if (m_view == Issues) {
        m_drop.hide();
        m_issues.draw(now, x0, kListTop, m_focused && !m_bar, dt, animating);
        return;
    }
    draw_requests(now, x0, dt, animating);
}

void RequestsScreen::draw_requests(double now, float x0, float dt, bool *animating)
{
    if (m_pages[m_filter].state == load::Idle)
        load(m_filter);
    const Load<model::RequestPage> &page = m_pages[m_filter];
    const gfx::Rect area{x0, kListTop, kListW, 300};
    if (!page.ready() || page.value.requests.empty()) {
        m_drop.hide();
        if (page.state == load::Off)
            draw_empty(area, m_ctx.off_text(i18n::SeerrOff));
        else if (page.state == load::Failed)
            draw_empty(area, fmt::labelled("Seerr", fmt::error(page.error, page.http_status)), i18n::tr(i18n::Retry),
                       true);
        else if (!page.ready())
            for (int i = 0; i < 4; i++)
                draw_skeleton({x0, kListTop + i * (kRowH + kRowGap), kListW, kRowH}, 24, now, animating);
        else
            draw_empty(area, i18n::tr(m_filter == 0   ? i18n::EmptyPending
                                      : m_filter == 1 ? i18n::EmptyApproved
                                                      : i18n::EmptyRequests));
        return;
    }

    const auto &reqs = page.value.requests;
    const int n = (int)reqs.size();
    m_focus = std::clamp(m_focus, 0, n - 1);
    const float list_h = n * (kRowH + kRowGap);
    const float scroll = list_scroll(m_scroll, m_focus * (kRowH + kRowGap), kRowH,
                                     gfx::H - 120 - kListTop, list_h, dt, animating);
    gfx::push_scissor({0, kListTop - 20, gfx::W, gfx::H - 110 - kListTop + 20});
    for (int i = 0; i < n; i++) {
        const float y = kListTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH >= kListTop - 20 && y <= gfx::H)
            glass_flat({x0, y, kListW, kRowH}, 24, 0.9f);
    }
    if (m_focused && !m_bar) {
        m_drop.to({x0, kListTop + m_focus * (kRowH + kRowGap) - scroll, kListW, kRowH}, m_focus, 0, -scroll);
        m_drop.draw(dt, 1.f, animating, 24);
    } else {
        m_drop.hide();
        m_drop.draw(dt, 1.f, animating);
    }
    const time_t now_t = time(nullptr);
    for (int i = 0; i < n; i++) {
        const model::Request &r = reqs[i];
        const float y = kListTop + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH < kListTop - 20 - 3 * kRowH || y > gfx::H + 3 * kRowH)
            continue;   /* a few beyond the edges still fetch their details */
        const model::MediaDetail *d = m_titles.get(r.tv, r.tmdb_id);
        if (y + kRowH < kListTop - 20 || y > gfx::H)
            continue;
        const bool f = m_focused && !m_bar && i == m_focus;
        const std::string title = title_of(r);
        const art::Ref poster = d ? art::Ref{config::Seerr, seerr::image_path(d->poster_path, "w185")} : art::Ref{};
        if (d || m_titles.failed(r.tv, r.tmdb_id))
            art::draw_poster({x0 + 18, y + 12, 72, 108}, poster, title, seed_of(title), 10, 1.f, 160);
        else
            draw_skeleton({x0 + 18, y + 12, 72, 108}, 10, now, animating);

        const Look lk = look(r.status);
        const std::string label = i18n::tr(lk.label);
        const float right = x0 + kListW - 28;
        const float chip_w = chip_width(label, 22);
        draw_chip(right - chip_w, y + 18, label, lk.color, 1.f, 22);
        if (r.is4k)
            draw_chip(right - chip_w - 14 - chip_width("4K", 22), y + 18, "4K", 0xffbf5af2u, 1.f, 22);

        const float tx = x0 + 116, text_w = kListW - 116 - chip_w - 160;
        gfx::text(tx, y + 50, title + (d && d->year > 0 ? " (" + std::to_string(d->year) + ")" : ""),
                  {f ? gfx::Bold : gfx::SemiBold, 32, text_w}, kText);
        std::string kind = i18n::tr(r.tv ? i18n::KindSeries : i18n::KindMovie);
        if (r.tv && !r.seasons.empty())
            kind += " · " + seasons_text(r.seasons);
        gfx::text(tx, y + 86, kind, {gfx::Regular, 24, text_w}, kText2);
        std::string who = fmt::f(i18n::tr(i18n::RequestedBy), r.requested_by.c_str());
        if (r.created)
            who += " · " + fmt::ago(r.created, now_t);
        gfx::text(tx, y + 118, who, {gfx::Regular, 22, text_w}, kText3);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
