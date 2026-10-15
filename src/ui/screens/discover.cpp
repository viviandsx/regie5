/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/discover.h"

#include "api/seerr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kListW = gfx::W - 2 * kPad;
constexpr float kFieldH = 76;
constexpr float kCardW = 160, kCardH = 240, kCardGap = 26;
constexpr float kRowH = 372;            /* a row: its label, the cards, two lines under them */
constexpr float kRowsTop = kFieldH + 70;   /* the first row's label, below the field */
constexpr float kButtonH = 64, kBarPad = 6;
const gfx::TextStyle kButtonText{gfx::SemiBold, 28}, kButtonOn{gfx::Bold, 28};

const seerr::Shelf kShelfOf[5] = {seerr::Shelf::Trending, seerr::Shelf::PopularMovies, seerr::Shelf::PopularTv,
                                  seerr::Shelf::UpcomingMovies, seerr::Shelf::UpcomingTv};
const i18n::S kShelfLabel[5] = {i18n::ShelfTrending, i18n::ShelfPopularMovies, i18n::ShelfPopularTv,
                                i18n::ShelfUpcomingMovies, i18n::ShelfUpcomingTv};

/* Never asked for, or deleted since: it may be asked for (again). */
bool requestable(int status)
{
    return status == model::SeerrNone || status == model::SeerrUnknown || status == model::SeerrDeleted;
}

struct Look {
    i18n::S label;
    uint32_t color;
};

/* Where a title stands, as a chip; false when there is nothing to say. */
bool look(int status, Look *out)
{
    switch (status) {
    case model::SeerrPending: *out = {i18n::SeerrStPending, kOrange}; return true;
    case model::SeerrProcessing: *out = {i18n::SeerrStProcessing, 0xff0a84ffu}; return true;
    case model::SeerrPartial: *out = {i18n::SeerrStPartial, 0xff64d2ffu}; return true;
    case model::SeerrAvailable: *out = {i18n::SeerrStAvailable, kGreen}; return true;
    case model::SeerrBlocklisted: *out = {i18n::SeerrStBlocklisted, kRed}; return true;
    case model::SeerrDeleted: *out = {i18n::SeerrStDeleted, 0xff8e8e93u}; return true;
    default: return false;
    }
}

art::Ref poster_ref(const std::string &path, const char *size)
{
    return path.empty() ? art::Ref{} : art::Ref{config::Seerr, seerr::image_path(path, size)};
}

/* A box, ticked or not; dim when it cannot change. */
void checkbox(float x, float cy, bool on, bool enabled)
{
    const gfx::Rect b{x, cy - 15, 30, 30};
    if (on) {
        gfx::fill(b, enabled ? 0xff0a84ffu : 0x66ffffffu, 8);
        gfx::text(b.x + 15, b.y + 23, "✓", {gfx::Bold, 22}, 0xffffffffu, 1);
    } else {
        gfx::fill(b, enabled ? kText : kText3, 8);
        gfx::fill({b.x + 2.5f, b.y + 2.5f, 25, 25}, 0xff1c1c22u, 6.5f);
    }
}

} // namespace

/* ---- the rows ---- */

DiscoverView::~DiscoverView()
{
    if (m_typing)
        platform::text_entry_cancel();
}

void DiscoverView::load(bool force)
{
    if (!m_ctx.services)
        return;
    if (!m_shelves_set) {
        for (int i = 0; i < 5; i++)
            m_shelves[i].label = kShelfLabel[i];
        m_shelves_set = true;
    }
    std::weak_ptr<bool> alive = m_alive;
    for (int i = 0; i < 5; i++) {
        Row &row = m_shelves[i];
        if (row.titles.loading() || (!force && row.titles.state != load::Idle))
            continue;
        if (!row.titles.ready())
            row.titles.state = load::Loading;
        m_ctx.services->discover(kShelfOf[i], [this, alive, i](api::Result<std::vector<model::SeerrTitle>> r) {
            if (alive.expired())
                return;
            m_shelves[i].titles.take(std::move(r), platform::now());
        });
    }
    if (force && !m_query.empty())
        search(m_query);
    if (m_public.state == load::Idle || force) {
        m_public.state = load::Loading;
        m_ctx.services->seerr_public([this, alive](api::Result<model::SeerrPublic> r) {
            if (!alive.expired())
                m_public.take(std::move(r), platform::now());
        });
    }
}

void DiscoverView::search(const std::string &query)
{
    m_query = query;
    m_results.label = i18n::SearchResults;
    m_results.focus = 0;
    m_results.scroll.snap(0);
    if (query.empty() || !m_ctx.services) {
        m_results.titles = {};
        return;
    }
    m_results.titles.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->search(query, [this, alive, query](api::Result<std::vector<model::SeerrTitle>> r) {
        if (alive.expired() || query != m_query)
            return;   /* another search since */
        m_results.titles.take(std::move(r), platform::now());
    });
}

std::vector<DiscoverView::Row *> DiscoverView::rows()
{
    std::vector<Row *> out;
    if (!m_query.empty())
        out.push_back(&m_results);
    for (Row &r : m_shelves)
        if (r.titles.state != load::Off)
            out.push_back(&r);
    return out;
}

const model::SeerrTitle *DiscoverView::focused_title() const
{
    if (m_row <= 0)
        return nullptr;
    auto *self = const_cast<DiscoverView *>(this);
    const std::vector<Row *> rs = self->rows();
    if (m_row - 1 >= (int)rs.size())
        return nullptr;
    const Row &r = *rs[m_row - 1];
    if (!r.titles.ready() || r.focus >= (int)r.titles.value.size())
        return nullptr;
    return &r.titles.value[r.focus];
}

bool DiscoverView::modal() const { return m_typing || m_page_open || m_req_open || m_sheet.is_open(); }

bool DiscoverView::take_requested()
{
    const bool r = m_requested;
    m_requested = false;
    return r;
}

std::vector<PadHint> DiscoverView::hints(bool focused) const
{
    if (!focused || modal())
        return {};
    if (m_row == 0) {
        std::vector<PadHint> h = {{PadButton::Cross, i18n::tr(i18n::HintSearch)}};
        if (!m_query.empty())
            h.push_back({PadButton::Square, i18n::tr(i18n::HintClear)});
        return h;
    }
    if (!focused_title())
        return {};
    return {{PadButton::Options, i18n::tr(i18n::HintOptions)}};
}

bool DiscoverView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    const model::SeerrTitle *t = m_page_open ? &m_page_title : focused_title();
    if (!t || t->backdrop_path.empty())
        return false;
    *ref = poster_ref(t->backdrop_path, "w780");
    *key = ref->path;
    *seed = (int)(seed_of(t->title) % 997);
    return true;
}

Action DiscoverView::input(const input::State &in)
{
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    if (m_req_open) {
        request_input(in);
        return Action::None;
    }
    if (m_page_open) {
        page_input(in);
        return Action::None;
    }
    if (m_typing)
        return Action::None;   /* the keyboard has it */
    const uint32_t p = in.pressed;
    const std::vector<Row *> rs = rows();
    const int n = (int)rs.size();
    if (p & input::Up) {
        if (m_row == 0)
            return Action::ToTabs;
        m_row--;
    } else if (p & input::Down) {
        if (m_row >= n)
            return Action::Bump;
        m_row++;
    } else if (m_row == 0) {
        if (p & input::Cross) {
            m_typing = platform::text_entry_start(platform::TextKind::Search, i18n::tr(i18n::SearchTitle), m_query);
        } else if ((p & input::Square) && !m_query.empty()) {
            search("");
        }
    } else {
        Row &row = *rs[m_row - 1];
        const int count = row.titles.ready() ? (int)row.titles.value.size() : 0;
        if (p & input::Left) {
            if (row.focus <= 0)
                return Action::Bump;
            row.focus--;
        } else if (p & input::Right) {
            if (row.focus >= count - 1)
                return Action::Bump;
            row.focus++;
        } else if ((p & input::Cross) && row.focus < count) {
            open_page(row.titles.value[row.focus]);
        } else if ((p & input::Options) && row.focus < count) {
            open_menu(row.titles.value[row.focus]);
        }
    }
    return Action::None;
}

void DiscoverView::open_menu(const model::SeerrTitle &t)
{
    const model::SeerrTitle title = t;
    std::vector<std::string> labels = {i18n::tr(i18n::RequestOpenPage)};
    const bool jelly5 = !t.jellyfin_id.empty() && m_ctx.jelly5.available(m_ctx.services);
    if (jelly5)
        labels.push_back(m_ctx.jelly5.label(m_ctx.services));
    labels.push_back(i18n::tr(i18n::Cancel));
    m_sheet.open(t.title, i18n::tr(t.tv ? i18n::KindSeries : i18n::KindMovie), labels, (int)labels.size() - 1,
                 [this, title, jelly5](int c) {
                     if (c == 0)
                         open_page(title);
                     else if (c == 1 && jelly5)
                         m_ctx.jelly5.open(m_ctx.services, title.jellyfin_id, title.title);
                 });
}

void DiscoverView::draw(double now, float x0, float top, bool focused, float dt, bool *animating)
{
    if (!m_shelves_set || m_shelves[0].titles.state == load::Idle)
        load(false);
    if (m_typing) {
        *animating = true;   /* the text as it is typed (Linux), the keyboard's end */
        bool confirmed = false;
        std::string text;
        if (platform::text_entry_finished(&confirmed, &text)) {
            m_typing = false;
            if (confirmed) {
                search(text);
                if (!text.empty())
                    m_row = 1;   /* onto the results */
            }
        } else if (!platform::text_entry_active()) {
            m_typing = false;   /* the keyboard could not open */
        }
    }
    if (m_shelves[0].titles.state == load::Off) {   /* Seerr is off */
        m_drop.hide();
        draw_empty({x0, top, kListW, 300}, m_ctx.off_text(i18n::SeerrOff));
        return;
    }
    const std::vector<Row *> rs = rows();
    m_row = std::clamp(m_row, 0, (int)rs.size());
    const float list_h = kRowsTop + rs.size() * kRowH;
    const float focus_y = m_row == 0 ? 0 : kRowsTop - 50 + (m_row - 1) * kRowH;
    const float scroll = list_scroll(m_vscroll, focus_y, m_row == 0 ? kFieldH : kRowH, gfx::H - 110 - top, list_h,
                                     dt, animating);
    gfx::push_scissor({0, top - 30, gfx::W, gfx::H - 100 - top + 30});
    draw_field(x0, top - scroll, focused && m_row == 0, dt, animating);
    for (size_t i = 0; i < rs.size(); i++) {
        const float y = top + kRowsTop + i * kRowH - scroll;
        if (y + kRowH < top - 60 || y - 60 > gfx::H)
            continue;
        draw_row(*rs[i], focused && m_row == (int)i + 1, x0, y, now, dt, animating);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

void DiscoverView::draw_field(float x0, float y, bool focused, float dt, bool *animating)
{
    const gfx::Rect field{x0, y, kListW, kFieldH};
    glass_flat(field, kFieldH / 2, 0.95f);
    if (focused && !modal()) {
        m_drop.to(field, 1, 0, 0);
        m_drop.draw(dt, 1.f, animating, kFieldH / 2);
    } else {
        m_drop.hide();
        m_drop.draw(dt, 1.f, animating);
    }
    /* A magnifying glass: a ring and its handle. */
    const float cx = x0 + 46, cy = y + kFieldH / 2 - 3;
    gfx::fill({cx - 13, cy - 13, 26, 26}, kText2, 13);
    gfx::fill({cx - 9, cy - 9, 18, 18}, 0xff26262cu, 9);
    gfx::fill({cx + 8, cy + 9, 14, 5}, kText2, 2.5f);
    const std::string text = m_typing ? platform::text_entry_text() : m_query;
    const float tx = x0 + 86, tw = kListW - 120;
    if (text.empty() && !m_typing) {
        gfx::text(tx, y + 49, i18n::tr(i18n::SearchPrompt), {gfx::Regular, 30, tw}, kText3);
    } else {
        const float w = gfx::text(tx, y + 49, text, {gfx::Medium, 30, tw}, kText);
        if (m_typing && std::fmod(platform::now(), 1.0) < 0.6)   /* the caret, on Linux where it is typed here */
            gfx::fill({tx + w + 4, y + 20, 3, 38}, kText, 1.5f);
    }
}

void DiscoverView::draw_row(Row &row, bool focused, float x0, float y, double now, float dt, bool *animating)
{
    const std::string label = row.label == i18n::SearchResults ? fmt::f(i18n::tr(i18n::SearchResults), m_query.c_str())
                                                                : std::string(i18n::tr(row.label));
    gfx::text(x0, y, label, {gfx::SemiBold, 32, kListW}, focused ? kText : kText2);
    const float top = y + 26;
    const gfx::Rect area{x0, top, kListW, kCardH};
    if (!row.titles.ready()) {
        if (row.titles.state == load::Failed)
            draw_empty(area, fmt::labelled("Seerr", fmt::error(row.titles.error, row.titles.http_status)),
                       i18n::tr(i18n::Retry), true);
        else
            for (int i = 0; i < 8; i++)
                draw_skeleton({x0 + i * (kCardW + kCardGap), top, kCardW, kCardH}, 12, now, animating);
        return;
    }
    const auto &titles = row.titles.value;
    const int n = (int)titles.size();
    if (n == 0) {
        draw_empty(area, row.label == i18n::SearchResults ? fmt::f(i18n::tr(i18n::SearchNone), m_query.c_str())
                                                          : std::string(i18n::tr(i18n::ShelfEmpty)));
        return;
    }
    row.focus = std::clamp(row.focus, 0, n - 1);
    const float row_w = n * kCardW + (n - 1) * kCardGap;
    const float scroll = row_scroll(row.scroll, row_w, kListW, row.focus * (kCardW + kCardGap) + kCardW / 2, dt,
                                    animating);
    for (int i = 0; i < n; i++) {
        const model::SeerrTitle &t = titles[i];
        const float x = x0 + i * (kCardW + kCardGap) - scroll;
        const bool f = focused && i == row.focus;
        const float lift = row.lifts.step((t.tv ? "t" : "m") + std::to_string(t.tmdb_id), f, dt, animating);
        if (x + kCardW < -80 || x > gfx::W + 80)
            continue;
        draw_poster_card({x, top, kCardW, kCardH}, poster_ref(t.poster_path, "w342"), t.title, seed_of(t.title), lift,
                         1.f);
        Look lk;
        if (look(t.status, &lk))
            draw_chip(x + 8, top + kCardH - 34, i18n::tr(lk.label), lk.color, 1.f, 16);
        const float ty = top + kCardH + 34 + 10 * lift;
        gfx::text(x, ty, t.title, {f ? gfx::SemiBold : gfx::Medium, 22, kCardW + 8}, f ? kText : kText2);
        std::string line = i18n::tr(t.tv ? i18n::KindSeries : i18n::KindMovie);
        if (t.year > 0)
            line = std::to_string(t.year) + " · " + line;
        gfx::text(x, ty + 28, line, {gfx::Regular, 19, kCardW + 8}, kText3);
    }
}

/* ---- a title's page ---- */

void DiscoverView::open_page(const model::SeerrTitle &t)
{
    m_page_open = true;
    m_page_title = t;
    m_page_focus = 0;
    m_page_show.snap(0.f);
    m_page_show.to(1.f);
    m_page_drop.hide();
    m_detail = {};
    m_detail.state = load::Loading;
    m_servers = {};
    m_servers.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    const int id = t.tmdb_id;
    const bool tv = t.tv;
    m_ctx.services->title_detail(tv, id, [this, alive, id, tv](api::Result<model::SeerrDetail> r) {
        if (alive.expired() || !m_page_open || m_page_title.tmdb_id != id || m_page_title.tv != tv)
            return;
        m_detail.take(std::move(r), platform::now());
        if (m_detail.ready())
            m_page_title = m_detail.value.title;
    });
    /* Which servers Seerr has for this kind: whether 4K may be asked for. */
    m_ctx.services->seerr_servers(tv, [this, alive, id, tv](api::Result<std::vector<model::SeerrServer>> r) {
        if (alive.expired() || !m_page_open || m_page_title.tmdb_id != id || m_page_title.tv != tv)
            return;
        m_servers.take(std::move(r), platform::now());
    });
}

std::vector<model::SeerrServer> DiscoverView::servers_for(bool is4k) const
{
    std::vector<model::SeerrServer> out;
    if (m_servers.ready())
        for (const auto &s : m_servers.value)
            if (s.is4k == is4k)
                out.push_back(s);
    return out;
}

std::vector<int> DiscoverView::requestable_seasons(bool is4k) const
{
    std::vector<int> out;
    if (!m_detail.ready())
        return out;
    const bool specials = m_public.ready() && m_public.value.special_episodes;
    for (const auto &s : m_detail.value.seasons) {
        if (s.number == 0 && !specials)
            continue;
        if (requestable(is4k ? s.status4k : s.status) && !(is4k ? s.requested4k : s.requested))
            out.push_back(s.number);
    }
    return out;
}

bool DiscoverView::can_request(bool is4k) const
{
    if (!m_detail.ready() || (is4k && servers_for(true).empty()))
        return false;
    const model::SeerrTitle &t = m_detail.value.title;
    if (t.tv)
        return !requestable_seasons(is4k).empty();
    return requestable(is4k ? t.status4k : t.status);
}

std::vector<DiscoverView::PageButton> DiscoverView::page_buttons() const
{
    std::vector<PageButton> b;
    if (can_request(false) || can_request(true))
        b.push_back(PageButton::Request);
    if (!m_page_title.jellyfin_id.empty() && m_ctx.jelly5.available(m_ctx.services))
        b.push_back(PageButton::Jelly5);
    b.push_back(PageButton::Close);
    return b;
}

void DiscoverView::page_input(const input::State &in)
{
    const uint32_t p = in.pressed;
    const std::vector<PageButton> bs = page_buttons();
    const int n = (int)bs.size();
    if (p & input::Circle) {
        m_page_open = false;
    } else if (p & input::Left) {
        m_page_focus = std::max(0, m_page_focus - 1);
    } else if (p & input::Right) {
        m_page_focus = std::min(n - 1, m_page_focus + 1);
    } else if (p & input::Cross) {
        switch (bs[(size_t)std::clamp(m_page_focus, 0, n - 1)]) {
        case PageButton::Request: open_request(); break;
        case PageButton::Jelly5:
            m_ctx.jelly5.open(m_ctx.services, m_page_title.jellyfin_id, m_page_title.title);
            break;
        case PageButton::Close: m_page_open = false; break;
        }
    } else if (p & input::Triangle) {
        open_page(m_page_title);
    }
}

void DiscoverView::draw_overlay(float dt, bool *animating)
{
    if (m_page_open)
        draw_page(dt, animating);
    if (m_req_open)
        draw_request(dt, animating);
    m_sheet.draw(dt, animating);
}

void DiscoverView::draw_page(float dt, bool *animating)
{
    if (m_page_show.step(dt, 10.f))
        *animating = true;
    if (art::busy())
        *animating = true;
    const float a = smoothstep(m_page_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.55f * a));
    const gfx::Rect r{150, 96 + (1 - a) * 40, gfx::W - 300, gfx::H - 192};
    glass_panel(r, 44, a, true);
    gfx::push_opacity(a);
    const model::SeerrTitle &t = m_page_title;
    const float px = r.x + 64, py = r.y + 64, pw = 330, ph = 495;
    art::draw_poster({px, py, pw, ph}, poster_ref(t.poster_path, "w500"), t.title, seed_of(t.title), 22, 1.f, 500);
    const float tx = px + pw + 60, tw = r.x + r.w - 64 - tx;
    float y = py + 50;
    gfx::text(tx, y, t.title, {gfx::Bold, 52, tw}, kText);
    y += 54;
    std::string line = i18n::tr(t.tv ? i18n::KindSeries : i18n::KindMovie);
    if (t.year > 0)
        line = std::to_string(t.year) + " · " + line;
    if (m_detail.ready()) {
        const model::SeerrDetail &d = m_detail.value;
        if (d.runtime > 0)
            line += " · " + fmt::f(i18n::tr(i18n::Minutes), d.runtime);
        for (size_t i = 0; i < d.genres.size() && i < 2; i++)
            line += " · " + d.genres[i];
    }
    gfx::text(tx, y, line, {gfx::Regular, 26, tw}, kText2);
    y += 30;
    /* Where it stands: the standard side, and the 4K side when Seerr has one. */
    float cx = tx;
    Look lk;
    if (look(t.status, &lk))
        cx += draw_chip(cx, y, i18n::tr(lk.label), lk.color, 1.f, 22) + 12;
    if (!servers_for(true).empty() && look(t.status4k, &lk))
        draw_chip(cx, y, "4K · " + std::string(i18n::tr(lk.label)), lk.color, 1.f, 22);
    y += 76;
    if (m_detail.state == load::Failed) {
        gfx::text(tx, y, fmt::labelled("Seerr", fmt::error(m_detail.error, m_detail.http_status)),
                  {gfx::SemiBold, 28, tw, 2, 36}, 0xffff8a80u);
    } else if (!m_detail.ready()) {
        gfx::text(tx, y, i18n::tr(i18n::Loading), {gfx::Regular, 28, tw}, kText3);
    } else {
        const model::SeerrDetail &d = m_detail.value;
        const std::string overview = d.title.overview.empty() ? t.overview : d.title.overview;
        gfx::text(tx, y, overview, {gfx::Regular, 28, tw, d.title.tv ? 3 : 6, 40}, kText);
        y += (d.title.tv ? 3 : 6) * 40 + 20;
        if (d.title.tv) {   /* the seasons, two columns */
            gfx::text(tx, y, i18n::tr(i18n::SeasonsTitle), {gfx::SemiBold, 28}, kText2);
            y += 44;
            const float col_w = tw / 2;
            int shown = 0;
            for (const auto &s : d.seasons) {
                if (shown >= 10)
                    break;
                const float sx = tx + (shown % 2) * col_w, sy = y + (shown / 2) * 44;
                std::string name = s.number == 0 ? std::string(i18n::tr(i18n::Specials))
                                                 : fmt::f(i18n::tr(i18n::SeasonOne), s.number);
                if (s.episodes > 0)
                    name += " · " + fmt::count(i18n::LibEpisodesOne, i18n::LibEpisodes, s.episodes);
                const float w = gfx::text(sx, sy, name, {gfx::Regular, 24, col_w - 220}, kText);
                const int st = s.status != model::SeerrUnknown && s.status != model::SeerrNone ? s.status
                               : s.requested                                                     ? model::SeerrPending
                                                                                                 : s.status;
                if (look(st, &lk))
                    draw_chip(sx + w + 14, sy - 24, i18n::tr(lk.label), lk.color, 1.f, 18);
                shown++;
            }
        }
    }

    /* The buttons, along the foot. */
    const std::vector<PageButton> bs = page_buttons();
    m_page_focus = std::clamp(m_page_focus, 0, (int)bs.size() - 1);
    float bx = tx;
    const float by = r.y + r.h - 64 - kButtonH;
    std::vector<gfx::Rect> rects;
    std::vector<std::string> labels;
    for (PageButton b : bs) {
        const std::string label = b == PageButton::Request ? i18n::tr(i18n::RequestAsk)
                                  : b == PageButton::Jelly5 ? m_ctx.jelly5.label(m_ctx.services)
                                                            : i18n::tr(i18n::Close);
        const float bw = std::max(170.f, gfx::text_width(label, kButtonText) + 64);
        const gfx::Rect bar{bx, by - kBarPad, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
        glass_panel(bar, bar.h / 2, 1.f, false);
        rects.push_back({bx + kBarPad, by, bw, kButtonH});
        labels.push_back(label);
        bx += bar.w + 18;
    }
    gfx::pop_opacity();
    if (!m_req_open && !m_sheet.is_open()) {
        m_page_drop.to(rects[(size_t)m_page_focus], m_page_focus, 0, 0);
        m_page_drop.draw(dt, a, animating);
    } else {
        m_page_drop.hide();
    }
    for (size_t i = 0; i < rects.size(); i++) {
        const bool on = (int)i == m_page_focus;
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 42, labels[i], on ? kButtonOn : kButtonText,
                  alpha(on ? kText : kText2, a), 1);
    }
}

/* ---- the request ---- */

void DiscoverView::open_request()
{
    if (!m_detail.ready())
        return;
    m_req_open = true;
    m_req_focus = 0;
    m_sending = false;
    m_req_show.snap(0.f);
    m_req_show.to(1.f);
    m_req_drop.hide();
    m_is4k = !can_request(false) && can_request(true);
    m_seasons = requestable_seasons(m_is4k);   /* every season still missing, ticked */
    const std::vector<model::SeerrServer> servers = servers_for(m_is4k);
    int def = 0;
    for (size_t i = 0; i < servers.size(); i++)
        if (servers[i].is_default) {
            def = (int)i;
            break;
        }
    choose_server(def);
}

void DiscoverView::choose_server(int index)
{
    const std::vector<model::SeerrServer> servers = servers_for(m_is4k);
    m_server = {};
    m_profile_index = m_folder_index = 0;
    if (servers.empty())
        return;   /* Seerr's defaults decide */
    m_server_index = std::clamp(index, 0, (int)servers.size() - 1);
    m_server.state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    const bool tv = m_page_title.tv;
    const int id = servers[(size_t)m_server_index].id;
    m_ctx.services->seerr_server(tv, id, [this, alive, id](api::Result<model::SeerrServer> r) {
        if (alive.expired() || !m_req_open)
            return;
        m_server.take(std::move(r), platform::now());
        if (!m_server.ready() || m_server.value.id != id)
            return;
        /* Seerr's defaults for this server, preselected. */
        const model::SeerrServer &s = m_server.value;
        for (size_t i = 0; i < s.profiles.size(); i++)
            if (s.profiles[i].first == s.profile_id)
                m_profile_index = (int)i;
        for (size_t i = 0; i < s.folders.size(); i++)
            if (s.folders[i] == s.folder)
                m_folder_index = (int)i;
    });
}

std::vector<DiscoverView::RequestLine> DiscoverView::request_lines() const
{
    std::vector<RequestLine> out;
    if (can_request(true))   /* said whenever 4K is involved; a choice only when both can be */
        out.push_back({Line::Quality});
    if (m_page_title.tv)
        for (int s : requestable_seasons(m_is4k))
            out.push_back({Line::Season, s});
    if (servers_for(m_is4k).size() > 1)
        out.push_back({Line::Server});
    if (m_server.ready() && !m_server.value.profiles.empty())
        out.push_back({Line::Profile});
    if (m_server.ready() && m_server.value.folders.size() > 1)
        out.push_back({Line::Folder});
    out.push_back({Line::Send});
    out.push_back({Line::Cancel});
    return out;
}

void DiscoverView::request_input(const input::State &in)
{
    const uint32_t p = in.pressed;
    const std::vector<RequestLine> lines = request_lines();
    const int n = (int)lines.size();
    m_req_focus = std::clamp(m_req_focus, 0, n - 1);
    const Line at = lines[(size_t)m_req_focus].line;
    const bool on_buttons = at == Line::Send || at == Line::Cancel;
    auto cycle = [&](int dir) {
        if (at == Line::Server) {
            const int count = (int)servers_for(m_is4k).size();
            choose_server((m_server_index + dir + count) % count);
        } else if (at == Line::Profile && m_server.ready()) {
            const int count = (int)m_server.value.profiles.size();
            m_profile_index = (m_profile_index + dir + count) % count;
        } else if (at == Line::Folder && m_server.ready()) {
            const int count = (int)m_server.value.folders.size();
            m_folder_index = (m_folder_index + dir + count) % count;
        }
    };
    if (p & input::Circle) {
        m_req_open = false;
    } else if (p & input::Up) {
        if (on_buttons)
            m_req_focus = std::max(0, n - 3);
        else
            m_req_focus = std::max(0, m_req_focus - 1);
    } else if (p & input::Down) {
        if (!on_buttons)
            m_req_focus = std::min(n - 2, m_req_focus + 1);
    } else if (p & (input::Left | input::Right)) {
        const int dir = (p & input::Left) ? -1 : 1;
        if (on_buttons)
            m_req_focus = dir < 0 ? n - 2 : n - 1;
        else
            cycle(dir);
    } else if (p & input::Cross) {
        switch (at) {
        case Line::Quality:
            if (!can_request(!m_is4k))
                break;   /* only one of them can be asked for */
            m_is4k = !m_is4k;
            m_seasons = requestable_seasons(m_is4k);
            choose_server(0);
            for (size_t i = 0; i < servers_for(m_is4k).size(); i++)
                if (servers_for(m_is4k)[i].is_default)
                    choose_server((int)i);
            break;
        case Line::Season: {
            if (m_public.ready() && !m_public.value.partial_requests)
                break;   /* Seerr asks for every season at once */
            const int s = lines[(size_t)m_req_focus].season;
            auto it = std::find(m_seasons.begin(), m_seasons.end(), s);
            if (it == m_seasons.end())
                m_seasons.push_back(s);
            else
                m_seasons.erase(it);
            break;
        }
        case Line::Server:
        case Line::Profile:
        case Line::Folder: cycle(1); break;
        case Line::Send: confirm_request(); break;
        case Line::Cancel: m_req_open = false; break;
        }
    }
}

void DiscoverView::confirm_request()
{
    if (m_sending)
        return;
    if (m_page_title.tv && m_seasons.empty()) {
        notify(i18n::tr(i18n::RequestNoSeason), false, platform::now());
        return;
    }
    std::sort(m_seasons.begin(), m_seasons.end());
    std::string body = i18n::tr(m_page_title.tv ? i18n::KindSeries : i18n::KindMovie);
    if (m_page_title.tv) {
        std::string list;
        for (size_t i = 0; i < m_seasons.size(); i++)
            list += (i ? ", " : "") + std::to_string(m_seasons[i]);
        body += " · " + (m_seasons.size() == 1 ? fmt::f(i18n::tr(i18n::SeasonOne), m_seasons[0])
                                               : fmt::f(i18n::tr(i18n::SeasonsList), list.c_str()));
    }
    if (m_is4k)
        body += " · 4K";
    if (m_server.ready() && m_profile_index < (int)m_server.value.profiles.size())
        body += " · " + m_server.value.profiles[(size_t)m_profile_index].second;
    body += "\n" + std::string(i18n::tr(i18n::RequestAsAccount));
    m_sheet.open(fmt::f(i18n::tr(i18n::RequestTitle), m_page_title.title.c_str()), body,
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::RequestSend)}, 0, [this](int c) {
                     if (c == 1)
                         send_request();
                 });
}

void DiscoverView::send_request()
{
    if (m_sending || !m_ctx.services || !m_detail.ready())
        return;
    seerr::NewRequest r;
    r.tmdb_id = m_page_title.tmdb_id;
    r.tv = m_page_title.tv;
    r.tvdb_id = m_detail.value.tvdb_id;
    r.seasons = m_seasons;
    r.is4k = m_is4k;
    if (m_server.ready()) {
        r.server_id = m_server.value.id;
        if (m_profile_index < (int)m_server.value.profiles.size())
            r.profile_id = m_server.value.profiles[(size_t)m_profile_index].first;
        if (m_folder_index < (int)m_server.value.folders.size())
            r.root_folder = m_server.value.folders[(size_t)m_folder_index];
    }
    m_sending = true;
    std::weak_ptr<bool> alive = m_alive;
    const model::SeerrTitle title = m_page_title;
    m_ctx.services->create_request(r, [this, alive, title](api::Result<model::SeerrCreated> res) {
        if (alive.expired())
            return;
        m_sending = false;
        const double now = platform::now();
        if (res.ok()) {
            m_requested = true;
            m_req_open = false;
            const char *text = m_ctx.mock                  ? i18n::tr(i18n::RequestMock)
                               : res.http_status == 202     ? i18n::tr(i18n::RequestNothing)
                               : res.value.status == 2      ? i18n::tr(i18n::RequestDoneApproved)
                                                            : i18n::tr(i18n::RequestDonePending);
            notify(text, res.http_status != 202, now);
            if (m_page_open && m_page_title.tmdb_id == title.tmdb_id)
                open_page(title);   /* where it stands now */
        } else if (res.http_status == 409) {
            notify(i18n::tr(i18n::RequestDuplicate), false, now);
        } else if (res.http_status == 403) {
            notify(i18n::tr(i18n::RequestRefused), false, now);
        } else {
            notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(res.error, res.http_status).c_str()), false, now);
        }
    });
}

void DiscoverView::draw_request(float dt, bool *animating)
{
    if (m_req_show.step(dt, 12.f))
        *animating = true;
    const float a = smoothstep(m_req_show.value);
    const std::vector<RequestLine> lines = request_lines();
    const int n = (int)lines.size();
    m_req_focus = std::clamp(m_req_focus, 0, n - 1);
    constexpr float kLineH = 64, kW = 1100;
    const int listed = n - 2;   /* the buttons apart */
    const int visible = std::min(listed, 9);
    const float h = 150 + std::max(1, visible) * kLineH + 40 + kButtonH + 70;
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0xff000000u, 0.45f * a));
    const gfx::Rect r{(gfx::W - kW) / 2, (gfx::H - h) / 2 + (1 - a) * 30, kW, h};
    glass_panel(r, 40, a, true);
    gfx::push_opacity(a);
    const float x = r.x + 60, w = kW - 120;
    gfx::text(x, r.y + 84, fmt::f(i18n::tr(i18n::RequestTitle), m_page_title.title.c_str()), {gfx::Bold, 42, w},
              kText);
    const float top = r.y + 130;
    /* The lines, scrolled to keep the focus in view. */
    const int first = std::clamp(m_req_focus < listed ? m_req_focus - visible + 1 : 0, 0, std::max(0, listed - visible));
    gfx::Rect focus_rect{};
    for (int i = first; i < listed && i < first + visible; i++) {
        const RequestLine &l = lines[(size_t)i];
        const float ly = top + (i - first) * kLineH;
        const gfx::Rect row{x - 16, ly, w + 32, kLineH - 6};
        if (i == m_req_focus)
            focus_rect = row;
        const float cy = ly + kLineH / 2 - 3;
        std::string text;
        switch (l.line) {
        case Line::Quality: text = i18n::tr(m_is4k ? i18n::Request4k : i18n::RequestStd); break;
        case Line::Season: {
            const bool on = std::find(m_seasons.begin(), m_seasons.end(), l.season) != m_seasons.end();
            const bool partial = !m_public.ready() || m_public.value.partial_requests;
            checkbox(x, cy, on, partial);
            text = l.season == 0 ? std::string(i18n::tr(i18n::Specials)) : fmt::f(i18n::tr(i18n::SeasonOne), l.season);
            if (m_detail.ready())
                for (const auto &s : m_detail.value.seasons)
                    if (s.number == l.season && s.episodes > 0)
                        text += " · " + fmt::count(i18n::LibEpisodesOne, i18n::LibEpisodes, s.episodes);
            gfx::text(x + 50, cy + 10, text, {gfx::Medium, 28, w - 50}, kText);
            continue;
        }
        case Line::Server: {
            const std::vector<model::SeerrServer> s = servers_for(m_is4k);
            text = fmt::f(i18n::tr(i18n::RequestServer), s[(size_t)std::min(m_server_index, (int)s.size() - 1)].name.c_str());
            break;
        }
        case Line::Profile:
            text = fmt::f(i18n::tr(i18n::RequestProfile),
                          m_server.value.profiles[(size_t)m_profile_index].second.c_str());
            break;
        case Line::Folder:
            text = fmt::f(i18n::tr(i18n::RequestFolder), m_server.value.folders[(size_t)m_folder_index].c_str());
            break;
        default: break;
        }
        gfx::text(x, cy + 10, text, {gfx::Medium, 28, w - 60}, kText);
        if (l.line != Line::Quality || can_request(!m_is4k))   /* something to change here */
            gfx::text(x + w, cy + 10, "‹ ›", {gfx::Medium, 26}, kText3, 2);
    }
    if (listed == 0 || (m_server.loading() && servers_for(m_is4k).size() > 0))
        gfx::text(x, top + listed * kLineH + 34, i18n::tr(i18n::Loading), {gfx::Regular, 24}, kText3);

    /* Send / Cancel. */
    const float by = r.y + r.h - 50 - kButtonH;
    const std::string labels[2] = {i18n::tr(i18n::RequestSend), i18n::tr(i18n::Cancel)};
    gfx::Rect rects[2];
    float bx = x;
    for (int i = 0; i < 2; i++) {
        const float bw = std::max(170.f, gfx::text_width(labels[i], kButtonText) + 64);
        const gfx::Rect bar{bx, by - kBarPad, bw + 2 * kBarPad, kButtonH + 2 * kBarPad};
        glass_panel(bar, bar.h / 2, 1.f, false);
        rects[i] = {bx + kBarPad, by, bw, kButtonH};
        bx += bar.w + 18;
    }
    gfx::pop_opacity();
    const Line at = lines[(size_t)m_req_focus].line;
    if (!m_sheet.is_open()) {
        m_req_drop.to(at == Line::Send ? rects[0] : at == Line::Cancel ? rects[1] : focus_rect, m_req_focus, 0, 0);
        m_req_drop.draw(dt, a, animating, at == Line::Send || at == Line::Cancel ? -1.f : 16.f);
    } else {
        m_req_drop.hide();
    }
    for (int i = 0; i < 2; i++) {
        const bool on = (i == 0 && at == Line::Send) || (i == 1 && at == Line::Cancel);
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 42, labels[i], on ? kButtonOn : kButtonText,
                  alpha(on ? kText : kText2, a), 1);
    }
}

} // namespace ui
