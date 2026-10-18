/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/plex.h"

#include "api/json.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace plex {

namespace {

/* A header's value: no line break, nothing outside printable ASCII. */
std::string header_safe(const std::string &s)
{
    std::string out;
    for (char c : s)
        if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f)
            out += c;
    return out;
}

/* A query value, percent-encoded. */
std::string encode(const std::string &s)
{
    static const char *const kHex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else
            out += std::string("%") + kHex[c >> 4] + kHex[c & 15];
    }
    return out;
}

/* An id as it may go in a path: Plex's ratingKeys and library keys are digits. */
std::string id_safe(const std::string &s)
{
    std::string out;
    for (char c : s)
        if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
            out += c;
    return out;
}

/* Plex writes some fields as a number or a string depending on the call
 * ("librarySectionID": 1 or "1", "allowSync": true or "1"): read leniently. */
bool flag(const cJSON *o, const char *key, bool def = false)
{
    const cJSON *v = json::get(o, key);
    if (cJSON_IsBool(v))
        return cJSON_IsTrue(v);
    if (cJSON_IsNumber(v))
        return v->valuedouble != 0;
    if (cJSON_IsString(v) && v->valuestring)
        return std::string(v->valuestring) == "1" || std::string(v->valuestring) == "true";
    return def;
}

std::string text(const cJSON *o, const char *key)
{
    const cJSON *v = json::get(o, key);
    if (cJSON_IsString(v) && v->valuestring)
        return v->valuestring;
    int64_t n = 0;
    if (cJSON_IsNumber(v) && json::whole(v->valuedouble, &n)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%lld", (long long)n);
        return buf;
    }
    return {};
}

double number(const cJSON *o, const char *key, double def = 0)
{
    const cJSON *v = json::get(o, key);
    if (cJSON_IsNumber(v))
        return std::isfinite(v->valuedouble) ? v->valuedouble : def;
    if (cJSON_IsString(v) && v->valuestring && *v->valuestring) {
        char *end = nullptr;
        const double d = std::strtod(v->valuestring, &end);
        if (end && *end == '\0' && std::isfinite(d))
            return d;
    }
    return def;
}

int64_t whole(const cJSON *o, const char *key, int64_t def = 0)
{
    int64_t n = 0;
    return json::whole(number(o, key, (double)def), &n) ? n : def;
}

/* The answer's MediaContainer, which every Plex answer is wrapped in. */
const cJSON *container(const json::Doc &doc) { return json::get(doc.root(), "MediaContainer"); }

/* A list in an element: an array, or (some versions) a lone object. */
std::vector<const cJSON *> list(const cJSON *o, const char *key)
{
    std::vector<const cJSON *> out;
    const cJSON *v = json::get(o, key);
    if (cJSON_IsArray(v)) {
        const cJSON *it;
        cJSON_ArrayForEach(it, v) if (cJSON_IsObject(it)) out.push_back(it);
    } else if (cJSON_IsObject(v)) {
        out.push_back(v);
    }
    return out;
}

/* "imdb://tt0807840", "tmdb://9761", "tvdb://276116", and the older agents'
 * "com.plexapp.agents.imdb://tt…?lang=en", "com.plexapp.agents.thetvdb://276116/1/2",
 * "com.plexapp.agents.themoviedb://9761". */
void read_guid(const std::string &g, int *tmdb, int *tvdb, std::string *imdb)
{
    const size_t sep = g.find("://");
    if (sep == std::string::npos)
        return;
    std::string scheme = g.substr(0, sep);
    const size_t dot = scheme.rfind('.');
    if (dot != std::string::npos)
        scheme = scheme.substr(dot + 1);
    std::string value = g.substr(sep + 3);
    value = value.substr(0, value.find_first_of("/?"));
    if (value.empty())
        return;
    if (scheme == "imdb" && value.rfind("tt", 0) == 0 && imdb->empty())
        *imdb = value;
    else if ((scheme == "tmdb" || scheme == "themoviedb") && std::isdigit((unsigned char)value[0]) && !*tmdb)
        *tmdb = json::int_of(value);
    else if ((scheme == "tvdb" || scheme == "thetvdb") && std::isdigit((unsigned char)value[0]) && !*tvdb)
        *tvdb = json::int_of(value);
}

/* "movie", "episode"... as Jellyfin's item types, which the screens know. */
std::string item_type(const std::string &plex_type)
{
    if (plex_type == "movie")
        return "Movie";
    if (plex_type == "episode")
        return "Episode";
    if (plex_type == "show")
        return "Series";
    if (plex_type == "season")
        return "Season";
    return plex_type;
}

model::Session::Stream read_stream(const cJSON *st)
{
    model::Session::Stream s;
    const int type = (int)whole(st, "streamType");
    s.kind = type == 1 ? model::Session::Stream::Video
             : type == 2 ? model::Session::Stream::Audio
             : type == 3 ? model::Session::Stream::Subtitle
                         : model::Session::Stream::Other;
    s.index = (int)whole(st, "id", -1);
    s.codec = text(st, "codec");
    s.profile = text(st, "profile");
    s.language = text(st, "languageTag");
    if (s.language.empty())
        s.language = text(st, "languageCode");
    s.width = (int)whole(st, "width");
    s.height = (int)whole(st, "height");
    s.channels = (int)whole(st, "channels");
    s.bit_depth = (int)whole(st, "bitDepth");
    s.bitrate = whole(st, "bitrate") * 1000;   /* kb/s */
    if (s.kind == model::Session::Stream::Video) {
        const std::string trc = text(st, "colorTrc");
        s.range = flag(st, "DOVIPresent") ? "DOVI" : trc == "smpte2084" ? "HDR10" : trc == "arib-std-b67" ? "HLG" : "SDR";
    }
    if (s.kind == model::Session::Stream::Subtitle) {
        static const char *const kText[] = {"srt", "subrip", "ass", "ssa", "vtt", "webvtt", "mov_text", "txt", "smi"};
        s.text = std::any_of(std::begin(kText), std::end(kText), [&](const char *c) { return s.codec == c; });
    }
    return s;
}

/* Why Plex transcodes, worked out from what goes in and what comes out
 * (Plex says nothing of it in a session), in Jellyfin's reason names. */
void guess_reasons(const cJSON *ts, model::Session *s)
{
    const std::string vd = text(ts, "videoDecision"), ad = text(ts, "audioDecision"), sd = text(ts, "subtitleDecision");
    const std::string vin = text(ts, "sourceVideoCodec"), vout = text(ts, "videoCodec");
    const std::string ain = text(ts, "sourceAudioCodec"), aout = text(ts, "audioCodec");
    auto add = [s](const char *r) { s->transcode_reasons.push_back(r); };
    if (sd == "burn")
        add("SubtitleCodecNotSupported");
    if (vd == "transcode") {
        if (!vin.empty() && !vout.empty() && vin != vout)
            add("VideoCodecNotSupported");
        else
            add("ContainerBitrateExceedsLimit");   /* the same codec, made smaller: the quality asked for */
    }
    if (ad == "transcode" && !ain.empty() && ain != aout)
        add("AudioCodecNotSupported");
    s->reasons_guessed = !s->transcode_reasons.empty();
}

} // namespace

std::vector<std::string> client_headers(const std::string &client_id, const std::string &device_name,
                                        const std::string &version)
{
    return {
        "X-Plex-Client-Identifier: " + header_safe(client_id),
        "X-Plex-Product: Regie5",
        "X-Plex-Version: " + header_safe(version),
        "X-Plex-Platform: " + header_safe(device_name == "PS5" ? "PlayStation" : "Linux"),
        "X-Plex-Device-Name: " + header_safe(device_name),
    };
}

std::string token_header(const std::string &token)
{
    return token.empty() ? std::string() : "X-Plex-Token: " + header_safe(token);
}

std::string identity_path() { return "/identity"; }
std::string info_path() { return "/"; }

bool parse_identity(const std::string &body, std::string *machine_id, std::string *version)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    *machine_id = text(c, "machineIdentifier");
    *version = text(c, "version");
    return !machine_id->empty();
}

bool parse_info(const std::string &body, std::string *version)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c) || !json::get(c, "machineIdentifier"))
        return false;   /* not a Plex server's root */
    *version = text(c, "version");
    return true;
}

bool parse_plex_pass(const std::string &body, bool *plex_pass)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    *plex_pass = flag(c, "myPlexSubscription");
    return true;
}

std::string image_path(const std::string &id, const char *type, int width)
{
    const bool backdrop = std::string(type) == "Backdrop";
    const int height = backdrop ? width * 9 / 16 : width * 3 / 2;   /* fitted inside: a 16:9 thumb in a poster's box is fine */
    return "/photo/:/transcode?width=" + std::to_string(width) + "&height=" + std::to_string(height) +
           "&upscale=0&url=" + encode("/library/metadata/" + id_safe(id) + (backdrop ? "/art" : "/thumb"));
}

std::string sessions_path() { return "/status/sessions"; }

bool parse_sessions(const std::string &body, std::vector<model::Session> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *m : list(c, "Metadata")) {
        model::Session s;
        const cJSON *session = json::get(m, "Session"), *player = json::get(m, "Player"), *user = json::get(m, "User");
        s.id = text(session, "id");   /* none: nothing to stop it by */
        s.user = text(user, "title");
        s.user_id = text(user, "id");
        if (s.user_id.empty())
            s.user_id = text(player, "userID");
        s.device = text(player, "title");   /* never its address */
        s.client = text(player, "product");
        s.paused = text(player, "state") == "paused";
        s.item_id = text(m, "ratingKey");
        s.item_type = item_type(text(m, "type"));
        if (s.item_type == "Episode") {
            s.item_name = text(m, "title");
            s.series_id = text(m, "grandparentRatingKey");
            s.series_name = text(m, "grandparentTitle");
            s.season = (int)whole(m, "parentIndex");
            s.episode = (int)whole(m, "index");
        } else {
            s.item_name = text(m, "title");
        }
        s.position_ticks = whole(m, "viewOffset") * 10000;   /* milliseconds */
        s.runtime_ticks = whole(m, "duration") * 10000;
        const std::vector<const cJSON *> media = list(m, "Media");
        const cJSON *md = media.empty() ? nullptr : media[0];
        const std::vector<const cJSON *> parts = list(md, "Part");
        const cJSON *part = parts.empty() ? nullptr : parts[0];
        s.media_id = text(md, "id");
        /* What is sent: the chosen audio and subtitle streams say which of the file's they are. */
        for (const cJSON *st : list(part, "Stream")) {
            const int type = (int)whole(st, "streamType");
            if (type == 2)
                s.audio_index = (int)whole(st, "id", -1);
            else if (type == 3)
                s.subtitle_index = (int)whole(st, "id", -1);
        }
        const cJSON *ts = json::get(m, "TranscodeSession");
        if (cJSON_IsObject(ts)) {
            const std::string vd = text(ts, "videoDecision"), ad = text(ts, "audioDecision");
            s.video_direct = vd != "transcode";
            s.audio_direct = ad != "transcode";
            s.method = !s.video_direct ? model::Session::Transcode
                       : !s.audio_direct || vd == "copy" ? model::Session::DirectStream
                                                         : model::Session::DirectPlay;
            s.video_codec = text(ts, "videoCodec");
            s.audio_codec = text(ts, "audioCodec");
            s.container_out = text(ts, "container");
            s.audio_channels = (int)whole(ts, "audioChannels");
            s.width = (int)whole(md, "width");
            s.height = (int)whole(md, "height");
            s.bitrate = whole(md, "bitrate") * 1000;
            /* Hardware: what decodes or encodes, by name; requested but neither: the CPU. */
            const std::string hw = !text(ts, "transcodeHwEncodingTitle").empty() ? text(ts, "transcodeHwEncodingTitle")
                                   : !text(ts, "transcodeHwDecodingTitle").empty() ? text(ts, "transcodeHwDecodingTitle")
                                   : !text(ts, "transcodeHwEncoding").empty()      ? text(ts, "transcodeHwEncoding")
                                                                                   : text(ts, "transcodeHwDecoding");
            s.hw_accel = !hw.empty() ? hw : s.video_direct ? std::string() : std::string("none");
            /* How far the transcode has gone, as a share of the item (seconds against milliseconds). */
            const double ahead = number(ts, "maxOffsetAvailable", -1), duration = number(m, "duration");
            if (ahead >= 0 && duration > 0)
                s.completion = std::min(100.0, ahead * 1000.0 / duration * 100.0);
            else if (flag(ts, "complete"))
                s.completion = 100;
            guess_reasons(ts, &s);
        } else {
            s.method = model::Session::DirectPlay;
        }
        out->push_back(std::move(s));
    }
    return true;
}

std::string metadata_path(const std::vector<std::string> &ids)
{
    std::string joined;
    for (const std::string &id : ids) {
        const std::string safe = id_safe(id);
        if (!safe.empty())
            joined += (joined.empty() ? "" : ",") + safe;
    }
    return "/library/metadata/" + joined;
}

bool apply_source(const std::string &body, model::Session *s)
{
    json::Doc doc(body);
    const std::vector<const cJSON *> items = list(container(doc), "Metadata");
    if (items.empty())
        return false;
    const std::vector<const cJSON *> media = list(items[0], "Media");
    const cJSON *md = media.empty() ? nullptr : media[0];
    for (const cJSON *x : media)
        if (!s->media_id.empty() && text(x, "id") == s->media_id)
            md = x;
    if (!md)
        return false;
    const std::vector<const cJSON *> parts = list(md, "Part");
    s->container_in = text(md, "container");
    s->streams.clear();
    for (const cJSON *part : parts)
        for (const cJSON *st : list(part, "Stream")) {
            model::Session::Stream x = read_stream(st);
            if (x.kind != model::Session::Stream::Other)
                s->streams.push_back(x);
        }
    if (s->streams.empty())
        return true;
    /* A direct play sends what the file holds: its own streams, the selected ones. */
    for (const cJSON *part : parts)
        for (const cJSON *st : list(part, "Stream"))
            if (flag(st, "selected")) {
                const int type = (int)whole(st, "streamType");
                if (type == 2 && s->audio_index < 0)
                    s->audio_index = (int)whole(st, "id", -1);
                else if (type == 3 && s->subtitle_index < 0)
                    s->subtitle_index = (int)whole(st, "id", -1);
            }
    return true;
}

bool parse_items(const std::string &body, std::vector<model::JellyfinItem> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *m : list(c, "Metadata")) {
        model::JellyfinItem it;
        it.id = text(m, "ratingKey");
        it.name = text(m, "title");
        it.type = item_type(text(m, "type"));
        it.year = (int)whole(m, "year");
        if (it.type == "Episode") {
            it.series_name = text(m, "grandparentTitle");
            it.season = (int)whole(m, "parentIndex");
            it.episode = (int)whole(m, "index");
        }
        out->push_back(std::move(it));
    }
    return true;
}

std::string stop_path(const std::string &session_id, const std::string &reason)
{
    return "/status/sessions/terminate?sessionId=" + encode(session_id) + "&reason=" + encode(reason);
}

std::string sections_path() { return "/library/sections/all"; }

bool parse_sections(const std::string &body, std::vector<model::JellyfinLibrary> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *d : list(c, "Directory")) {
        model::JellyfinLibrary l;
        l.id = text(d, "key");
        l.name = text(d, "title");
        const std::string type = text(d, "type");
        l.kind = type == "movie" ? model::JellyfinLibrary::Movies
                 : type == "show" ? model::JellyfinLibrary::Shows
                                  : model::JellyfinLibrary::Other;
        for (const cJSON *loc : list(d, "Location")) {   /* the last name only, never the path */
            std::string p = text(loc, "path");
            while (!p.empty() && (p.back() == '/' || p.back() == '\\'))
                p.pop_back();
            const size_t slash = p.find_last_of("/\\");
            if (!p.empty())
                l.folders.push_back(slash == std::string::npos ? p : p.substr(slash + 1));
        }
        if (!l.id.empty())
            out->push_back(std::move(l));
    }
    return true;
}

std::string section_refresh_path(const std::string &section_id)
{
    return "/library/sections/" + id_safe(section_id) + "/refresh";
}

std::string recent_path(const std::string &section_id, bool episodes, int limit)
{
    return "/library/sections/" + id_safe(section_id) + "/all?type=" + (episodes ? "4" : "1") +
           "&sort=addedAt:desc&X-Plex-Container-Start=0&X-Plex-Container-Size=" + std::to_string(limit);
}

bool parse_recent(const std::string &body, std::vector<model::RecentItem> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *m : list(c, "Metadata")) {
        model::RecentItem r;
        r.id = text(m, "ratingKey");
        r.type = item_type(text(m, "type"));
        r.added = (time_t)whole(m, "addedAt");
        if (r.type == "Episode") {
            r.title = text(m, "grandparentTitle");
            r.episode_title = text(m, "title");
            r.season = (int)whole(m, "parentIndex");
            r.episode = (int)whole(m, "index");
            r.series_id = text(m, "grandparentRatingKey");
            r.image_id = r.series_id;
            r.backdrop_id = r.series_id;
            r.year = (int)whole(m, "year");
        } else {
            r.title = text(m, "title");
            r.year = (int)whole(m, "year");
            r.image_id = r.id;
            r.backdrop_id = r.id;
        }
        if (!r.id.empty())
            out->push_back(std::move(r));
    }
    return true;
}

std::vector<model::RecentItem> merge_recent(std::vector<model::RecentItem> all, size_t max_items)
{
    std::stable_sort(all.begin(), all.end(),
                     [](const model::RecentItem &a, const model::RecentItem &b) { return a.added > b.added; });
    std::vector<model::RecentItem> out;
    std::map<std::string, size_t> series_at;
    for (auto &r : all) {
        if (r.type == "Episode" && !r.series_id.empty()) {
            auto f = series_at.find(r.series_id);
            if (f != series_at.end()) {
                out[f->second].new_episodes++;
                continue;
            }
        }
        if (out.size() >= max_items)
            continue;
        if (r.type == "Episode" && !r.series_id.empty())
            series_at[r.series_id] = out.size();
        out.push_back(std::move(r));
    }
    return out;
}

std::string section_items_path(const std::string &section_id)
{
    return "/library/sections/" + id_safe(section_id) + "/all?includeGuids=1";
}

bool parse_section_items(const std::string &body, std::vector<model::UserItem> *out)
{
    out->clear();
    /* A whole library: one title at a time (json::each); none at all is an empty one. */
    return json::each(body, {"MediaContainer", "Metadata"}, [out](const cJSON *m) {
        if (!cJSON_IsObject(m))
            return;
        const std::string type = text(m, "type");
        if (type != "movie" && type != "show")
            return;
        model::UserItem it;
        it.id = text(m, "ratingKey");
        it.series = type == "show";
        for (const cJSON *g : list(m, "Guid"))
            read_guid(text(g, "id"), &it.tmdb_id, &it.tvdb_id, &it.imdb_id);
        read_guid(text(m, "guid"), &it.tmdb_id, &it.tvdb_id, &it.imdb_id);   /* an older agent's own */
        it.played = whole(m, "viewCount") > 0;
        it.last_played = (time_t)whole(m, "lastViewedAt");
        out->push_back(std::move(it));
    }) != json::Each::Bad;
}

std::string count_path(const std::string &section_id, const std::string &type)
{
    return "/library/sections/" + id_safe(section_id) + "/all?" + (type == "Episode" ? "type=4&" : "") +
           "X-Plex-Container-Start=0&X-Plex-Container-Size=0";
}

bool parse_count(const std::string &body, int *count)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c) || !json::get(c, "totalSize"))
        return false;
    *count = (int)whole(c, "totalSize");
    return true;
}

std::string butler_path() { return "/butler"; }

bool parse_butler(const std::string &body, std::vector<model::Task> *out)
{
    json::Doc doc(body);
    const cJSON *tasks = json::get(json::get(doc.root(), "ButlerTasks"), "ButlerTask");
    if (!cJSON_IsArray(tasks) && !cJSON_IsObject(json::get(doc.root(), "ButlerTasks")))
        return false;
    out->clear();
    for (const cJSON *t : list(json::get(doc.root(), "ButlerTasks"), "ButlerTask")) {
        if (!flag(t, "enabled"))
            continue;
        model::Task task;
        task.key = text(t, "name");
        task.id = task.key;
        /* Plex titles few of them: the name, words apart ("ButlerTaskGenerateIntroMarkers"
         * -> "Generate Intro Markers"). */
        task.name = text(t, "title");
        if (task.name.empty()) {
            std::string n = task.key;
            if (n.rfind("ButlerTask", 0) == 0)
                n = n.substr(10);
            for (size_t i = 0; i < n.size(); i++) {
                if (i > 0 && std::isupper((unsigned char)n[i]) && !std::isupper((unsigned char)n[i - 1]))
                    task.name += ' ';
                task.name += n[i];
            }
        }
        task.every_days = std::max(1, (int)whole(t, "interval", 1));
        out->push_back(std::move(task));
    }
    return true;
}

std::string activities_path() { return "/activities"; }

bool parse_activities(const std::string &body, std::vector<model::Task> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *a : list(c, "Activity")) {
        model::Task t;
        t.id = text(a, "uuid");
        t.key = text(a, "type");
        t.name = text(a, "title");
        t.state = model::Task::Running;
        t.progress = std::clamp(number(a, "progress", 0), 0.0, 100.0);
        out->push_back(std::move(t));
    }
    return true;
}

std::string history_path(time_t since, int limit)
{
    return "/status/sessions/history/all?sort=viewedAt:desc&viewedAt%3E=" + std::to_string((long long)since) +
           "&X-Plex-Container-Start=0&X-Plex-Container-Size=" + std::to_string(limit);
}

bool parse_history(const std::string &body, std::vector<model::ActivityEntry> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *m : list(c, "Metadata")) {   /* never deviceID */
        model::ActivityEntry e;
        e.when = (time_t)whole(m, "viewedAt");
        e.type = "Watched";
        e.user_id = text(m, "accountID");
        e.item_id = text(m, "ratingKey");
        if (e.when > 0)
            out->push_back(std::move(e));
    }
    return true;
}

std::map<std::string, time_t> last_played_by_account(const std::vector<model::ActivityEntry> &history)
{
    std::map<std::string, time_t> out;
    for (const auto &e : history)
        out[e.user_id] = std::max(out[e.user_id], e.when);
    return out;
}

std::string accounts_path() { return "/accounts"; }
std::string account_path(const std::string &id) { return "/accounts/" + id_safe(id); }

namespace {
bool read_account(const cJSON *a, model::JellyfinUser *u)
{
    u->id = text(a, "id");
    u->name = text(a, "name");
    u->admin = u->id == "1";   /* the server's owner */
    return !u->id.empty() && u->id != "0" && !u->name.empty();
}
} // namespace

bool parse_accounts(const std::string &body, std::vector<model::JellyfinUser> *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    out->clear();
    for (const cJSON *a : list(c, "Account")) {
        model::JellyfinUser u;
        if (read_account(a, &u))
            out->push_back(std::move(u));
    }
    return true;
}

bool parse_account(const std::string &body, model::JellyfinUser *out)
{
    json::Doc doc(body);
    const std::vector<const cJSON *> a = list(container(doc), "Account");
    return !a.empty() && read_account(a[0], out);
}

std::string prefs_path() { return "/:/prefs"; }

bool parse_prefs(const std::string &body, model::EncodingInfo *out)
{
    json::Doc doc(body);
    const cJSON *c = container(doc);
    if (!cJSON_IsObject(c))
        return false;
    *out = model::EncodingInfo{};
    for (const cJSON *s : list(c, "Setting")) {
        const std::string id = text(s, "id");
        if (id == "HardwareAcceleratedCodecs")   /* only there with Plex Pass */
            out->hw_accel = flag(s, "value") ? "plex" : "none";
        else if (id == "TranscoderThrottleBuffer") {
            out->throttle_delay = (int)whole(s, "value");
            out->throttling = out->throttle_delay > 0;
        }
    }
    return true;
}

} // namespace plex
