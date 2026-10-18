/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/jellyfin.h"

#include "api/json.h"
#include "core/libraries.h"

#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>

namespace jellyfin {

std::string auth_value(const std::string &api_key) { return "MediaBrowser Token=\"" + api_key + "\""; }

std::string info_path() { return "/System/Info"; }
std::string sessions_path() { return "/Sessions?activeWithinSeconds=960"; }
std::string tasks_path() { return "/ScheduledTasks?isHidden=false"; }

std::string recent_path(int limit)
{
    return "/Items?SortBy=DateCreated&SortOrder=Descending&Recursive=true&IncludeItemTypes=Movie,Episode"
           "&IsMissing=false&Limit=" +
           std::to_string(limit) + "&Fields=DateCreated&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1";
}

std::string activity_path(time_t since, int limit)
{
    char date[40];
    struct tm tm;
    gmtime_r(&since, &tm);
    /* snprintf, not strftime: the PS5 title has no C locale behind strftime. */
    std::snprintf(date, sizeof date, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string("/System/ActivityLog/Entries?minDate=") + date + "&hasUserId=true&limit=" + std::to_string(limit);
}

std::string users_path() { return "/Users"; }
std::string encoding_path() { return "/System/Configuration/encoding"; }

std::string user_items_path(const std::string &user_id, bool emby)
{
    return "/Items?userId=" + user_id + "&Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds" +
           (emby ? ",UserDataLastPlayedDate" : "") + "&EnableUserData=true&EnableImages=false";
}

std::string user_path(const std::string &user_id) { return "/Users/" + user_id; }

std::string played_episodes_path(const std::string &user_id)
{
    return "/Items?userId=" + user_id +
           "&Recursive=true&IncludeItemTypes=Episode&IsPlayed=true&Fields=UserDataLastPlayedDate&EnableUserData=true"
           "&EnableImages=false";
}

bool parse_played_episodes(const std::string &body, std::map<std::string, time_t> *latest)
{
    latest->clear();
    /* Every episode a user watched: one at a time (json::each). */
    return json::each(body, {"Items"}, [latest](const cJSON *it) {
        const std::string series = json::str(it, "SeriesId");
        const time_t when = json::parse_time(json::str(json::get(it, "UserData"), "LastPlayedDate"));
        if (!series.empty() && when > 0)
            (*latest)[series] = std::max((*latest)[series], when);
    }) == json::Each::Done;
}

std::string index_path()
{
    return "/Items?Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds&EnableUserData=false"
           "&EnableImages=false";
}

std::string libraries_path() { return "/Library/VirtualFolders"; }

std::string library_items_path(const std::string &library_id)
{
    return "/Items?ParentId=" + library_id +
           "&Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds&EnableUserData=false&EnableImages=false";
}

std::string library_count_path(const std::string &library_id, const std::string &type)
{
    return "/Items?ParentId=" + library_id + "&Recursive=true" +
           (type.empty() ? std::string("&IsFolder=false") : "&IncludeItemTypes=" + type) +
           "&Limit=0&EnableTotalRecordCount=true&EnableImages=false&EnableUserData=false";
}

bool parse_libraries(const std::string &body, std::vector<model::JellyfinLibrary> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::JellyfinLibrary l;
        l.id = json::str(it, "ItemId");
        l.name = json::str(it, "Name");
        const std::string type = json::str(it, "CollectionType");
        l.kind = type == "movies"    ? model::JellyfinLibrary::Movies
                 : type == "tvshows" ? model::JellyfinLibrary::Shows
                 : type == "boxsets" ? model::JellyfinLibrary::Collections
                                     : model::JellyfinLibrary::Other;
        const cJSON *loc;
        cJSON_ArrayForEach(loc, json::get(it, "Locations"))
            if (cJSON_IsString(loc))
                l.folders.push_back(libraries::last_name(loc->valuestring));
        if (!l.id.empty())
            out->push_back(std::move(l));
    }
    return true;
}

bool parse_count(const std::string &body, int *count)
{
    json::Doc d(body);
    const cJSON *n = json::get(d.root(), "TotalRecordCount");
    if (!cJSON_IsNumber(n))
        return false;
    *count = n->valueint;
    return true;
}

bool parse_user_items(const std::string &body, std::vector<model::UserItem> *out)
{
    out->clear();
    /* A whole library: one item at a time (json::each). */
    return json::each(body, {"Items"}, [out](const cJSON *it) {
        model::UserItem u;
        u.id = json::str(it, "Id");
        u.series = json::str(it, "Type") == "Series";
        const cJSON *ids = json::get(it, "ProviderIds");
        u.tmdb_id = json::int_of(json::str(ids, "Tmdb"));
        u.tvdb_id = json::int_of(json::str(ids, "Tvdb"));
        u.imdb_id = json::str(ids, "Imdb");
        const cJSON *ud = json::get(it, "UserData");
        u.played = json::boolean(ud, "Played");
        u.favorite = json::boolean(ud, "IsFavorite");
        u.last_played = json::parse_time(json::str(ud, "LastPlayedDate"));
        const cJSON *left = json::get(ud, "UnplayedItemCount");
        u.unplayed = cJSON_IsNumber(left) ? left->valueint : -1;
        out->push_back(std::move(u));
    }) == json::Each::Done;
}

bool parse_encoding(const std::string &body, model::EncodingInfo *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    out->hw_accel = json::str(d.root(), "HardwareAccelerationType");
    const cJSON *emby_mode = json::get(d.root(), "HardwareAccelerationMode");   /* Emby: 0 off, else on */
    if (out->hw_accel.empty() && cJSON_IsNumber(emby_mode) && emby_mode->valueint == 0)
        out->hw_accel = "none";
    out->throttling = json::boolean(d.root(), "EnableThrottling");
    out->throttle_delay = json::integer(d.root(), "ThrottleDelaySeconds");
    return true;
}

std::string items_path(const std::vector<std::string> &ids)
{
    std::string list;
    for (const auto &id : ids)
        list += (list.empty() ? "" : ",") + id;
    return "/Items?ids=" + list + "&Fields=ProductionYear";
}

std::string stop_path(const std::string &session_id) { return "/Sessions/" + session_id + "/Playing/Stop"; }
std::string library_refresh_path() { return "/Library/Refresh"; }

std::string image_path(const std::string &item_id, const char *type, int width, const std::string &tag)
{
    std::string p = "/Items/" + item_id + "/Images/" + type + "?fillWidth=" + std::to_string(width) +
                    "&quality=90&format=Jpg";
    if (!tag.empty())
        p += "&tag=" + tag;
    return p;
}

std::string emby_image_path(const std::string &path)
{
    std::string p = path;
    const size_t at = p.find("?fillWidth=");
    if (at != std::string::npos)
        p.replace(at + 1, 9, "maxWidth");
    return p;
}

bool parse_info(const std::string &body, std::string *version)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *version = json::str(d.root(), "Version");
    return !version->empty();
}

bool parse_server_id(const std::string &body, std::string *id)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *id = json::str(d.root(), "Id");
    return !id->empty();
}

bool parse_sessions(const std::string &body, std::vector<model::Session> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        const cJSON *np = json::get(it, "NowPlayingItem");
        if (!cJSON_IsObject(np))
            continue;
        model::Session s;
        s.id = json::str(it, "Id");
        s.user = json::str(it, "UserName");
        s.device = json::str(it, "DeviceName");
        s.client = json::str(it, "Client");
        s.item_id = json::str(np, "Id");
        s.item_name = json::str(np, "Name");
        s.item_type = json::str(np, "Type");
        s.series_id = json::str(np, "SeriesId");
        s.series_name = json::str(np, "SeriesName");
        s.season = json::integer(np, "ParentIndexNumber");
        s.episode = json::integer(np, "IndexNumber");
        s.runtime_ticks = json::i64(np, "RunTimeTicks");
        const cJSON *ps = json::get(it, "PlayState");
        s.position_ticks = json::i64(ps, "PositionTicks");
        s.paused = json::boolean(ps, "IsPaused");
        s.audio_index = json::integer(ps, "AudioStreamIndex", -1);
        s.subtitle_index = json::integer(ps, "SubtitleStreamIndex", -1);
        s.container_in = json::str(np, "Container");
        const cJSON *streams = json::get(np, "MediaStreams");
        const cJSON *m;
        cJSON_ArrayForEach(m, streams)
        {
            model::Session::Stream st;
            const std::string type = json::str(m, "Type");
            st.kind = type == "Video"      ? model::Session::Stream::Video
                      : type == "Audio"    ? model::Session::Stream::Audio
                      : type == "Subtitle" ? model::Session::Stream::Subtitle
                                           : model::Session::Stream::Other;
            st.index = json::integer(m, "Index", -1);
            st.codec = json::str(m, "Codec");
            st.profile = json::str(m, "Profile");
            st.range = json::str(m, "VideoRangeType", json::str(m, "VideoRange"));
            st.language = json::str(m, "Language");
            st.width = json::integer(m, "Width");
            st.height = json::integer(m, "Height");
            st.channels = json::integer(m, "Channels");
            st.bit_depth = json::integer(m, "BitDepth");
            st.bitrate = json::i64(m, "BitRate");
            st.text = json::boolean(m, "IsTextSubtitleStream");
            s.streams.push_back(st);
        }
        const std::string method = json::str(ps, "PlayMethod");
        s.method = method == "Transcode"      ? model::Session::Transcode
                   : method == "DirectStream" ? model::Session::DirectStream
                                              : model::Session::DirectPlay;
        const cJSON *ti = json::get(it, "TranscodingInfo");
        if (cJSON_IsObject(ti)) {
            s.video_direct = json::boolean(ti, "IsVideoDirect", true);
            s.audio_direct = json::boolean(ti, "IsAudioDirect", true);
            s.video_codec = json::str(ti, "VideoCodec");
            s.audio_codec = json::str(ti, "AudioCodec");
            s.width = json::integer(ti, "Width");
            s.height = json::integer(ti, "Height");
            s.bitrate = json::i64(ti, "Bitrate");
            s.container_out = json::str(ti, "Container");
            s.audio_channels = json::integer(ti, "AudioChannels");
            s.framerate = json::num(ti, "Framerate");
            s.hw_accel = json::str(ti, "HardwareAccelerationType");
            const cJSON *pipeline = json::get(ti, "VideoPipelineInfo");   /* Emby */
            if (s.hw_accel.empty() && cJSON_IsArray(pipeline)) {
                const cJSON *step;
                cJSON_ArrayForEach(step, pipeline) if (json::boolean(step, "IsHardwareContext") && s.hw_accel.empty())
                    s.hw_accel = json::str(step, "HardwareContextName", "hardware");
                if (s.hw_accel.empty() && !s.video_direct)
                    s.hw_accel = "none";
            }
            const cJSON *done = json::get(ti, "CompletionPercentage");
            s.completion = cJSON_IsNumber(done) ? done->valuedouble : -1;
            const cJSON *reasons = json::get(ti, "TranscodeReasons");
            if (cJSON_IsArray(reasons)) {
                const cJSON *r;
                cJSON_ArrayForEach(r, reasons) if (cJSON_IsString(r)) s.transcode_reasons.push_back(r->valuestring);
            } else if (cJSON_IsString(reasons)) {   /* older servers: "A, B" */
                std::string all = reasons->valuestring, cur;
                for (char c : all + ",") {
                    if (c == ',') {
                        if (!cur.empty())
                            s.transcode_reasons.push_back(cur);
                        cur.clear();
                    } else if (c != ' ') {
                        cur += c;
                    }
                }
            }
        }
        out->push_back(s);
    }
    return true;
}

bool parse_tasks(const std::string &body, std::vector<model::Task> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        if (json::boolean(it, "IsHidden"))
            continue;
        model::Task t;
        t.id = json::str(it, "Id");
        t.key = json::str(it, "Key");
        t.name = json::str(it, "Name");
        t.category = json::str(it, "Category");
        const std::string state = json::str(it, "State");
        t.state = state == "Running" ? model::Task::Running
                  : state == "Cancelling" ? model::Task::Cancelling
                                          : model::Task::Idle;
        t.progress = std::clamp(json::num(it, "CurrentProgressPercentage"), 0.0, 100.0);
        const cJSON *last = json::get(it, "LastExecutionResult");
        if (cJSON_IsObject(last)) {
            const std::string st = json::str(last, "Status");
            t.last = st == "Completed" ? model::Task::Completed
                     : st == "Failed"  ? model::Task::Failed
                     : st == "Cancelled" ? model::Task::Cancelled
                     : st == "Aborted"   ? model::Task::Aborted
                                         : model::Task::Never;
            t.last_end = json::parse_time(json::str(last, "EndTimeUtc"));
        }
        out->push_back(t);
    }
    return true;
}

bool parse_recent(const std::string &body, std::vector<model::RecentItem> *out, size_t max_items)
{
    json::Doc d(body);
    const cJSON *items = json::get(d.root(), "Items");
    if (!cJSON_IsArray(items))
        return false;
    out->clear();
    std::map<std::string, size_t> series_at;   /* series id -> its entry */
    const cJSON *it;
    cJSON_ArrayForEach(it, items)
    {
        const std::string type = json::str(it, "Type");
        const std::string series = json::str(it, "SeriesId");
        if (type == "Episode" && !series.empty()) {
            auto f = series_at.find(series);
            if (f != series_at.end()) {
                (*out)[f->second].new_episodes++;
                continue;
            }
        }
        if (out->size() >= max_items)
            continue;
        model::RecentItem r;
        r.id = json::str(it, "Id");
        r.type = type;
        r.added = json::parse_time(json::str(it, "DateCreated"));
        r.year = json::integer(it, "ProductionYear");
        const cJSON *hashes = json::get(it, "ImageBlurHashes");
        if (type == "Episode") {
            r.title = json::str(it, "SeriesName");
            r.episode_title = json::str(it, "Name");
            r.season = json::integer(it, "ParentIndexNumber");
            r.episode = json::integer(it, "IndexNumber");
            r.series_id = series;
            r.image_id = series;
            r.image_tag = json::str(it, "SeriesPrimaryImageTag");
            r.backdrop_id = json::str(it, "ParentBackdropItemId", series);
        } else {
            r.title = json::str(it, "Name");
            r.image_id = r.id;
            r.image_tag = json::str(json::get(it, "ImageTags"), "Primary");
            r.backdrop_id = r.id;
        }
        r.blurhash = json::str(json::get(hashes, "Primary"), r.image_tag.c_str());
        if (!series.empty() && type == "Episode")
            series_at[series] = out->size();
        out->push_back(r);
    }
    return true;
}

bool parse_activity(const std::string &body, std::vector<model::ActivityEntry> *out)
{
    json::Doc d(body);
    const cJSON *items = json::get(d.root(), "Items");
    if (!cJSON_IsArray(items))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, items)
    {
        /* Never read here: Name, ShortOverview, Overview. */
        model::ActivityEntry e;
        e.when = json::parse_time(json::str(it, "Date"));
        e.type = json::str(it, "Type");
        static const struct {
            const char *emby, *jellyfin;
        } kEmby[] = {{"playback.start", "VideoPlayback"},
                     {"playback.stop", "VideoPlaybackStopped"},
                     {"user.authenticated", "AuthenticationSucceeded"},
                     {"user.created", "UserCreated"}};
        for (const auto &k : kEmby)
            if (e.type == k.emby)
                e.type = k.jellyfin;
        e.user_id = json::str(it, "UserId");
        e.item_id = json::str(it, "ItemId");
        if (e.when > 0 && !e.type.empty())
            out->push_back(std::move(e));
    }
    return true;
}

namespace {

model::JellyfinUser user_of(const cJSON *it)
{
    model::JellyfinUser u;
    u.id = json::str(it, "Id");
    u.name = json::str(it, "Name");
    const cJSON *policy = json::get(it, "Policy");
    u.disabled = json::boolean(policy, "IsDisabled");
    u.admin = json::boolean(policy, "IsAdministrator");
    u.last_activity = json::parse_time(json::str(it, "LastActivityDate"));
    return u;
}

} // namespace

bool parse_users(const std::string &body, std::vector<model::JellyfinUser> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::JellyfinUser u = user_of(it);
        if (!u.id.empty())
            out->push_back(std::move(u));
    }
    return true;
}

bool parse_user(const std::string &body, model::JellyfinUser *out)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *out = user_of(d.root());
    return !out->id.empty();
}

bool parse_items(const std::string &body, std::vector<model::JellyfinItem> *out)
{
    json::Doc d(body);
    const cJSON *items = json::get(d.root(), "Items");
    if (!cJSON_IsArray(items))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, items)
    {
        model::JellyfinItem m;
        m.id = json::str(it, "Id");
        m.name = json::str(it, "Name");
        m.type = json::str(it, "Type");
        m.series_name = json::str(it, "SeriesName");
        m.season = json::integer(it, "ParentIndexNumber");
        m.episode = json::integer(it, "IndexNumber");
        m.year = json::integer(it, "ProductionYear");
        if (!m.id.empty())
            out->push_back(std::move(m));
    }
    return true;
}

namespace {

/* A value inside the MediaBrowser header: no quotes, no commas, no control characters. */
std::string header_safe(const std::string &s)
{
    std::string out;
    for (char c : s)
        if (c != '"' && c != ',' && (unsigned char)c >= 0x20)
            out += c;
    return out;
}

} // namespace

std::string auth_client(const std::string &device_id, const std::string &device_name, const std::string &version,
                        const std::string &token)
{
    std::string h = "MediaBrowser Client=\"Regie5\", Device=\"" + header_safe(device_name) + "\", DeviceId=\"" +
                    header_safe(device_id) + "\", Version=\"" + header_safe(version) + "\"";
    if (!token.empty())
        h += ", Token=\"" + header_safe(token) + "\"";
    return h;
}

std::string quick_connect_enabled_path() { return "/QuickConnect/Enabled"; }
std::string quick_connect_initiate_path() { return "/QuickConnect/Initiate"; }
std::string quick_connect_auth_path() { return "/Users/AuthenticateWithQuickConnect"; }

std::string quick_connect_poll_path(const std::string &secret)
{
    std::string hex;   /* the secret is hex; anything else is left out of the URL */
    for (char c : secret)
        if (std::isxdigit((unsigned char)c))
            hex += c;
    return "/QuickConnect/Connect?secret=" + hex;
}

std::string quick_connect_auth_body(const std::string &secret)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "Secret", secret.c_str());
    char *text = cJSON_PrintUnformatted(o);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(o);
    return out;
}

bool parse_quick_connect_enabled(const std::string &body, bool *on)
{
    json::Doc d(body);
    if (!cJSON_IsBool(d.root()))
        return false;
    *on = cJSON_IsTrue(d.root());
    return true;
}

bool parse_quick_connect(const std::string &body, model::QuickConnect *out)
{
    json::Doc d(body);
    out->code = json::str(d.root(), "Code");
    out->secret = json::str(d.root(), "Secret");
    return !out->code.empty() && !out->secret.empty();
}

bool parse_quick_connect_poll(const std::string &body, bool *approved)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *approved = json::boolean(d.root(), "Authenticated");
    return true;
}

bool parse_session(const std::string &body, model::JellyfinSession *out)
{
    json::Doc d(body);
    out->token = json::str(d.root(), "AccessToken");
    const cJSON *user = json::get(d.root(), "User");
    out->user = json::str(user, "Name");
    out->admin = json::boolean(json::get(user, "Policy"), "IsAdministrator");
    return !out->token.empty();
}

} // namespace jellyfin
