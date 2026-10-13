/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bibliothèque: Sonarr's series and Radarr's films as grids of posters, sorted
 * (title, date added, next airing) and filtered (all, monitored, missing,
 * ended or downloaded), a week's guide (ui/screens/guide) and what is missing
 * (wanted/missing). Read-only: no search of any kind, no change. L2/R2 jump to
 * the previous / next letter when sorted by title, else by a page (a week in
 * the guide).
 */
#pragma once

#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/screens/cleanup.h"
#include "ui/screens/guide.h"
#include "ui/screens/library_detail.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

class LibraryScreen : public Screen {
public:
    explicit LibraryScreen(Context &ctx);

    void activate() override;
    Action input(const input::State &in) override;
    void draw(double now, float dt, float ox, bool *animating) override;
    void enter_from_top() override
    {
        m_header = true;
        m_header_focus = slot_of(m_view);
    }
    void go_to(int view, time_t when) override;
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const override;
    bool modal() const override { return m_sheet.is_open() || m_cleanup.modal(); }
    void draw_overlay(float dt, bool *animating) override
    {
        m_sheet.draw(dt, animating);
        m_cleanup.draw_overlay(dt, animating);
    }
    std::vector<PadHint> hints() const override;

    enum View { Series = 0, Movies, Guide, Missing, Cleanup, ViewCount };
    enum Sort { ByTitle = 0, ByAdded, ByNext, SortCount };
    enum Filter { All = 0, Monitored, MissingOnly, EndedOrDownloaded, FilterCount };

    /* A card of the grid (a series or a film), as sorted and filtered. */
    struct Card {
        int index = 0;   /* into the series or the films */
        std::string title, sort_key;
        char letter = '#';
        std::string line;   /* under the title */
        std::string chip;   /* on the poster: missing, not monitored */
        uint32_t chip_dot = 0;
    };
    /* A row of the missing list. */
    struct MissingRow {
        bool tv = true;
        int instance = 0;   /* which Sonarr / Radarr */
        int id = 0;   /* series or film */
        std::string title, line;
        time_t when = 0;
    };

private:
    void load(bool force);
    void rebuild();
    void rebuild_missing();
    std::string profile_name(config::Service s, int instance, int id) const;
    /* The instances whose part failed, "Radarr 4K : Injoignable · …" (empty: none). */
    template <typename T> std::string failed_parts(const std::vector<Part<T>> &parts) const;
    void open_detail();
    void open_media(config::Service s, int instance, int id);
    std::string instance_label(config::Service s, int instance) const;
    void show_series(const model::Series &x);   /* its page */
    void show_movie(const model::Movie &m);
    /* The views shown, in the pills' order (the Ménage only once turned on). */
    std::vector<View> views() const;
    int slot_of(View v) const;   /* its pill's place in the header */
    bool has_sort() const { return m_view == Series || m_view == Movies; }
    /* The pills on the right of the header: sort and filter (the grids), the
     * cleanup's criteria; none for the others. */
    std::vector<std::string> right_pills() const;
    void choose_right(int i);
    /* Options on a series' or a film's page: its menu (empty: nothing to offer). */
    std::vector<std::string> detail_menu() const;
    void open_detail_menu();
    void choose_sort();
    void choose_filter();
    void jump(int dir);   /* L2/R2 */
    void draw_header(float x0, float dt, bool *animating);
    void draw_grid(double now, float x0, float dt, bool *animating);
    void draw_missing(double now, float x0, float dt, bool *animating);

    Context &m_ctx;
    /* Each read from every instance of Sonarr / Radarr (the parts), and seen
     * as one (merge_parts): what the grids and lists show. */
    Load<std::vector<model::Series>> m_series;
    Load<std::vector<model::Movie>> m_movies;
    Load<api::Services::MissingEpisodes> m_missing_eps;
    Load<api::Services::MissingMovies> m_missing_movies;
    std::vector<Part<std::vector<model::Series>>> m_series_parts;
    std::vector<Part<std::vector<model::Movie>>> m_movies_parts;
    std::vector<Part<api::Services::MissingEpisodes>> m_missing_eps_parts;
    std::vector<Part<api::Services::MissingMovies>> m_missing_movies_parts;
    std::vector<Part<std::vector<model::QualityProfile>>> m_profiles;   /* every Sonarr, then every Radarr */

    View m_view = Series;
    Sort m_sort[2] = {ByTitle, ByTitle};      /* per grid */
    Filter m_filter[2] = {All, All};
    std::vector<Card> m_cards;                /* the grid on screen */
    std::vector<MissingRow> m_missing;
    int m_focus[ViewCount] = {};              /* per view */
    bool m_header = true;                     /* the focus is on the pills / sort / filter */
    int m_header_focus = 0;                   /* the views' pills, then sort, then filter */
    Anim m_scroll[ViewCount];
    GuideView m_guide;
    CleanupView m_cleanup;
    Lifts m_lifts;
    Drop m_drop, m_header_drop;
    Sheet m_sheet;
    LibraryDetail m_detail;
    char m_letter = 0;   /* the letter jumped to, shown big for a moment */
    double m_letter_at = -10;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
