/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * "Why does it stutter?": a Jellyfin session read apart from any drawing (and
 * so tested). What to advise for its transcode reasons, and whether the
 * transcode keeps ahead of the playback. Jellyfin exposes neither ffmpeg's
 * speed nor the client's buffer: the pace is an estimate, the transcode's
 * lead over the playback position.
 */
#pragma once

#include "core/model.h"

#include <string>
#include <vector>

namespace diagnosis {

enum Advice {
    Container,     /* the container: a remux would do */
    VideoCodec,    /* the video as it is: another version or client */
    Resolution,    /* too large for the client */
    Hdr,           /* HDR / Dolby Vision turned into SDR */
    Audio,         /* the sound converted */
    SubImage,      /* picture subtitles (PGS...) burnt in */
    SubText,       /* text subtitles converted */
    Bitrate,       /* over the client's limit */
    Generic,       /* Jellyfin could not play it directly */
    NoHardware,    /* transcoding on the CPU */
    Slow,          /* the transcode does not keep up */
};

/* The chosen streams, or null. */
const model::Session::Stream *video(const model::Session &s);
const model::Session::Stream *audio(const model::Session &s);
const model::Session::Stream *subtitle(const model::Session &s);

struct Pace {
    enum State { Unknown, Ahead, Limit, Behind } state = Unknown;
    double lead = 0;   /* seconds the transcode is ahead of the playback */
};
/* prev_lead: the lead at the last reading (< 0: none), to see it fall. */
Pace pace(const model::Session &s, double prev_lead, const model::EncodingInfo &enc);

/* What to advise, most useful first, each once. */
std::vector<Advice> advice(const model::Session &s, const model::EncodingInfo &enc, const Pace &p);

/* The hardware acceleration in use: the session's, else the server's setting. */
std::string hw_accel(const model::Session &s, const model::EncodingInfo &enc);

} // namespace diagnosis
