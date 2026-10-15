/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/screens/session_diag.h"

#include "core/i18n.h"
#include "ui/format.h"
#include "ui/hints.h"
#include "ui/style.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kW = 1640, kH = 910;
constexpr float kX = (gfx::W - kW) / 2, kY = (gfx::H - kH) / 2 + 24;
constexpr float kButtonH = 62;

uint32_t level_color(double pct) { return pct >= 90 ? kRed : pct >= 70 ? kOrange : kGreen; }

std::string join(std::initializer_list<std::string> parts)
{
    std::string out;
    for (const auto &p : parts)
        if (!p.empty())
            out += (out.empty() ? "" : " · ") + p;
    return out;
}

std::string describe(const model::Session::Stream *st)
{
    if (!st)
        return i18n::tr(i18n::DiagNone);
    switch (st->kind) {
    case model::Session::Stream::Video:
        return join({fmt::codec(st->codec) + (st->profile.empty() ? "" : " " + st->profile), fmt::resolution(st->height),
                     st->range == "SDR" ? "" : st->range, st->bit_depth > 8 ? fmt::f("%d bits", st->bit_depth) : "",
                     fmt::bitrate(st->bitrate)});
    case model::Session::Stream::Audio:
        return join({fmt::codec(st->codec), fmt::channels(st->channels), st->language});
    case model::Session::Stream::Subtitle:
        return join({fmt::codec(st->codec), i18n::tr(st->text ? i18n::DiagTextSub : i18n::DiagImageSub), st->language});
    default: return {};
    }
}

std::string advice_text(diagnosis::Advice a, const model::Session &s)
{
    const model::Session::Stream *v = diagnosis::video(s), *au = diagnosis::audio(s), *sub = diagnosis::subtitle(s);
    switch (a) {
    case diagnosis::Container: return fmt::f(i18n::tr(i18n::AdvContainer), fmt::codec(s.container_in).c_str());
    case diagnosis::VideoCodec:
        return fmt::f(i18n::tr(i18n::AdvVideoCodec), v ? fmt::codec(v->codec).c_str() : "?");
    case diagnosis::Resolution: return i18n::tr(i18n::AdvResolution);
    case diagnosis::Hdr: return i18n::tr(i18n::AdvHdr);
    case diagnosis::Audio: return fmt::f(i18n::tr(i18n::AdvAudio), au ? fmt::codec(au->codec).c_str() : "?");
    case diagnosis::SubImage: return fmt::f(i18n::tr(i18n::AdvSubImage), sub ? fmt::codec(sub->codec).c_str() : "?");
    case diagnosis::SubText: return fmt::f(i18n::tr(i18n::AdvSubText), sub ? fmt::codec(sub->codec).c_str() : "?");
    case diagnosis::Bitrate: return i18n::tr(i18n::AdvBitrate);
    case diagnosis::Generic: return i18n::tr(i18n::AdvGeneric);
    case diagnosis::NoHardware: return i18n::tr(i18n::emby() ? i18n::AdvNoHardwareEmby : i18n::AdvNoHardware);
    case diagnosis::Slow: return i18n::tr(i18n::AdvSlow);
    }
    return {};
}

} // namespace

float draw_server_load(float right, float y, const model::ServerLoad &l)
{
    const struct {
        i18n::S label;
        double v;
    } items[3] = {{i18n::LoadLoad, l.load}, {i18n::LoadMem, l.mem}, {i18n::LoadCpu, l.cpu}};
    float x = right;
    for (const auto &it : items) {
        const std::string t = fmt::f(i18n::tr(it.label), (int)std::lround(it.v));
        x -= chip_width(t, 22);
        draw_chip(x, y, t, level_color(it.v), 1.f, 22);
        x -= 12;
    }
    return right - x;
}

void SessionDiag::open(const std::string &session_id)
{
    m_open = true;
    m_id = session_id;
    m_focus = 1;
    m_show.snap(0.f);
    m_show.to(1.f);
}

SessionDiag::Result SessionDiag::input(const input::State &in)
{
    const uint32_t p = in.pressed;
    if (p & input::Circle) {
        m_open = false;
        return Close;
    }
    if (p & input::Left)
        m_focus = 0;
    else if (p & input::Right)
        m_focus = 1;
    else if (p & input::Cross) {
        m_open = false;
        return m_focus == 0 ? Stop : Close;
    }
    return None;
}

void SessionDiag::draw(const model::Session &s, const model::EncodingInfo &enc, const diagnosis::Pace &pace,
                       const Load<model::ServerLoad> *load, float dt, bool *animating)
{
    if (!m_open)
        return;
    if (m_show.step(dt, 12.f))
        *animating = true;
    const float a = smoothstep(m_show.value);
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(kBg, 0.55f * a));
    gfx::push_opacity(a);
    const float y0 = kY + (1.f - a) * 30;
    const gfx::Rect panel{kX, y0, kW, kH};
    glass_panel(panel, 36, 1.f, true);
    const float x = kX + 56, w = kW - 112;

    /* What, who, and how it plays. */
    const std::string title = s.series_name.empty() ? s.item_name : s.series_name;
    gfx::text(x, y0 + 84, title, {gfx::Bold, 44, w - 420}, kText);
    const std::string what = s.episode > 0 ? fmt::episode(s.season, s.episode) + " · " + s.item_name : "";
    gfx::text(x, y0 + 124, join({what, s.user, s.device, s.client}), {gfx::Regular, 24, w - 420}, kText2);
    const bool tr = s.method == model::Session::Transcode;
    const char *method = i18n::tr(tr ? i18n::Transcode : s.method == model::Session::DirectStream ? i18n::DirectStream
                                                                                                : i18n::DirectPlay);
    float cx = x + w - chip_width(method, 24);
    draw_chip(cx, y0 + 52, method, tr ? kOrange : kGreen, 1.f, 24);
    if (tr && pace.state == diagnosis::Pace::Behind) {
        const char *c = i18n::tr(i18n::PaceBehindChip);
        cx -= chip_width(c, 24) + 12;
        draw_chip(cx, y0 + 52, c, kRed, 1.f, 24);
    } else if (tr && pace.state == diagnosis::Pace::Limit) {
        const char *c = i18n::tr(i18n::PaceLimitChip);
        cx -= chip_width(c, 24) + 12;
        draw_chip(cx, y0 + 52, c, kOrange, 1.f, 24);
    }

    float y = y0 + 176;
    const gfx::TextStyle head{gfx::SemiBold, 28}, cell{gfx::Regular, 24};
    /* Why. */
    if (tr) {
        gfx::text(x, y, i18n::tr(i18n::DiagWhy), head, kText);
        gfx::text(x + 420, y, s.transcode_reasons.empty() ? "?" : fmt::transcode_reasons(s.transcode_reasons),
                  {gfx::Medium, 24, w - 420}, kText2);
    } else {
        gfx::text(x, y, i18n::tr(s.method == model::Session::DirectStream ? i18n::DiagStreamNote : i18n::DiagDirectNote),
                  {gfx::Medium, 26, w}, kText2);
    }

    /* Source against what is sent. */
    y += 52;
    const float c1 = x + 260, c2 = x + 260 + (w - 260) / 2, cw = (w - 260) / 2 - 30;
    gfx::text(c1, y, i18n::tr(i18n::DiagSource), {gfx::SemiBold, 22}, kText3);
    gfx::text(c2, y, i18n::tr(i18n::DiagOutput), {gfx::SemiBold, 22}, kText3);
    const model::Session::Stream *v = diagnosis::video(s), *au = diagnosis::audio(s), *sub = diagnosis::subtitle(s);
    const std::string copy = i18n::tr(i18n::DiagCopy);
    const std::string v_out = !tr || s.video_direct ? copy
                                                    : join({fmt::codec(s.video_codec), fmt::resolution(s.height),
                                                            fmt::bitrate(s.bitrate)});
    const std::string a_out = !tr || s.audio_direct ? copy : join({fmt::codec(s.audio_codec), fmt::channels(s.audio_channels)});
    const bool burnt = tr && sub && !sub->text &&
                       std::find(s.transcode_reasons.begin(), s.transcode_reasons.end(), "SubtitleCodecNotSupported") !=
                           s.transcode_reasons.end();
    const std::string s_out = !sub ? i18n::tr(i18n::DiagNone) : burnt ? i18n::tr(i18n::DiagBurnIn) : describe(sub);
    const struct {
        i18n::S label;
        std::string in, out;
    } rows[4] = {{i18n::DiagVideo, describe(v), v_out},
                 {i18n::DiagAudio, describe(au), a_out},
                 {i18n::DiagSubs, sub ? describe(sub) : std::string(i18n::tr(i18n::DiagNone)), s_out},
                 {i18n::DiagContainer, fmt::codec(s.container_in), tr ? fmt::codec(s.container_out) : copy}};
    for (const auto &r : rows) {
        y += 36;
        gfx::fill({x, y - 30, w, 1}, alpha(0xffffffffu, 0.07f));
        gfx::text(x, y, i18n::tr(r.label), {gfx::Medium, 24}, kText2);
        gfx::text(c1, y, r.in.empty() ? "?" : r.in, {cell.weight, cell.size, cw}, kText);
        gfx::text(c2, y, r.out.empty() ? "?" : r.out, {cell.weight, cell.size, cw}, r.out == copy ? kText3 : kText);
    }

    /* The hardware, and whether it keeps up. */
    y += 54;
    const std::string hw = diagnosis::hw_accel(s, enc);
    gfx::text(x, y, i18n::tr(i18n::DiagHw), head, kText);
    gfx::text(x + 420, y,
              hw.empty() ? std::string(i18n::tr(i18n::DiagHwUnknown))
              : hw == "none" ? std::string(i18n::tr(i18n::DiagHwNone))
                             : fmt::codec(hw),
              {gfx::Medium, 26, w - 420}, hw == "none" && tr ? kOrange : kText2);
    if (tr) {
        y += 46;
        gfx::text(x, y, i18n::tr(i18n::DiagPace), head, kText);
        std::string verdict;
        uint32_t ink = kText2;
        switch (pace.state) {
        case diagnosis::Pace::Ahead:
            verdict = fmt::f(i18n::tr(i18n::DiagAhead), fmt::seconds(pace.lead).c_str()), ink = kGreen;
            break;
        case diagnosis::Pace::Limit:
            verdict = fmt::f(i18n::tr(i18n::DiagLimit), fmt::seconds(pace.lead).c_str()), ink = kOrange;
            break;
        case diagnosis::Pace::Behind:
            verdict = fmt::f(i18n::tr(i18n::DiagBehind), fmt::seconds(pace.lead).c_str()), ink = 0xffff8a80u;
            break;
        case diagnosis::Pace::Unknown: verdict = i18n::tr(i18n::DiagPaceUnknown); break;
        }
        gfx::text(x + 420, y, verdict, {gfx::SemiBold, 26, w - 420}, ink);
        if (pace.state != diagnosis::Pace::Unknown && s.runtime_ticks > 0) {
            /* The playback and, behind it, how far the transcode has gone. */
            const gfx::Rect bar{x + 420, y + 18, w - 420, 10};
            draw_progress(bar, (float)std::min(1.0, s.completion / 100.0), alpha(kText, 0.35f));
            draw_progress(bar, (float)((double)s.position_ticks / s.runtime_ticks), ink);
            gfx::text(x + 420, y + 52, i18n::tr(i18n::DiagPaceNote), {gfx::Regular, 20, w - 420}, kText3);
            y += 34;
        }
    }

    /* What to do. */
    const std::vector<diagnosis::Advice> adv = diagnosis::advice(s, enc, pace);
    if (!adv.empty()) {
        y += 54;
        gfx::text(x, y, i18n::tr(i18n::DiagAdvice), head, kText);
        /* Each on up to two lines; as many as fit above the buttons. */
        const float limit = y0 + kH - kButtonH - 60;
        for (size_t i = 0; i < adv.size() && y + 30 < limit; i++) {
            gfx::fill({x + 424, y - 11, 8, 8}, adv[i] == diagnosis::Slow ? kRed : kOrange, 4);
            gfx::text(x + 446, y, advice_text(adv[i], s), {gfx::Regular, 22, w - 446, 2, 27}, kText);
            y += gfx::text_width(advice_text(adv[i], s), {gfx::Regular, 22}) > w - 446 ? 62 : 36;
        }
    }

    /* Stop (it still asks) / Close, at the right. */
    const float by = y0 + kH - kButtonH - 40;
    const std::string labels[2] = {i18n::tr(i18n::DiagStop), i18n::tr(i18n::Close)};
    float bx = x + w;
    gfx::Rect rects[2];
    for (int i = 1; i >= 0; i--) {
        const float bw = gfx::text_width(labels[i], {gfx::SemiBold, 28}) + 80;
        bx -= bw;
        rects[i] = {bx, by, bw, kButtonH};
        bx -= 16;
    }

    /* The server, when Glances is there to say: its chips end before the
     * buttons, however long the language makes them. */
    if (load && load->state != load::Off) {
        gfx::text(x, by + 42, i18n::tr(i18n::DiagServer), head, kText);
        if (load->ready())
            draw_server_load(std::min(x + 420 + 3 * 190, rects[0].x - 40), by + 22, load->value);
        else if (load->state == load::Failed)
            gfx::text(x + 420, by + 42, i18n::tr(i18n::DiagLoadNA), {gfx::Regular, 24, rects[0].x - 40 - x - 420}, kText3);
    }
    glass_panel({rects[0].x - 6, by - 6, rects[1].x + rects[1].w - rects[0].x + 12, kButtonH + 12}, (kButtonH + 12) / 2,
                1.f, false);
    m_drop.to(rects[m_focus], m_focus);
    m_drop.draw(dt, 1.f, animating);
    for (int i = 0; i < 2; i++)
        gfx::text(rects[i].x + rects[i].w / 2, by + 41, labels[i], {i == m_focus ? gfx::Bold : gfx::SemiBold, 28},
                  i == 0 ? 0xffff8a80u : (i == m_focus ? kText : kText2), 1);
    gfx::pop_opacity();
}

} // namespace ui
