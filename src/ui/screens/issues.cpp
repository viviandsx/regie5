/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/issues.h"

#include "api/seerr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"
#include "ui/style.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kRowH = 132, kRowGap = 14;
constexpr float kListW = gfx::W - 2 * kPad;

const seerr::IssueFilter kFilterApi[IssuesView::kFilters] = {seerr::IssueFilter::Open, seerr::IssueFilter::Resolved,
                                                              seerr::IssueFilter::All};

/* The ready-made comments, in the order offered. */
const i18n::S kComments[] = {i18n::CommentLooking, i18n::CommentFixed, i18n::CommentReplaced,
                             i18n::CommentCannotReproduce};

i18n::S type_label(model::Issue::Type t)
{
    switch (t) {
    case model::Issue::Video: return i18n::IssueVideo;
    case model::Issue::Audio: return i18n::IssueAudio;
    case model::Issue::Subtitles: return i18n::IssueSubtitles;
    case model::Issue::Other: break;
    }
    return i18n::IssueOther;
}

} // namespace

void IssuesView::load(bool force) { load_filter(m_filter, force); }

void IssuesView::load_filter(int f, bool force)
{
    if (!m_ctx.services || m_pages[f].loading() || (!force && m_pages[f].state != load::Idle))
        return;
    if (!m_pages[f].ready())
        m_pages[f].state = load::Loading;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->issues(kFilterApi[f], [this, alive, f](api::Result<model::IssuePage> r) {
        if (alive.expired())
            return;
        m_pages[f].take(std::move(r), platform::now());
        if (f == m_filter)
            m_focus = std::min(m_focus, std::max(0, (int)m_pages[f].value.issues.size() - 1));
    });
}

std::vector<std::string> IssuesView::filter_labels() const
{
    return {i18n::tr(i18n::IssueFilterOpen), i18n::tr(i18n::IssueFilterResolved), i18n::tr(i18n::IssueFilterAll)};
}

void IssuesView::set_filter(int f)
{
    if (f < 0 || f >= kFilters)
        return;
    m_filter = f;
    m_focus = 0;
    m_scroll.snap(0);
    load(platform::now() - m_pages[f].at > 30);
}

const model::Issue *IssuesView::focused() const
{
    const Load<model::IssuePage> &p = m_pages[m_filter];
    if (!p.ready() || m_focus < 0 || m_focus >= (int)p.value.issues.size())
        return nullptr;
    return &p.value.issues[m_focus];
}

std::string IssuesView::what(const model::Issue &i) const
{
    std::string s = i18n::tr(type_label(i.type));
    if (i.tv && i.season > 0)
        s += " · " + (i.episode > 0 ? fmt::episode(i.season, i.episode) : fmt::f(i18n::tr(i18n::SeasonOne), i.season));
    return s;
}

std::vector<ListSheet::Item> IssuesView::thread_items(const model::Issue &i) const
{
    const time_t now = time(nullptr);
    std::vector<ListSheet::Item> items;
    for (size_t k = 0; k < i.comments.size(); k++) {
        const model::IssueComment &c = i.comments[k];
        /* The report is the reporter's; the others' authors come with the issue alone. */
        const std::string who = !c.author.empty() ? c.author : k == 0 ? i.reported_by : std::string("…");
        ListSheet::Item it;
        it.chips.push_back({who, k == 0 ? kOrange : 0xff0a84ffu});
        it.text = c.message;
        it.detail = c.created ? fmt::ago(c.created, now) : std::string();
        items.push_back(std::move(it));
    }
    return items;
}

void IssuesView::open_thread(const model::Issue &i)
{
    m_thread_id = i.id;
    m_thread.open(m_titles.title(i.tv, i.tmdb_id) + " · " + what(i), thread_items(i),
                  i18n::tr(i18n::IssueThreadLoading));
    if (!m_ctx.services)
        return;
    std::weak_ptr<bool> alive = m_alive;
    m_ctx.services->issue(i.id, [this, alive](api::Result<model::Issue> r) {
        if (alive.expired() || !r.ok() || !m_thread.is_open() || r.value.id != m_thread_id)
            return;
        m_thread.update(thread_items(r.value));
    });
}

void IssuesView::open_menu(const model::Issue &i)
{
    const model::Issue issue = i;
    std::vector<std::string> labels;
    std::vector<int> what_of;   /* 0 resolve, 1 comment */
    if (!i.resolved) {
        labels.push_back(i18n::tr(i18n::ResolveAsk));
        what_of.push_back(0);
    }
    labels.push_back(i18n::tr(i18n::CommentAsk));
    what_of.push_back(1);
    labels.push_back(i18n::tr(i18n::Cancel));
    m_sheet.open(m_titles.title(i.tv, i.tmdb_id), what(i) + " · " + fmt::f(i18n::tr(i18n::ReportedBy), i.reported_by.c_str()),
                 labels, (int)labels.size() - 1, [this, issue, what_of](int c) {
                     if (c < 0 || c >= (int)what_of.size())
                         return;
                     if (what_of[c] == 0)
                         ask_resolve(issue);
                     else
                         choose_comment(issue);
                 });
}

void IssuesView::ask_resolve(const model::Issue &i)
{
    const int id = i.id;
    m_sheet.open(i18n::tr(i18n::ConfirmResolve),
                 m_titles.title(i.tv, i.tmdb_id) + " · " + what(i) + " · " +
                     fmt::f(i18n::tr(i18n::ReportedBy), i.reported_by.c_str()),
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::Resolve)}, 0, [this, id](int c) {
                     if (c != 1 || m_busy || !m_ctx.services)
                         return;
                     m_busy = true;
                     std::weak_ptr<bool> alive = m_alive;
                     m_ctx.services->resolve_issue(id, [this, alive](api::Result<bool> r) {
                         if (!alive.expired())
                             done(std::move(r), i18n::tr(i18n::DoneResolved));
                     });
                 });
}

void IssuesView::choose_comment(const model::Issue &i)
{
    const model::Issue issue = i;
    std::vector<ListSheet::Item> items;
    for (i18n::S s : kComments) {
        ListSheet::Item it;
        it.text = i18n::tr(s);
        it.detail = i18n::tr(i18n::CommentAsSeerr);
        items.push_back(std::move(it));
    }
    m_thread.open(i18n::tr(i18n::ChooseComment), std::move(items), {}, [this, issue](int c) {
        if (c >= 0 && c < (int)(sizeof kComments / sizeof kComments[0]))
            ask_comment(issue, i18n::tr(kComments[c]));
    });
}

void IssuesView::ask_comment(const model::Issue &i, const std::string &message)
{
    const int id = i.id;
    m_sheet.open(i18n::tr(i18n::ConfirmComment), fmt::f(i18n::tr(i18n::Quoted), message.c_str()) + " · " + m_titles.title(i.tv, i.tmdb_id),
                 {i18n::tr(i18n::Cancel), i18n::tr(i18n::Post)}, 0, [this, id, message](int c) {
                     if (c != 1 || m_busy || !m_ctx.services)
                         return;
                     m_busy = true;
                     std::weak_ptr<bool> alive = m_alive;
                     m_ctx.services->comment_issue(id, message, [this, alive](api::Result<bool> r) {
                         if (!alive.expired())
                             done(std::move(r), i18n::tr(i18n::DoneCommented));
                     });
                 });
}

void IssuesView::done(api::Result<bool> res, const char *ok_text)
{
    m_busy = false;
    const double now = platform::now();
    if (res.ok())
        notify(ok_text, true, now);
    else
        notify(fmt::f(i18n::tr(i18n::ActionFailed), fmt::error(res.error, res.http_status).c_str()), false, now);
    for (int f = 0; f < kFilters; f++)   /* every list may have changed */
        if (m_pages[f].state != load::Idle)
            load_filter(f, true);
}

std::vector<PadHint> IssuesView::hints(bool focused_view) const
{
    if (!focused_view || !focused())
        return {};
    return {{PadButton::Options, i18n::tr(i18n::HintOptions)}};
}

Action IssuesView::input(const input::State &in)
{
    if (m_thread.is_open()) {
        m_thread.input(in);
        return Action::None;
    }
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    const Load<model::IssuePage> &page = m_pages[m_filter];
    const int n = page.ready() ? (int)page.value.issues.size() : 0;
    if (p & input::Up) {
        if (m_focus <= 0)
            return Action::ToTabs;
        m_focus--;
    } else if (p & input::Down) {
        if (m_focus >= n - 1)
            return Action::Bump;
        m_focus++;
    } else if (const model::Issue *i = focused()) {
        if (p & input::Cross)
            open_thread(*i);
        else if (p & input::Options)
            open_menu(*i);
    }
    return Action::None;
}

bool IssuesView::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    const model::Issue *i = focused();
    return i && m_titles.backdrop(i->tv, i->tmdb_id, key, ref, seed);
}

void IssuesView::draw_overlay(float dt, bool *animating)
{
    m_thread.draw(dt, animating);
    m_sheet.draw(dt, animating);
}

void IssuesView::draw(double now, float x0, float top, bool focused_view, float dt, bool *animating)
{
    load(false);
    const Load<model::IssuePage> &page = m_pages[m_filter];
    const gfx::Rect area{x0, top, kListW, 300};
    if (!page.ready() || page.value.issues.empty()) {
        m_drop.hide();
        if (page.state == load::Off)
            draw_empty(area, m_ctx.off_text(i18n::SeerrOff));
        else if (page.state == load::Failed)
            draw_empty(area, fmt::labelled("Seerr", fmt::error(page.error, page.http_status)), i18n::tr(i18n::Retry),
                       true);
        else if (!page.ready())
            for (int i = 0; i < 4; i++)
                draw_skeleton({x0, top + i * (kRowH + kRowGap), kListW, kRowH}, 24, now, animating);
        else
            draw_empty(area, i18n::tr(m_filter == 0   ? i18n::EmptyIssuesOpen
                                      : m_filter == 1 ? i18n::EmptyIssuesResolved
                                                      : i18n::EmptyIssues));
        return;
    }

    const auto &issues = page.value.issues;
    const int n = (int)issues.size();
    m_focus = std::clamp(m_focus, 0, n - 1);
    const float list_h = n * (kRowH + kRowGap);
    const float scroll =
        list_scroll(m_scroll, m_focus * (kRowH + kRowGap), kRowH, gfx::H - 120 - top, list_h, dt, animating);
    gfx::push_scissor({0, top - 20, gfx::W, gfx::H - 110 - top + 20});
    for (int i = 0; i < n; i++) {
        const float y = top + i * (kRowH + kRowGap) - scroll;
        if (y + kRowH >= top - 20 && y <= gfx::H)
            glass_flat({x0, y, kListW, kRowH}, 24, 0.9f);
    }
    if (focused_view) {
        m_drop.to({x0, top + m_focus * (kRowH + kRowGap) - scroll, kListW, kRowH}, m_focus, 0, -scroll);
        m_drop.draw(dt, 1.f, animating, 24);
    } else {
        m_drop.hide();
        m_drop.draw(dt, 1.f, animating);
    }
    const time_t now_t = time(nullptr);
    for (int k = 0; k < n; k++) {
        const model::Issue &is = issues[k];
        const float y = top + k * (kRowH + kRowGap) - scroll;
        if (y + kRowH < top - 20 - 3 * kRowH || y > gfx::H + 3 * kRowH)
            continue;   /* a few beyond the edges still fetch their details */
        const model::MediaDetail *d = m_titles.get(is.tv, is.tmdb_id);
        if (y + kRowH < top - 20 || y > gfx::H)
            continue;
        const bool f = focused_view && k == m_focus;
        const std::string title = m_titles.title(is.tv, is.tmdb_id);
        const art::Ref poster = d ? art::Ref{config::Seerr, seerr::image_path(d->poster_path, "w185")} : art::Ref{};
        if (d || m_titles.failed(is.tv, is.tmdb_id))
            art::draw_poster({x0 + 18, y + 12, 72, 108}, poster, title, seed_of(title), 10, 1.f, 160);
        else
            draw_skeleton({x0 + 18, y + 12, 72, 108}, 10, now, animating);

        const std::string state = i18n::tr(is.resolved ? i18n::IssueResolved : i18n::IssueOpen);
        const float right = x0 + kListW - 28;
        const float chip_w = chip_width(state, 22);
        draw_chip(right - chip_w, y + 18, state, is.resolved ? kGreen : kOrange, 1.f, 22);

        const float tx = x0 + 116, text_w = kListW - 116 - chip_w - 160;
        gfx::text(tx, y + 50, title + (d && d->year > 0 ? " (" + std::to_string(d->year) + ")" : ""),
                  {f ? gfx::Bold : gfx::SemiBold, 32, text_w}, kText);
        std::string line = what(is) + " · " + fmt::f(i18n::tr(i18n::ReportedBy), is.reported_by.c_str());
        if (is.created)
            line += " · " + fmt::ago(is.created, now_t);
        const int replies = (int)is.comments.size() - 1;
        if (replies > 0)
            line += " · " + fmt::count(i18n::IssueRepliesOne, i18n::IssueReplies, replies);
        gfx::text(tx, y + 86, line, {gfx::Regular, 24, text_w}, kText2);
        if (!is.comments.empty())   /* the last thing said */
            gfx::text(tx, y + 118, fmt::f(i18n::tr(i18n::Quoted), is.comments.back().message.c_str()), {gfx::Regular, 22, kListW - 116 - 60},
                      kText3);
    }
    gfx::pop_scissor();
    if (art::busy())
        *animating = true;
}

} // namespace ui
