/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/format.h"

#include "core/i18n.h"
#include "core/lang/language.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <set>

namespace fmt {

namespace {

const i18n::Language &L() { return i18n::language(); }

/* A language's date pattern: {name} replaced by its value. */
std::string fill(const char *pattern, std::initializer_list<std::pair<const char *, std::string>> values)
{
    std::string out;
    for (const char *p = pattern; *p;) {
        bool done = false;
        if (*p == '{')
            for (const auto &v : values) {
                const size_t n = std::strlen(v.first);
                if (!std::strncmp(p + 1, v.first, n) && p[n + 1] == '}') {
                    out += v.second;
                    p += n + 2;
                    done = true;
                    break;
                }
            }
        if (!done)
            out += *p++;
    }
    return out;
}

std::string num(int n) { return std::to_string(n); }

/* "5.6" in the language's way ("5,6"). */
std::string decimal(std::string s)
{
    std::replace(s.begin(), s.end(), '.', L().decimal);
    return s;
}

int64_t local_day(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return (int64_t)(tm.tm_year + 1900) * 400 + tm.tm_yday;   /* any order-keeping day number */
}

} // namespace

std::string f(const char *pattern, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, pattern);
    std::vsnprintf(buf, sizeof buf, pattern, ap);
    va_end(ap);
    return buf;
}

std::string day(time_t t, time_t now)
{
    struct tm tm;
    localtime_r(&t, &tm);
    /* Days apart by calendar day, not by 24 hours. */
    struct tm a, b;
    localtime_r(&now, &a);
    localtime_r(&t, &b);
    const bool same_year = a.tm_year == b.tm_year;
    const int diff = same_year ? b.tm_yday - a.tm_yday : (local_day(t) > local_day(now) ? 99 : -99);
    if (diff == 0)
        return i18n::tr(i18n::Today);
    if (diff == 1)
        return i18n::tr(i18n::Tomorrow);
    if (diff == -1)
        return i18n::tr(i18n::Yesterday);
    return fill(L().date_day, {{"wd", L().days[tm.tm_wday]}, {"d", num(tm.tm_mday)}, {"mon", L().months[tm.tm_mon]}});
}

std::string date_full(const struct tm &tm)
{
    return fill(L().date_full, {{"d", num(tm.tm_mday)}, {"mon", L().months[tm.tm_mon]}, {"y", num(tm.tm_year + 1900)}});
}

std::string weekday(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return f("%s %d", L().days[tm.tm_wday], tm.tm_mday);
}

std::string week_range(time_t first_day)
{
    struct tm a, b;
    localtime_r(&first_day, &a);
    const time_t last = first_day + 6 * 86400 + 3600;   /* (a DST change on the way) */
    localtime_r(&last, &b);
    return fill(a.tm_mon == b.tm_mon ? L().week_same : L().week_other,
                {{"d1", num(a.tm_mday)}, {"mon1", L().months[a.tm_mon]}, {"d2", num(b.tm_mday)},
                 {"mon2", L().months[b.tm_mon]}});
}

time_t week_start(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    tm.tm_mday -= (tm.tm_wday + 6) % 7;   /* back to Monday */
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

std::string clock(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return f("%02d:%02d", tm.tm_hour, tm.tm_min);
}

std::string duration(int64_t s)
{
    if (s < 0)
        return "";
    if (s < 60)
        return "< 1 min";
    const int64_t m = s / 60, h = m / 60, d = h / 24;
    if (d > 0)
        return f("%lld %s %lld h", (long long)d, L().day_unit, (long long)(h % 24));
    if (h > 0)
        return f("%lld h %02lld", (long long)h, (long long)(m % 60));
    return f("%lld min", (long long)m);
}

std::string ago(time_t t, time_t now)
{
    const int64_t s = (int64_t)(now - t);
    std::string d;
    if (s < 3600)
        d = f("%lld min", (long long)std::max<int64_t>(1, s / 60));
    else if (s < 86400)
        d = f("%lld h", (long long)(s / 3600));
    else
        d = f("%lld %s", (long long)(s / 86400), L().day_unit);
    return f(i18n::tr(i18n::Ago), d.c_str());
}

std::string size(int64_t bytes)
{
    double v = (double)bytes;
    int u = 0;
    while (v >= 1000 && u < 5)
        v /= 1000, u++;
    return decimal(v < 10 && u > 0 ? f("%.1f", v) : f("%.0f", v)) + " " + L().size_units[u];
}

std::string episode(int season, int ep) { return f("S%02dE%02d", season, ep); }

std::string error(api::Error e, int http_status)
{
    switch (e) {
    case api::Error::Unreachable: return i18n::tr(i18n::ErrUnreachable);
    case api::Error::Timeout: return i18n::tr(i18n::ErrTimeout);
    case api::Error::Unsupported: return i18n::tr(i18n::ErrUnsupported);
    case api::Error::Certificate: return i18n::tr(i18n::ErrCertificate);
    case api::Error::Unauthorized: return i18n::tr(i18n::ErrUnauthorized);
    case api::Error::NotFound: return i18n::tr(i18n::ErrNotFound);
    case api::Error::BadResponse: return i18n::tr(i18n::ErrBadResponse);
    case api::Error::Redirected: return f(i18n::tr(i18n::ErrRedirected), http_status);
    case api::Error::Http: return f(i18n::tr(i18n::ErrHttp), http_status);
    case api::Error::Disabled: return i18n::tr(i18n::StatusDisabled);
    case api::Error::None: break;
    }
    return {};
}

std::string health(const model::Health &h)
{
    /* The kinds and their sentences are in the same order (model.h, i18n.h). */
    static_assert((int)model::Health::KindCount - 1 == (int)i18n::HkUnknown - (int)i18n::HkClientNone,
                  "every health kind has its sentence");
    if (h.kind > model::Health::Unknown && h.kind < model::Health::Announcement)
        return i18n::tr((i18n::S)(i18n::HkClientNone + (h.kind - 1)));
    if (!h.message.empty())
        return h.message;   /* an unknown check or an announcement, and its text names nothing */
    return i18n::tr(h.kind == model::Health::Announcement ? i18n::HkAnnouncement : i18n::HkUnknown);
}

std::string health_level(model::Health::Level l)
{
    return i18n::tr(l == model::Health::Error     ? i18n::HealthLevelError
                    : l == model::Health::Warning ? i18n::HealthLevelWarning
                                                  : i18n::HealthLevelNotice);
}

std::string transcode_reasons(const std::vector<std::string> &reasons)
{
    std::set<int> keys;   /* one phrase per kind, in a stable order */
    for (const std::string &r : reasons) {
        i18n::S k = i18n::TrOther;
        if (r == "ContainerNotSupported")
            k = i18n::TrContainer;
        else if (r == "VideoCodecNotSupported" || r == "VideoCodecTagNotSupported")
            k = i18n::TrVideoCodec;
        else if (r.rfind("Audio", 0) == 0 && r.find("Channels") != std::string::npos)
            k = i18n::TrAudioChannels;
        else if (r.rfind("Audio", 0) == 0 || r == "SecondaryAudioNotSupported")
            k = i18n::TrAudioCodec;
        else if (r == "SubtitleCodecNotSupported")
            k = i18n::TrSubtitles;
        else if (r.find("Bitrate") != std::string::npos)
            k = i18n::TrBitrate;
        else if (r == "VideoResolutionNotSupported")
            k = i18n::TrResolution;
        else if (r == "VideoRangeTypeNotSupported" || r == "VideoBitDepthNotSupported")
            k = i18n::TrHdr;
        else if (r == "VideoProfileNotSupported" || r == "VideoLevelNotSupported")
            k = i18n::TrVideoProfile;
        keys.insert((int)k);
    }
    std::string out;
    for (int k : keys) {
        if (!out.empty())
            out += ", ";
        out += i18n::tr((i18n::S)k);
    }
    return out;
}

std::string labelled(const std::string &who, const std::string &what)
{
    return f(i18n::tr(i18n::LabelPattern), who.c_str(), what.c_str());
}

std::string count(i18n::S one, i18n::S many, int n)
{
    return f(i18n::tr(i18n::singular(n) ? one : many), n);
}

std::string codec(const std::string &name)
{
    static const struct {
        const char *in, *out;
    } kNames[] = {{"hevc", "HEVC"}, {"h265", "HEVC"}, {"h264", "H.264"}, {"avc", "H.264"}, {"av1", "AV1"},
                  {"vp9", "VP9"}, {"vp8", "VP8"}, {"mpeg2video", "MPEG-2"}, {"mpeg4", "MPEG-4"}, {"vc1", "VC-1"},
                  {"truehd", "TrueHD"}, {"eac3", "E-AC-3"}, {"ac3", "AC-3"}, {"dts", "DTS"}, {"aac", "AAC"},
                  {"mp3", "MP3"}, {"flac", "FLAC"}, {"opus", "Opus"}, {"vorbis", "Vorbis"}, {"pcm_s16le", "PCM"},
                  {"pgssub", "PGS"}, {"pgs", "PGS"}, {"dvdsub", "VobSub"}, {"dvd_subtitle", "VobSub"},
                  {"dvbsub", "DVB"}, {"subrip", "SRT"}, {"srt", "SRT"}, {"ass", "ASS"}, {"ssa", "SSA"},
                  {"webvtt", "WebVTT"}, {"mov_text", "MP4 text"}, {"mkv", "MKV"}, {"matroska", "MKV"},
                  {"mp4", "MP4"}, {"ts", "MPEG-TS"}, {"mpegts", "MPEG-TS"}, {"avi", "AVI"}, {"webm", "WebM"}};
    std::string low;
    for (char c : name)
        low += (char)std::tolower((unsigned char)c);
    for (const auto &k : kNames)
        if (low == k.in)
            return k.out;
    std::string up;
    for (char c : name)
        up += (char)std::toupper((unsigned char)c);
    return up;
}

std::string resolution(int height)
{
    if (height <= 0)
        return {};
    if (height >= 2000)
        return "2160p";
    if (height >= 1000)
        return "1080p";
    if (height >= 700)
        return "720p";
    return f("%dp", height);
}

std::string channels(int n)
{
    switch (n) {
    case 0: return {};
    case 1: return "1.0";
    case 2: return "2.0";
    case 6: return "5.1";
    case 8: return "7.1";
    default: return f("%d ch", n);
    }
}

std::string bitrate(int64_t bps)
{
    if (bps <= 0)
        return {};
    return decimal(f("%.1f", bps / 1e6)) + " Mb/s";
}

std::string seconds(double s)
{
    if (s < 0)
        s = 0;
    if (s < 90)
        return f("%d s", (int)(s + 0.5));
    return f("%d min", (int)(s / 60 + 0.5));
}

} // namespace fmt
