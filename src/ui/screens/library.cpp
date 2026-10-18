/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/library.h"

#include "api/arr.h"
#include "core/i18n.h"
#include "platform/platform.h"
#include "ui/art.h"
#include "ui/format.h"
#include "ui/poster.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kHeaderY = 140, kPillH = 60, kBarPad = 6;
constexpr float kGridTop = 300;
constexpr int kCols = 8;
constexpr float kCardW = 180, kCardH = 270, kRowH = 360;
constexpr float kGridW = gfx::W - 2 * kPad;
constexpr float kGapX = (kGridW - kCols * kCardW) / (kCols - 1);
constexpr float kListRowH = 120, kListGap = 12;
const gfx::TextStyle kPillText{gfx::SemiBold, 26};

const i18n::S kViewLabels[LibraryScreen::ViewCount] = {i18n::ViewSeries, i18n::ViewMovies, i18n::ViewGuide,
                                                       i18n::ViewMissing, i18n::ViewCleanup};
const i18n::S kSortLabels[3] = {i18n::SortTitle, i18n::SortAdded, i18n::SortNext};

i18n::S filter_label(int view, int f)
{
    switch (f) {
    case LibraryScreen::Monitored: return i18n::LibMonitored;
    case LibraryScreen::MissingOnly: return i18n::LibMissing;
    case LibraryScreen::EndedOrDownloaded: return view == LibraryScreen::Series ? i18n::LibEnded : i18n::LibDownloaded;
    default: return i18n::LibAll;
    }
}

float pill_w(const std::string &s) { return gfx::text_width(s, kPillText) + 64; }

/* For sorting and for the letter jump: lower case, accents off the common
 * Latin letters ("Été" sorts with "ete", under E). */
std::string fold(const std::string &s)
{
    static const struct {
        const char *from;
        char to;
    } kMap[] = {{"à", 'a'}, {"á", 'a'}, {"â", 'a'}, {"ä", 'a'}, {"ã", 'a'}, {"å", 'a'}, {"ç", 'c'}, {"è", 'e'},
                {"é", 'e'}, {"ê", 'e'}, {"ë", 'e'}, {"ì", 'i'}, {"í", 'i'}, {"î", 'i'}, {"ï", 'i'}, {"ñ", 'n'},
                {"ò", 'o'}, {"ó", 'o'}, {"ô", 'o'}, {"ö", 'o'}, {"õ", 'o'}, {"ù", 'u'}, {"ú", 'u'}, {"û", 'u'},
                {"ü", 'u'}, {"ý", 'y'}, {"ÿ", 'y'}, {"À", 'a'}, {"É", 'e'}, {"È", 'e'}, {"Ê", 'e'}, {"Î", 'i'},
                {"Ô", 'o'}, {"Ç", 'c'}};
    std::string out;
    for (size_t i = 0; i < s.size();) {
        bool done = false;
        if ((unsigned char)s[i] >= 0x80)
            for (const auto &m : kMap) {
                const size_t n = std::char_traits<char>::length(m.from);
                if (s.compare(i, n, m.from) == 0) {
                    out += m.to;
                    i += n;
                    done = true;
                    break;
                }
            }
        if (!done)
            out += (char)std::tolower((unsigned char)s[i++]);
    }
    return out;
}

char letter_of(const std::string &folded)
{
    for (char c : folded) {
        if (c >= 'a' && c <= 'z')
            return (char)(c - 'a' + 'A');
        if (c != ' ' && c != '\'' && c != '"' && c != '(' && c != '.')
            return '#';
    }
    return '#';
}

/* "12 shown of 53", "1 affiché sur 53". */
std::string shown_of(int n, int total)
{
    return fmt::f(i18n::tr(i18n::singular(n) ? i18n::ShownOfOne : i18n::ShownOf), n, total);
}

/* The poster's decoded width for a card (as draw_poster_card asks for it). */
int poster_px(float w) { return (int)std::lround(w * gfx::scale() * 1.1f / 40) * 40; }

} // namespace

LibraryScreen::LibraryScreen(Context &ctx) : m_ctx(ctx), m_guide(ctx), m_cleanup(ctx) {}

std::vector<LibraryScreen::View> LibraryScreen::views() const
{
    std::vector<View> v = {Series, Movies, Guide, Missing};
    if (m_ctx.cleanup_enabled())
        v.push_back(Cleanup);
    return v;
}

std::vector<std::string> LibraryScreen::right_pills() const
{
    if (has_sort()) {
        const int g = m_view == Series ? 0 : 1;
        return {fmt::f(i18n::tr(i18n::SortLabel), i18n::tr(kSortLabels[m_sort[g]])),
                fmt::f(i18n::tr(i18n::FilterLabel), i18n::tr(filter_label(m_view, m_filter[g])))};
    }
    if (m_view == Cleanup)
        return m_cleanup.pills();
    return {};
}

void LibraryScreen::choose_right(int i)
{
    if (has_sort())
        i == 0 ? choose_sort() : choose_filter();
    else if (m_view == Cleanup)
        m_cleanup.choose(i);
}

int LibraryScreen::slot_of(View v) const
{
    const std::vector<View> vs = views();
    for (int i = 0; i < (int)vs.size(); i++)
        if (vs[i] == v)
            return i;
    return 0;
}

void LibraryScreen::go_to(int view, time_t when)
{
    if (view < 0 || view >= ViewCount)
        return;
    m_detail.close();
    m_view = (View)view;
    m_header = false;
    if (m_view == Guide && when > 0)
        m_guide.show_week_of(when);
    load(false);
    rebuild();
}

void LibraryScreen::open_media(config::Service s, int instance, int id)
{
    if (s == config::Sonarr && m_series.ready()) {
        for (const auto &x : series_list())
            if (x.id == id && x.instance == instance)
                show_series(x);
    } else if (s == config::Radarr && m_movies.ready()) {
        for (const auto &m : movies_list())
            if (m.id == id && m.instance == instance)
                show_movie(m);
    }
}

/* Which instance it is from, said only when there are several. */
std::string LibraryScreen::instance_label(config::Service s, int instance) const
{
    return m_ctx.services && m_ctx.services->instances(s) > 1 ? m_ctx.services->config().at(s, instance).name : "";
}

void LibraryScreen::show_series(const model::Series &x)
{
    m_detail.open_series(x, profile_name(config::Sonarr, x.instance, x.quality_profile), m_ctx.services,
                         instance_label(config::Sonarr, x.instance));
}

void LibraryScreen::show_movie(const model::Movie &m)
{
    m_detail.open_movie(m, profile_name(config::Radarr, m.instance, m.quality_profile),
                        instance_label(config::Radarr, m.instance));
}

template <typename T> std::string LibraryScreen::failed_parts(const std::vector<Part<T>> &parts) const
{
    std::string out;
    for (const auto &p : parts)
        if (p.load.state == load::Failed)
            out += (out.empty() ? "" : " · ") +
                   fmt::labelled(m_ctx.services->config().at(p.service, p.instance).name,
                                 fmt::error(p.load.error, p.load.http_status));
    return out;
}

void LibraryScreen::sync_lists()
{
    if (!m_ctx.lists || m_ctx.lists->version() == m_lists_seen)
        return;
    m_lists_seen = m_ctx.lists->version();
    join_shared(m_ctx.lists->series(), &m_series);
    join_shared(m_ctx.lists->movies(), &m_movies);
    rebuild();
    rebuild_missing();
}

void LibraryScreen::activate()
{
    load(false);
    m_ctx.jelly5.prepare(m_ctx.services);
}

std::vector<std::string> LibraryScreen::detail_menu() const
{
    std::vector<std::string> items;
    const Jelly5Link &j = m_ctx.jelly5;
    if (j.available(m_ctx.services) &&
        (m_detail.is_series() ? j.may_have(m_detail.series()) : j.may_have(m_detail.movie())))
        items.push_back(j.label(m_ctx.services));
    return items;
}

void LibraryScreen::open_detail_menu()
{
    std::vector<std::string> items = detail_menu();
    if (items.empty())
        return;
    const int cancel = (int)items.size();
    items.push_back(i18n::tr(i18n::Cancel));
    const bool tv = m_detail.is_series();
    m_sheet.open(tv ? m_detail.series().title : m_detail.movie().title, "", items, cancel, [this, tv, cancel](int c) {
        if (c < 0 || c >= cancel || !m_detail.is_open())
            return;
        if (tv)
            m_ctx.jelly5.open(m_ctx.services, m_detail.series());
        else
            m_ctx.jelly5.open(m_ctx.services, m_detail.movie());
    });
}

void LibraryScreen::load(bool force)
{
    if (!m_ctx.services)
        return;
    std::weak_ptr<bool> alive = m_alive;
    api::Services &s = *m_ctx.services;
    /* Never asked yet, or asked again with △ (a failure waits for △: no retry every frame). */
    auto stale = [force](const auto &l) { return force || l.state == load::Idle; };
    /* Every instance's part, asked again unless on its way; a ready part stays
     * shown meanwhile. ask(k, done) asks part k. */
    auto fetch = [&](auto &parts, auto *merged, config::Service svc, auto ask) {
        if (parts.empty())
            for (int i = 0; i < s.instances(svc); i++)
                parts.push_back({svc, i, {}});
        for (size_t k = 0; k < parts.size(); k++) {
            if (parts[k].load.loading())
                continue;
            if (!parts[k].load.ready())
                parts[k].load.state = load::Loading;
            ask(k, parts[k].instance);
        }
        if (!merged->ready())
            merged->state = load::Loading;
    };
    if (m_ctx.lists) {   /* shared with the cleanup and the Jellyfin tab: asked once */
        m_ctx.lists->want_series(force);
        m_ctx.lists->want_movies(force);
        sync_lists();
    }
    if (m_profiles.empty())
        for (config::Service svc : {config::Sonarr, config::Radarr})
            for (int i = 0; i < s.instances(svc); i++)
                m_profiles.push_back({svc, i, {}});
    for (size_t k = 0; k < m_profiles.size(); k++)
        if (stale(m_profiles[k].load) && !m_profiles[k].load.loading()) {
            m_profiles[k].load.state = load::Loading;
            s.quality_profiles(
                m_profiles[k].service,
                [this, alive, k](api::Result<std::vector<model::QualityProfile>> r) {
                    if (!alive.expired() && k < m_profiles.size())
                        m_profiles[k].load.take(std::move(r), platform::now());
                },
                m_profiles[k].instance);
        }
    if (m_view == Cleanup && !m_ctx.cleanup_enabled())
        m_view = Series;   /* turned off in Settings meanwhile */
    if (m_view == Guide)
        m_guide.load(force);
    if (m_view == Cleanup)
        m_cleanup.load(force);
    if (m_view == Missing) {
        if (stale(m_missing_eps) && !m_missing_eps.loading())
            fetch(m_missing_eps_parts, &m_missing_eps, config::Sonarr, [&](size_t k, int inst) {
                s.missing_episodes(
                    [this, alive, k](api::Result<api::Services::MissingEpisodes> r) {
                        if (alive.expired() || k >= m_missing_eps_parts.size())
                            return;
                        m_missing_eps_parts[k].load.take(std::move(r), platform::now());
                        merge_parts(m_missing_eps_parts, &m_missing_eps,
                                    [](api::Services::MissingEpisodes *m, const api::Services::MissingEpisodes &p) {
                                        m->items.insert(m->items.end(), p.items.begin(), p.items.end());
                                        m->total += p.total;
                                    });
                        rebuild_missing();
                    },
                    inst);
            });
        if (stale(m_missing_movies) && !m_missing_movies.loading())
            fetch(m_missing_movies_parts, &m_missing_movies, config::Radarr, [&](size_t k, int inst) {
                s.missing_movies(
                    [this, alive, k](api::Result<api::Services::MissingMovies> r) {
                        if (alive.expired() || k >= m_missing_movies_parts.size())
                            return;
                        m_missing_movies_parts[k].load.take(std::move(r), platform::now());
                        merge_parts(m_missing_movies_parts, &m_missing_movies,
                                    [](api::Services::MissingMovies *m, const api::Services::MissingMovies &p) {
                                        m->items.insert(m->items.end(), p.items.begin(), p.items.end());
                                        m->total += p.total;
                                    });
                        rebuild_missing();
                    },
                    inst);
            });
    }
}

std::string LibraryScreen::profile_name(config::Service s, int instance, int id) const
{
    for (const auto &p : m_profiles)
        if (p.service == s && p.instance == instance && p.load.ready())
            for (const auto &q : p.load.value)
                if (q.id == id)
                    return q.name;
    return {};
}

void LibraryScreen::rebuild()
{
    if (!has_sort())
        return;
    const bool tv = m_view == Series;
    const int g = tv ? 0 : 1;
    const std::string focused = m_focus[g] < (int)m_cards.size() ? m_cards[m_focus[g]].title : "";
    m_cards.clear();
    const time_t now = time(nullptr);
    struct Item {
        Card card;
        time_t added = 0, next = 0;
    };
    std::vector<Item> items;
    if (tv && m_series.ready()) {
        for (int i = 0; i < (int)series_list().size(); i++) {
            const model::Series &s = series_list()[i];
            const Filter f = m_filter[0];
            if ((f == Monitored && !s.monitored) || (f == MissingOnly && s.missing() == 0) ||
                (f == EndedOrDownloaded && s.status != model::Series::Ended))
                continue;
            Item it;
            it.card.index = i;
            it.card.title = s.title;
            it.card.sort_key = fold(s.sort_title);
            it.card.letter = letter_of(it.card.sort_key);
            it.added = s.added;
            it.next = s.next_airing;
            int seasons = 0;
            for (const auto &ss : s.seasons)
                seasons += ss.number > 0;
            const std::string status = i18n::tr(s.status == model::Series::Ended      ? i18n::SeEnded
                                                : s.status == model::Series::Upcoming ? i18n::SeUpcoming
                                                                                      : i18n::SeContinuing);
            it.card.line = m_sort[0] == ByNext && s.next_airing
                               ? fmt::day(s.next_airing, now)
                               : (seasons == 1 ? std::string(i18n::tr(i18n::SeasonsOne))
                                               : fmt::f(i18n::tr(i18n::SeasonsCount), seasons)) +
                                     " · " + status;
            const std::string where = instance_label(config::Sonarr, s.instance);
            if (!where.empty())   /* several Sonarr: which one */
                it.card.line = where + " · " + it.card.line;
            if (!s.monitored)
                it.card.chip = i18n::tr(i18n::Unmonitored), it.card.chip_dot = 0xff8e8e93u;
            else if (s.missing() > 0)
                it.card.chip = s.missing() == 1 ? std::string(i18n::tr(i18n::MissingOne))
                                                : fmt::f(i18n::tr(i18n::MissingCount), s.missing()),
                it.card.chip_dot = kOrange;
            items.push_back(std::move(it));
        }
    } else if (!tv && m_movies.ready()) {
        for (int i = 0; i < (int)movies_list().size(); i++) {
            const model::Movie &m = movies_list()[i];
            const Filter f = m_filter[1];
            const bool missing = m.monitored && !m.has_file;
            if ((f == Monitored && !m.monitored) || (f == MissingOnly && !missing) ||
                (f == EndedOrDownloaded && !m.has_file))
                continue;
            Item it;
            it.card.index = i;
            it.card.title = m.title;
            it.card.sort_key = fold(m.sort_title);
            it.card.letter = letter_of(it.card.sort_key);
            it.added = m.added;
            it.next = m.next_release(now);
            if (m_sort[1] == ByNext && it.next)
                it.card.line = fmt::day(it.next, now);
            else
                it.card.line = m.year > 0 ? std::to_string(m.year) : "";
            const std::string where = instance_label(config::Radarr, m.instance);
            if (!where.empty())   /* several Radarr: which one */
                it.card.line = it.card.line.empty() ? where : where + " · " + it.card.line;
            if (!m.monitored)
                it.card.chip = i18n::tr(i18n::Unmonitored), it.card.chip_dot = 0xff8e8e93u;
            else if (!m.has_file)
                it.card.chip = i18n::tr(i18n::NoFile), it.card.chip_dot = kOrange;
            items.push_back(std::move(it));
        }
    }
    const Sort sort = m_sort[g];
    std::stable_sort(items.begin(), items.end(), [sort](const Item &a, const Item &b) {
        if (sort == ByAdded && a.added != b.added)
            return a.added > b.added;   /* newest first */
        if (sort == ByNext && a.next != b.next) {
            if (!a.next || !b.next)
                return a.next != 0;   /* nothing to come: last */
            return a.next < b.next;
        }
        return a.card.sort_key < b.card.sort_key;
    });
    m_focus[g] = 0;
    for (auto &it : items) {
        if (it.card.title == focused)
            m_focus[g] = (int)m_cards.size();
        m_cards.push_back(std::move(it.card));
    }
}

void LibraryScreen::rebuild_missing()
{
    m_missing.clear();
    const time_t now = time(nullptr);
    if (m_missing_eps.ready())
        for (const auto &e : m_missing_eps.value.items) {
            MissingRow r;
            r.tv = true;
            r.instance = e.instance;
            r.id = e.series_id;
            r.title = e.series_title;
            r.when = e.aired;
            r.line = fmt::episode(e.season, e.episode) + (e.title.empty() ? "" : " · " + e.title) +
                     (e.aired ? " · " + fmt::f(i18n::tr(i18n::AiredOn), fmt::day(e.aired, now).c_str()) : "");
            const std::string where = instance_label(config::Sonarr, r.instance);
            if (!where.empty())
                r.line = where + " · " + r.line;
            m_missing.push_back(std::move(r));
        }
    if (m_missing_movies.ready())
        for (const auto &m : m_missing_movies.value.items) {
            MissingRow r;
            r.tv = false;
            r.instance = m.instance;
            r.id = m.id;
            r.title = m.title + (m.year > 0 ? " (" + std::to_string(m.year) + ")" : "");
            /* Its latest release so far, else the first one to come. */
            for (time_t t : {m.in_cinemas, m.digital, m.physical})
                if (t && t <= now && t > r.when)
                    r.when = t;
            if (!r.when)
                r.when = m.next_release(now);
            r.line = r.when ? fmt::f(i18n::tr(i18n::ReleasedOn), fmt::day(r.when, now).c_str()) : "";
            const std::string where = instance_label(config::Radarr, r.instance);
            if (!where.empty())
                r.line = r.line.empty() ? where : where + " · " + r.line;
            m_missing.push_back(std::move(r));
        }
    /* What has aired or come out, latest first; then what is still to come, soonest first. */
    std::stable_sort(m_missing.begin(), m_missing.end(), [now](const MissingRow &a, const MissingRow &b) {
        const bool fa = a.when > now, fb = b.when > now;
        if (fa != fb)
            return !fa;
        return fa ? a.when < b.when : a.when > b.when;
    });
    m_focus[Missing] = std::min(m_focus[Missing], std::max(0, (int)m_missing.size() - 1));
}

void LibraryScreen::open_detail()
{
    if (m_view == Missing) {
        if (m_focus[Missing] >= (int)m_missing.size())
            return;
        const MissingRow &r = m_missing[m_focus[Missing]];
        if (r.tv && m_series.ready()) {
            for (const auto &s : series_list())
                if (s.id == r.id && s.instance == r.instance)
                    show_series(s);
        } else if (!r.tv && m_missing_movies.ready()) {
            for (const auto &m : m_missing_movies.value.items)
                if (m.id == r.id && m.instance == r.instance)
                    show_movie(m);
        }
        return;
    }
    const int g = m_view == Series ? 0 : 1;
    if (m_focus[g] >= (int)m_cards.size())
        return;
    const int i = m_cards[m_focus[g]].index;
    if (m_view == Series) {
        const model::Series &x = series_list()[i];
        show_series(x);
    } else {
        const model::Movie &m = movies_list()[i];
        show_movie(m);
    }
}

void LibraryScreen::choose_sort()
{
    const int g = m_view == Series ? 0 : 1;
    std::vector<std::string> opts;
    for (int i = 0; i < SortCount; i++)
        opts.push_back(i18n::tr(kSortLabels[i]));
    m_sheet.open(i18n::tr(i18n::ChooseSort), "", opts, m_sort[g], [this, g](int c) {
        if (c >= 0 && c < SortCount) {
            m_sort[g] = (Sort)c;
            m_scroll[g].snap(0);
            rebuild();
            m_focus[g] = 0;
        }
    });
}

void LibraryScreen::choose_filter()
{
    const int g = m_view == Series ? 0 : 1;
    std::vector<std::string> opts;
    for (int i = 0; i < FilterCount; i++)
        opts.push_back(i18n::tr(filter_label(m_view, i)));
    m_sheet.open(i18n::tr(i18n::ChooseFilter), "", opts, m_filter[g], [this, g](int c) {
        if (c >= 0 && c < FilterCount) {
            m_filter[g] = (Filter)c;
            m_scroll[g].snap(0);
            rebuild();
            m_focus[g] = 0;
        }
    });
}

void LibraryScreen::jump(int dir)
{
    if (m_view == Missing) {
        const int n = (int)m_missing.size();
        m_focus[Missing] = std::clamp(m_focus[Missing] + dir * 6, 0, std::max(0, n - 1));
        return;
    }
    const int g = m_view == Series ? 0 : 1;
    const int n = (int)m_cards.size();
    if (n == 0)
        return;
    int &f = m_focus[g];
    if (m_sort[g] != ByTitle) {   /* a page: two rows */
        f = std::clamp(f + dir * kCols * 2, 0, n - 1);
        return;
    }
    const char cur = m_cards[f].letter;
    if (dir > 0) {
        int i = f;
        while (i < n && m_cards[i].letter == cur)
            i++;
        if (i >= n)
            return;
        f = i;
    } else {
        int i = f;
        while (i > 0 && m_cards[i - 1].letter == cur)
            i--;
        if (i == f && i > 0) {   /* already at the start of this letter: the one before */
            const char prev = m_cards[i - 1].letter;
            while (i > 0 && m_cards[i - 1].letter == prev)
                i--;
        }
        f = i;
    }
    m_letter = m_cards[f].letter;
    m_letter_at = platform::now();
}

std::vector<PadHint> LibraryScreen::hints() const
{
    if (m_detail.is_open())
        return detail_menu().empty() ? std::vector<PadHint>{} : std::vector<PadHint>{{PadButton::Options, i18n::tr(i18n::HintOptions)}};
    if (m_header)
        return {};
    if (m_view == Guide)
        return m_guide.hints();
    if (m_view == Cleanup)
        return m_cleanup.hints();
    const bool letters = m_view != Missing && m_sort[m_view == Series ? 0 : 1] == ByTitle;
    return {{PadButton::L2, ""}, {PadButton::R2, i18n::tr(letters ? i18n::HintLetter : i18n::HintPage)}};
}

Action LibraryScreen::input(const input::State &in)
{
    if (m_sheet.is_open()) {
        m_sheet.input(in);
        return Action::None;
    }
    if (m_cleanup.modal()) {
        m_cleanup.input(in);
        return Action::None;
    }
    if (m_detail.is_open()) {
        if (in.pressed & input::Options)
            open_detail_menu();
        else
            m_detail.input(in);
        return Action::None;
    }
    const uint32_t p = in.pressed;
    if (p & input::Circle)
        return Action::ToTabs;
    if (p & input::Triangle) {
        art::retry_failed();
        load(true);
        return Action::None;
    }
    const std::vector<View> vs = views();
    const int nv = (int)vs.size();
    const int header_n = nv + (int)right_pills().size();
    if (m_header) {
        if (p & input::Up)
            return Action::ToTabs;
        if (p & (input::Left | input::Right)) {
            /* The focus moves; a view opens with ✕ (landing on Manquants must not
             * take Tri and Filtre away on the way to them). */
            const int f = m_header_focus + ((p & input::Left) ? -1 : 1);
            if (f < 0 || f >= header_n)
                return Action::Bump;
            m_header_focus = f;
        } else if (p & input::Cross) {
            if (m_header_focus >= nv) {
                choose_right(m_header_focus - nv);
            } else if (vs[m_header_focus] != m_view) {
                m_view = vs[m_header_focus];
                load(false);
                rebuild();
            } else {
                m_header = false;
            }
        } else if (p & input::Down) {
            m_header = false;
        }
        return Action::None;
    }
    if (m_view == Cleanup) {
        if (m_cleanup.input(in) == Action::ToTabs)
            m_header = true, m_header_focus = slot_of(Cleanup);
        return Action::None;
    }
    if (m_view == Guide) {
        if (m_guide.input(in) == Action::ToTabs)
            m_header = true, m_header_focus = slot_of(Guide);
        config::Service svc;
        int instance, id;
        if (m_guide.take_open(&svc, &instance, &id))
            open_media(svc, instance, id);
        return Action::None;
    }
    if (p & (input::L2 | input::R2)) {
        jump((p & input::R2) ? 1 : -1);
        return Action::None;
    }
    if (p & input::Cross) {
        open_detail();
        return Action::None;
    }
    if (m_view == Missing) {
        int &f = m_focus[Missing];
        const int n = (int)m_missing.size();
        if (p & input::Up) {
            if (f == 0)
                m_header = true, m_header_focus = slot_of(m_view);
            else
                f--;
        } else if (p & input::Down) {
            if (f >= n - 1)
                return Action::Bump;
            f++;
        }
        return Action::None;
    }
    int &f = m_focus[m_view == Series ? 0 : 1];
    const int n = (int)m_cards.size();
    if (p & input::Up) {
        if (f < kCols)
            m_header = true, m_header_focus = slot_of(m_view);
        else
            f -= kCols;
    } else if (p & input::Down) {
        if (f + kCols < n)
            f += kCols;
        else if (f / kCols < (n - 1) / kCols)
            f = n - 1;   /* the last row is short: its last card */
        else
            return Action::Bump;
    } else if (p & input::Left) {
        if (f % kCols == 0)
            return Action::Bump;
        f--;
    } else if (p & input::Right) {
        if (f % kCols == kCols - 1 || f >= n - 1)
            return Action::Bump;
        f++;
    }
    return Action::None;
}

bool LibraryScreen::backdrop(std::string *key, art::Ref *ref, int *seed) const
{
    if (m_detail.is_open())
        return m_detail.backdrop(key, ref, seed);
    if (!m_focused || m_header)
        return false;
    if (m_view == Guide)
        return m_guide.backdrop(key, ref, seed);
    if (m_view == Cleanup)
        return m_cleanup.backdrop(key, ref, seed);
    config::Service s;
    int id, instance;
    std::string title;
    if (m_view == Missing) {
        if (m_focus[Missing] >= (int)m_missing.size())
            return false;
        const MissingRow &r = m_missing[m_focus[Missing]];
        s = r.tv ? config::Sonarr : config::Radarr, id = r.id, instance = r.instance, title = r.title;
    } else {
        const int g = m_view == Series ? 0 : 1;
        if (m_focus[g] >= (int)m_cards.size())
            return false;
        const Card &c = m_cards[m_focus[g]];
        s = m_view == Series ? config::Sonarr : config::Radarr;
        id = m_view == Series ? series_list()[c.index].id : movies_list()[c.index].id;
        instance = m_view == Series ? series_list()[c.index].instance : movies_list()[c.index].instance;
        title = c.title;
    }
    *ref = {s, arr::mediacover_path(id, "fanart-360.jpg"), instance};
    *key = ref->path + std::to_string((int)s) + "." + std::to_string(instance);
    *seed = (int)(seed_of(title) % 997);
    return true;
}

void LibraryScreen::draw_header(float x0, float dt, bool *animating)
{
    /* The views on one pane of pills; sort and filter (the grids only) on the
     * right, each on its own piece of glass. */
    const std::vector<View> vs = views();
    const int nv = (int)vs.size();
    std::vector<std::string> labels;
    for (View v : vs)
        labels.push_back(i18n::tr(kViewLabels[v]));
    const std::vector<std::string> right = right_pills();
    /* A longer language may not fit both groups on the line: every pill's
     * text then gets smaller, alike (never below three quarters). */
    float natural = kBarPad;
    for (const auto &l : labels)
        natural += pill_w(l) + kBarPad;
    for (const auto &l : right)
        natural += pill_w(l) + 2 * kBarPad + 16;
    const float fit = std::clamp((kGridW - 24) / natural, 0.75f, 1.f);
    const gfx::TextStyle pill_text{gfx::SemiBold, 26 * fit}, pill_lit{gfx::Bold, 26 * fit};
    auto fit_w = [&](const std::string &l) { return gfx::text_width(l, pill_text) + 64 * fit; };
    float total = kBarPad;
    for (const auto &l : labels)
        total += fit_w(l) + kBarPad;
    const gfx::Rect pane{x0, kHeaderY, total, kPillH + 2 * kBarPad};
    glass_panel(pane, pane.h / 2, 1.f, false);
    std::vector<gfx::Rect> rects;
    float px = x0 + kBarPad;
    for (const auto &l : labels) {
        const float w = fit_w(l);
        rects.push_back({px, kHeaderY + kBarPad, w, kPillH});
        px += w + kBarPad;
    }
    if (!right.empty()) {
        labels.insert(labels.end(), right.begin(), right.end());
        rects.resize(nv + right.size());
        float rx = x0 + kGridW;
        for (int i = (int)labels.size() - 1; i >= nv; i--) {
            const float w = fit_w(labels[i]);
            rx -= w + 2 * kBarPad;
            const gfx::Rect r{rx, kHeaderY, w + 2 * kBarPad, kPillH + 2 * kBarPad};
            glass_panel(r, r.h / 2, 1.f, false);
            rects[i] = {rx + kBarPad, kHeaderY + kBarPad, w, kPillH};
            rx -= 16;
        }
    }
    /* The drop: on what has the focus in the header, else faint on the open view. */
    const bool here = m_focused && m_header;
    const int open = slot_of(m_view);
    const int on = here ? std::min(m_header_focus, (int)rects.size() - 1) : open;
    m_header_drop.to(rects[on], on);
    m_header_drop.draw(dt, here ? 1.f : 0.35f, animating);
    for (int i = 0; i < (int)rects.size(); i++) {
        const bool lit = i == open || (here && i == m_header_focus);
        gfx::text(rects[i].x + rects[i].w / 2, rects[i].y + 40, labels[i],
                  lit ? pill_lit : pill_text, lit ? kText : kText2, 1);
    }
}

void LibraryScreen::draw_grid(double now, float x0, float dt, bool *animating)
{
    const bool tv = m_view == Series;
    const int g = tv ? 0 : 1;
    const load::State st = tv ? m_series.state : m_movies.state;
    const int total = tv ? (int)series_list().size() : (int)movies_list().size();
    const int n = (int)m_cards.size();

    /* How many, and how they are sorted. */
    if (st == load::Ready) {
        std::string count = tv ? fmt::count(i18n::CountSeriesOne, i18n::CountSeries, n)
                               : fmt::count(i18n::CountMoviesOne, i18n::CountMovies, n);
        if (n != total)
            count = shown_of(n, total);
        const float count_w = gfx::text(x0, 262, count, {gfx::Medium, 24}, kText3);
        /* An instance that failed beside one that answered. */
        const std::string failures = tv ? failed_parts(m_ctx.lists->series()) : failed_parts(m_ctx.lists->movies());
        if (!failures.empty())
            draw_chip(x0 + count_w + 18, 238, failures, kRed, 1.f, 22);
    }
    const gfx::Rect area{x0, kGridTop, kGridW, kCardH};
    if (st == load::Off) {
        draw_empty(area, m_ctx.off_text(tv ? i18n::SonarrOff : i18n::RadarrOff));
        return;
    }
    if (st == load::Failed) {
        draw_empty(area, tv ? failed_parts(m_ctx.lists->series()) : failed_parts(m_ctx.lists->movies()), i18n::tr(i18n::Retry),
                   true);
        return;
    }
    if (st != load::Ready) {
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < kCols; c++)
                draw_skeleton({x0 + c * (kCardW + kGapX), kGridTop + r * kRowH, kCardW, kCardH}, 14, now, animating);
        return;
    }
    if (n == 0) {   /* nothing at all, or nothing this filter keeps */
        draw_empty(area, i18n::tr(total > 0 ? i18n::EmptyGrid : tv ? i18n::LibraryNoSeries : i18n::LibraryNoMovies));
        return;
    }

    int &f = m_focus[g];
    f = std::min(f, n - 1);
    const int rows = (n + kCols - 1) / kCols;
    const float view_h = gfx::H - 110 - kGridTop;
    const float scroll =
        list_scroll(m_scroll[g], (f / kCols) * kRowH, kRowH, view_h, rows * kRowH, dt, animating);
    const config::Service svc = tv ? config::Sonarr : config::Radarr;
    const int px = poster_px(kCardW);

    /* Only the rows on screen are drawn (and so fetched); the one beyond each
     * edge is asked for after them, ready before it scrolls in. */
    const int first = std::max(0, (int)std::floor((scroll - 40) / kRowH));
    const int last = std::min(rows - 1, (int)std::floor((scroll + view_h) / kRowH));
    gfx::push_scissor({0, kGridTop - 22, gfx::W, view_h + 22});   /* under the count line; room for a lift */
    for (int r = first; r <= last; r++)
        for (int c = 0; c < kCols; c++) {
            const int i = r * kCols + c;
            if (i >= n)
                break;
            const Card &card = m_cards[i];
            const int id = tv ? series_list()[card.index].id : movies_list()[card.index].id;
            const int inst = tv ? series_list()[card.index].instance : movies_list()[card.index].instance;
            const float x = x0 + c * (kCardW + kGapX), y = kGridTop + r * kRowH - scroll;
            const bool focused = m_focused && !m_header && i == f;
            const float lift = m_lifts.step(std::to_string(g) + ":" + std::to_string(inst) + ":" + std::to_string(id),
                                            focused, dt, animating);
            draw_poster_card({x, y, kCardW, kCardH}, {svc, arr::mediacover_path(id, "poster-500.jpg"), inst}, card.title,
                             seed_of(card.title), lift, 1.f);
            if (!card.chip.empty())
                draw_chip(x + 8, y + 8 - 12 * lift, card.chip, card.chip_dot, 1.f, 17);
            const float ty = y + kCardH + 34 + 12 * lift;
            gfx::text(x, ty, card.title, {focused ? gfx::SemiBold : gfx::Medium, 23, kCardW + 12},
                      focused ? kText : kText2);
            if (!card.line.empty())
                gfx::text(x, ty + 28, card.line, {gfx::Regular, 20, kCardW + 12}, kText3);
        }
    gfx::pop_scissor();
    for (int r : {first - 1, last + 1}) {
        if (r < 0 || r >= rows)
            continue;
        for (int c = 0; c < kCols && r * kCols + c < n; c++) {
            const Card &card = m_cards[r * kCols + c];
            const int id = tv ? series_list()[card.index].id : movies_list()[card.index].id;
            const int inst = tv ? series_list()[card.index].instance : movies_list()[card.index].instance;
            art::get({svc, arr::mediacover_path(id, "poster-500.jpg"), inst}, px, px * 3 / 2);
        }
    }

    /* The letter just jumped to, big for a moment. */
    const double age = now - m_letter_at;
    if (m_letter && age < 0.9) {
        const float a = (float)std::min(1.0, (0.9 - age) / 0.3);
        const gfx::Rect r{(gfx::W - 200) / 2, (gfx::H - 200) / 2 + 40, 200, 200};
        glass_panel(r, 40, a, true);
        gfx::text(r.x + 100, r.y + 136, std::string(1, m_letter), {gfx::Bold, 110}, alpha(kText, a), 1);
        *animating = true;
    }
}

void LibraryScreen::draw_missing(double now, float x0, float dt, bool *animating)
{
    const bool ready = m_missing_eps.ready() || m_missing_movies.ready();
    const bool loading = m_missing_eps.loading() || m_missing_movies.loading();
    std::string count;
    if (m_missing_eps.ready()) {
        count = fmt::count(i18n::MissingEpisodesOne, i18n::MissingEpisodesCount, m_missing_eps.value.total);
        if ((int)m_missing_eps.value.items.size() < m_missing_eps.value.total)
            count += " (" + shown_of((int)m_missing_eps.value.items.size(), m_missing_eps.value.total) + ")";
    }
    if (m_missing_movies.ready())
        count += (count.empty() ? "" : " · ") + fmt::count(i18n::MissingMoviesOne, i18n::MissingMoviesCount, m_missing_movies.value.total);
    const float count_w = gfx::text(x0, 262, count, {gfx::Medium, 24}, kText3);

    /* A service that failed is said even when the other one answered. */
    std::string failures = failed_parts(m_missing_eps_parts);
    const std::string movies_failed = failed_parts(m_missing_movies_parts);
    if (!movies_failed.empty())
        failures += (failures.empty() ? "" : " · ") + movies_failed;

    const gfx::Rect area{x0, kGridTop, kGridW, 260};
    const int n = (int)m_missing.size();
    if (n > 0 && !failures.empty())
        draw_chip(x0 + count_w + (count_w > 0 ? 18 : 0), 238, failures, kRed, 1.f, 22);
    if (n == 0) {
        if (!ready && loading)
            for (int i = 0; i < 4; i++)
                draw_skeleton({x0, kGridTop + i * (kListRowH + kListGap), kGridW, kListRowH}, 22, now, animating);
        else if (!failures.empty())   /* "nothing missing" would not be true */
            draw_empty(area, failures, i18n::tr(i18n::Retry), true);
        else if (m_missing_eps.state == load::Off && m_missing_movies.state == load::Off)
            draw_empty(area, m_ctx.off_text(i18n::BothArrOff));
        else
            draw_empty(area, i18n::tr(i18n::EmptyMissing));
        return;
    }
    int &f = m_focus[Missing];
    f = std::min(f, n - 1);
    const float view_h = gfx::H - 110 - kGridTop;
    const float list_h = n * (kListRowH + kListGap);
    const float scroll =
        list_scroll(m_scroll[Missing], f * (kListRowH + kListGap), kListRowH, view_h, list_h, dt, animating);
    gfx::push_scissor({0, kGridTop - 20, gfx::W, view_h + 20});
    for (int i = 0; i < n; i++) {
        const float y = kGridTop + i * (kListRowH + kListGap) - scroll;
        if (y + kListRowH >= kGridTop - 20 && y <= gfx::H)
            glass_flat({x0, y, kGridW, kListRowH}, 22, 0.9f);
    }
    if (m_focused && !m_header) {
        m_drop.to({x0, kGridTop + f * (kListRowH + kListGap) - scroll, kGridW, kListRowH}, f, 0, -scroll);
        m_drop.draw(dt, 1.f, animating, 22);
    } else {
        m_drop.hide();
    }
    for (int i = 0; i < n; i++) {
        const MissingRow &r = m_missing[i];
        const float y = kGridTop + i * (kListRowH + kListGap) - scroll;
        if (y + kListRowH < kGridTop - 20 || y > gfx::H)
            continue;
        const config::Service svc = r.tv ? config::Sonarr : config::Radarr;
        art::draw_poster({x0 + 16, y + 12, 64, 96}, {svc, arr::mediacover_path(r.id, "poster-250.jpg")}, r.title,
                         seed_of(r.title), 9, 1.f, 120);
        const std::string kind = i18n::tr(r.tv ? i18n::KindSeries : i18n::KindMovie);
        const float cw = chip_width(kind, 20);
        draw_chip(x0 + kGridW - 24 - cw, y + 18, kind, r.tv ? 0xff0a84ffu : 0xffbf5af2u, 1.f, 20);
        const bool focused = m_focused && !m_header && i == f;
        gfx::text(x0 + 104, y + 50, r.title, {focused ? gfx::Bold : gfx::SemiBold, 30, kGridW - 104 - cw - 60},
                  kText);
        gfx::text(x0 + 104, y + 88, r.line, {gfx::Regular, 24, kGridW - 140}, kText2);
    }
    gfx::pop_scissor();
}

void LibraryScreen::draw(double now, float dt, float ox, bool *animating)
{
    load(false);
    if (m_detail.is_open()) {
        m_detail.draw(now, dt, ox, m_focused, animating);
        if (art::busy())
            *animating = true;
        return;
    }
    const float x0 = kPad + ox;
    draw_header(x0, dt, animating);
    if (m_view == Missing)
        draw_missing(now, x0, dt, animating);
    else if (m_view == Guide)
        m_guide.draw(now, x0, m_focused && !m_header, dt, animating);
    else if (m_view == Cleanup)
        m_cleanup.draw(now, x0, m_focused && !m_header, dt, animating);
    else if (has_sort())
        draw_grid(now, x0, dt, animating);
    if (art::busy())
        *animating = true;
}

} // namespace ui
