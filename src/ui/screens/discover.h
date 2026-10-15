/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Demandes › Découvrir (Seerr): a search field (the system keyboard on the
 * PS5), then rows of posters: the search's results, trending titles, popular
 * and upcoming films and series. ✕ opens a title's page (where it stands,
 * season by season for a series); « Demander… » sets a request up (the
 * seasons, 4K when Seerr has a 4K server, the server, its quality profile
 * and folder) and asks again, Cancel first, before sending it. Requests go
 * as the API key's account. Pictures come through Seerr's image proxy.
 */
#pragma once

#include "core/model.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/hints.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class DiscoverView {
public:
    explicit DiscoverView(Context &ctx) : m_ctx(ctx) {}
    ~DiscoverView();   /* an open keyboard closes, its text given to nobody */

    /* The rows (force: again, as △ asks). */
    void load(bool force);
    void enter() { m_row = 0; }
    /* Action::ToTabs: up out of the field (to the pills). */
    Action input(const input::State &in);
    void draw(double now, float x0, float top, bool focused, float dt, bool *animating);
    void draw_overlay(float dt, bool *animating);
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;
    std::vector<PadHint> hints(bool focused) const;
    /* The keyboard, a title's page or a sheet is open: every press is theirs. */
    bool modal() const;
    /* A request was sent since the last call (the request lists are stale). */
    bool take_requested();

private:
    struct Row {
        i18n::S label = i18n::ShelfTrending;
        Load<std::vector<model::SeerrTitle>> titles;
        int focus = 0;
        Anim scroll;
        Lifts lifts;
    };
    /* The rows shown, in order: the results (with a query), then the shelves. */
    std::vector<Row *> rows();
    const model::SeerrTitle *focused_title() const;
    void search(const std::string &query);
    void draw_field(float x0, float y, bool focused, float dt, bool *animating);
    void draw_row(Row &row, bool focused, float x0, float y, double now, float dt, bool *animating);
    void open_menu(const model::SeerrTitle &t);

    /* A title's page. */
    void open_page(const model::SeerrTitle &t);
    void page_input(const input::State &in);
    void draw_page(float dt, bool *animating);
    enum class PageButton { Request, Jelly5, Close };
    std::vector<PageButton> page_buttons() const;

    /* Setting a request up, then asking before sending it. */
    void open_request();
    void request_input(const input::State &in);
    void draw_request(float dt, bool *animating);
    void choose_server(int index);
    void confirm_request();
    void send_request();
    enum class Line { Quality, Season, Server, Profile, Folder, Send, Cancel };
    struct RequestLine {
        Line line;
        int season = 0;   /* Season: its number */
    };
    std::vector<RequestLine> request_lines() const;
    std::vector<int> requestable_seasons(bool is4k) const;
    bool can_request(bool is4k) const;
    std::vector<model::SeerrServer> servers_for(bool is4k) const;

    Context &m_ctx;
    Row m_results;
    Row m_shelves[5];
    bool m_shelves_set = false;
    std::string m_query;
    bool m_typing = false;
    int m_row = 0;   /* 0: the search field; then rows() */
    Anim m_vscroll;
    Drop m_drop;
    Sheet m_sheet;
    bool m_requested = false;

    /* The page. */
    bool m_page_open = false;
    model::SeerrTitle m_page_title;
    Load<model::SeerrDetail> m_detail;
    Load<model::SeerrPublic> m_public;
    int m_page_focus = 0;
    Anim m_page_show;
    Drop m_page_drop;

    /* The request. */
    bool m_req_open = false;
    Load<std::vector<model::SeerrServer>> m_servers;
    Load<model::SeerrServer> m_server;   /* the chosen one, with its profiles and folders */
    bool m_is4k = false;
    std::vector<int> m_seasons;   /* ticked */
    int m_server_index = 0, m_profile_index = 0, m_folder_index = 0;
    int m_req_focus = 0;
    bool m_sending = false;
    Anim m_req_show;
    Drop m_req_drop;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
