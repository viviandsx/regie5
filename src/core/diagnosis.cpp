/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/diagnosis.h"

#include <algorithm>

namespace diagnosis {

namespace {

const model::Session::Stream *chosen(const model::Session &s, model::Session::Stream::Kind kind, int index)
{
    const model::Session::Stream *first = nullptr;
    for (const auto &st : s.streams) {
        if (st.kind != kind)
            continue;
        if (st.index == index)
            return &st;
        if (!first)
            first = &st;
    }
    /* No subtitle when none is chosen; else the first of its kind. */
    return kind == model::Session::Stream::Subtitle ? nullptr : first;
}

bool has(const model::Session &s, const char *reason)
{
    return std::find(s.transcode_reasons.begin(), s.transcode_reasons.end(), reason) != s.transcode_reasons.end();
}

} // namespace

const model::Session::Stream *video(const model::Session &s) { return chosen(s, model::Session::Stream::Video, -2); }
const model::Session::Stream *audio(const model::Session &s)
{
    return chosen(s, model::Session::Stream::Audio, s.audio_index);
}
const model::Session::Stream *subtitle(const model::Session &s)
{
    return s.subtitle_index < 0 ? nullptr : chosen(s, model::Session::Stream::Subtitle, s.subtitle_index);
}

std::string hw_accel(const model::Session &s, const model::EncodingInfo &enc)
{
    return !s.hw_accel.empty() ? s.hw_accel : enc.hw_accel;
}

Pace pace(const model::Session &s, double prev_lead, const model::EncodingInfo &enc)
{
    Pace p;
    if (s.method != model::Session::Transcode || s.completion < 0 || s.runtime_ticks <= 0)
        return p;
    const double runtime = s.runtime_ticks / 1e7;
    const double played = 100.0 * s.position_ticks / s.runtime_ticks;
    p.lead = (s.completion - played) / 100.0 * runtime;
    if (s.completion >= 99.5) {   /* all done: nothing left to keep up with */
        p.state = Pace::Ahead;
        return p;
    }
    /* Throttled, the transcode is held about this far ahead on purpose. */
    const double held = enc.throttling ? std::max(20, enc.throttle_delay) * 0.8 : 1e9;
    const bool falling = prev_lead >= 0 && p.lead < prev_lead - 2;
    if ((p.lead >= 60 || p.lead >= held) && !(falling && p.lead < 60))
        p.state = Pace::Ahead;
    else if (p.lead >= 20 || s.paused)   /* paused: it can only catch up */
        p.state = Pace::Limit;
    else
        p.state = Pace::Behind;
    return p;
}

std::vector<Advice> advice(const model::Session &s, const model::EncodingInfo &enc, const Pace &p)
{
    std::vector<Advice> out;
    auto add = [&out](Advice a) {
        if (std::find(out.begin(), out.end(), a) == out.end())
            out.push_back(a);
    };
    if (s.method != model::Session::Transcode)
        return out;
    if (has(s, "SubtitleCodecNotSupported")) {
        const model::Session::Stream *sub = subtitle(s);
        add(sub && sub->text ? SubText : SubImage);
    }
    if (has(s, "ContainerBitrateExceedsLimit") || has(s, "VideoBitrateNotSupported") || has(s, "AudioBitrateNotSupported"))
        add(Bitrate);
    if (has(s, "VideoRangeTypeNotSupported"))
        add(Hdr);
    if (has(s, "VideoResolutionNotSupported"))
        add(Resolution);
    for (const char *r : {"VideoCodecNotSupported", "VideoProfileNotSupported", "VideoLevelNotSupported",
                          "VideoBitDepthNotSupported", "VideoFramerateNotSupported", "RefFramesNotSupported",
                          "AnamorphicVideoNotSupported", "InterlacedVideoNotSupported", "VideoCodecTagNotSupported",
                          "VideoRotationNotSupported"})
        if (has(s, r))
            add(VideoCodec);
    for (const char *r : {"AudioCodecNotSupported", "AudioChannelsNotSupported", "AudioProfileNotSupported",
                          "AudioSampleRateNotSupported", "AudioBitDepthNotSupported", "SecondaryAudioNotSupported",
                          "AudioIsExternal"})
        if (has(s, r))
            add(Audio);
    if (has(s, "ContainerNotSupported"))
        add(Container);
    for (const char *r : {"UnknownVideoStreamInfo", "UnknownAudioStreamInfo", "DirectPlayError", "StreamCountExceedsLimit"})
        if (has(s, r))
            add(Generic);
    if (!s.video_direct && hw_accel(s, enc) == "none")
        add(NoHardware);
    if (p.state == Pace::Behind)
        out.insert(out.begin(), Slow);
    return out;
}

} // namespace diagnosis
