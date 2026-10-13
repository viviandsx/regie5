/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Demandes › Signalements (Seerr's issues): open, resolved or all, with the
 * title's poster, the kind of problem (video, audio, subtitles), the episode,
 * who reported it and when, and the last thing said. ✕ shows the whole thread
 * (the comments' authors come with the issue alone). Options: Résoudre… and
 * Commenter… (a few ready-made sentences: typing with a pad is no use), each
 * asked again with Annuler first before anything is sent.
 */
#pragma once

#include "core/input.h"
#include "core/model.h"
#include "ui/anim.h"
#include "ui/context.h"
#include "ui/drop.h"
#include "ui/hints.h"
#include "ui/load.h"
#include "ui/screen.h"
#include "ui/seerr_titles.h"
#include "ui/widgets.h"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class IssuesView {
public:
    IssuesView(Context &ctx, SeerrTitles &titles) : m_ctx(ctx), m_titles(titles) {}

    void load(bool force);
    std::vector<std::string> filter_labels() const;
    int filter() const { return m_filter; }
    void set_filter(int f);
    /* Action::ToTabs: up out of the list (to the pills). */
    Action input(const input::State &in);
    void draw(double now, float x0, float top, bool focused, float dt, bool *animating);
    bool backdrop(std::string *key, art::Ref *ref, int *seed) const;
    std::vector<PadHint> hints(bool focused) const;
    /* A sheet of its own is open: it gets every press. */
    bool modal() const { return m_sheet.is_open() || m_thread.is_open(); }
    void draw_overlay(float dt, bool *animating);
    void enter() { m_focus = 0; }

    static constexpr int kFilters = 3;   /* open, resolved, all */

private:
    void load_filter(int f, bool force);
    const model::Issue *focused() const;
    std::string what(const model::Issue &i) const;   /* "Sous-titres · S01E03" */
    void open_thread(const model::Issue &i);
    std::vector<ListSheet::Item> thread_items(const model::Issue &i) const;
    void open_menu(const model::Issue &i);
    void ask_resolve(const model::Issue &i);
    void choose_comment(const model::Issue &i);
    void ask_comment(const model::Issue &i, const std::string &message);
    void done(api::Result<bool> res, const char *ok_text);

    Context &m_ctx;
    SeerrTitles &m_titles;
    Load<model::IssuePage> m_pages[kFilters];
    int m_filter = 0;
    int m_focus = 0;
    int m_thread_id = 0;   /* the issue whose thread is open (its authors on their way) */
    bool m_busy = false;
    Anim m_scroll;
    Drop m_drop;
    Sheet m_sheet;
    ListSheet m_thread;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace ui
