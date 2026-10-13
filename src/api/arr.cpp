/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/arr.h"

#include "api/json.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace arr {

namespace {

std::string iso(time_t t)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[40];
    /* snprintf, not strftime: the PS5 title has no C locale behind strftime. */
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}


} // namespace

std::string status_path() { return "/api/v3/system/status"; }
std::string health_path() { return "/api/v3/health"; }
std::string diskspace_path() { return "/api/v3/diskspace"; }

std::string calendar_path(config::Service s, time_t start, time_t end)
{
    std::string p = "/api/v3/calendar?start=" + iso(start) + "&end=" + iso(end) + "&unmonitored=false";
    if (s == config::Sonarr)
        p += "&includeSeries=true";
    return p;
}

std::string delete_movie_path(int movie_id)
{
    return "/api/v3/movie/" + std::to_string(movie_id) + "?deleteFiles=true&addImportExclusion=true";
}

std::string delete_series_path(int series_id)
{
    return "/api/v3/series/" + std::to_string(series_id) + "?deleteFiles=true&addImportListExclusion=true";
}

std::string history_path(config::Service s, time_t since)
{
    std::string p = "/api/v3/history/since?date=" + iso(since) + "&eventType=downloadFolderImported";
    p += s == config::Sonarr ? "&includeSeries=true&includeEpisode=true" : "&includeMovie=true";
    return p;
}

bool parse_history_imports(const std::string &body, config::Service from, std::vector<model::ImportEvent> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        /* Never read here: sourceTitle (the release), data, downloadId. */
        if (json::str(it, "eventType") != "downloadFolderImported")
            continue;
        model::ImportEvent e;
        e.from = from;
        e.when = json::parse_time(json::str(it, "date"));
        const cJSON *q = json::get(json::get(it, "quality"), "quality");
        e.quality = json::str(q, "name");
        if (from == config::Sonarr) {
            const cJSON *series = json::get(it, "series"), *ep = json::get(it, "episode");
            e.media_id = json::integer(it, "seriesId", json::integer(series, "id"));
            e.title = json::str(series, "title");
            e.year = json::integer(series, "year");
            e.season = json::integer(ep, "seasonNumber");
            e.episode = json::integer(ep, "episodeNumber");
            e.episode_title = json::str(ep, "title");
        } else {
            const cJSON *movie = json::get(it, "movie");
            e.media_id = json::integer(it, "movieId", json::integer(movie, "id"));
            e.title = json::str(movie, "title");
            e.year = json::integer(movie, "year");
        }
        if (e.when > 0 && !e.title.empty())
            out->push_back(std::move(e));
    }
    std::stable_sort(out->begin(), out->end(),
                     [](const model::ImportEvent &a, const model::ImportEvent &b) { return a.when > b.when; });
    return true;
}

std::string queue_path(config::Service s, int page_size)
{
    std::string p = "/api/v3/queue?page=1&pageSize=" + std::to_string(page_size);
    p += s == config::Sonarr ? "&includeSeries=true&includeEpisode=true" : "&includeMovie=true";
    return p;
}

std::string mediacover_path(int media_id, const std::string &name)
{
    return "/api/v3/mediacover/" + std::to_string(media_id) + "/" + name;
}

std::string series_path() { return "/api/v3/series"; }
std::string movies_path() { return "/api/v3/movie"; }

std::string missing_path(config::Service s, int page_size)
{
    std::string p = "/api/v3/wanted/missing?page=1&pageSize=" + std::to_string(page_size) + "&monitored=true";
    p += s == config::Sonarr ? "&includeSeries=true&sortKey=episodes.airDateUtc&sortDirection=descending"
                             : "&sortKey=movieMetadata.inCinemas&sortDirection=descending";
    return p;
}

std::string quality_profiles_path() { return "/api/v3/qualityprofile"; }
std::string episode_files_path(int series_id) { return "/api/v3/episodefile?seriesId=" + std::to_string(series_id); }

namespace {

std::string lower_sort(const std::string &sort, const std::string &title)
{
    std::string s = sort.empty() ? title : sort;
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

model::Movie read_movie(const cJSON *it)
{
    model::Movie m;
    m.id = json::integer(it, "id");
    m.tmdb_id = json::integer(it, "tmdbId");
    m.imdb_id = json::str(it, "imdbId");
    m.title = json::str(it, "title");
    m.sort_title = lower_sort(json::str(it, "sortTitle"), m.title);
    m.year = json::integer(it, "year");
    const std::string st = json::str(it, "status");
    m.status = st == "tba"         ? model::Movie::Tba
               : st == "announced" ? model::Movie::Announced
               : st == "inCinemas" ? model::Movie::InCinemas
               : st == "deleted"   ? model::Movie::Deleted
                                   : model::Movie::Released;
    m.monitored = json::boolean(it, "monitored", true);
    m.has_file = json::boolean(it, "hasFile");
    m.available = json::boolean(it, "isAvailable");
    m.added = json::parse_time(json::str(it, "added"));
    m.in_cinemas = json::parse_time(json::str(it, "inCinemas"));
    m.digital = json::parse_time(json::str(it, "digitalRelease"));
    m.physical = json::parse_time(json::str(it, "physicalRelease"));
    m.quality_profile = json::integer(it, "qualityProfileId");
    m.size = json::i64(it, "sizeOnDisk");
    m.studio = json::str(it, "studio");
    m.overview = json::str(it, "overview");
    m.runtime = json::integer(it, "runtime");
    /* Of its file: the quality only (never its path, name, scene name or group). */
    const cJSON *file = json::get(it, "movieFile");
    if (cJSON_IsObject(file))
        m.file_quality = json::str(json::get(json::get(file, "quality"), "quality"), "name");
    return m;
}

} // namespace

bool parse_series(const std::string &body, std::vector<model::Series> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::Series s;
        s.id = json::integer(it, "id");
        s.title = json::str(it, "title");
        s.tvdb_id = json::integer(it, "tvdbId");
        s.tmdb_id = json::integer(it, "tmdbId");
        s.imdb_id = json::str(it, "imdbId");
        s.sort_title = lower_sort(json::str(it, "sortTitle"), s.title);
        s.year = json::integer(it, "year");
        const std::string st = json::str(it, "status");
        s.status = st == "ended"      ? model::Series::Ended
                   : st == "upcoming" ? model::Series::Upcoming
                   : st == "deleted"  ? model::Series::Deleted
                                      : model::Series::Continuing;
        s.monitored = json::boolean(it, "monitored", true);
        s.added = json::parse_time(json::str(it, "added"));
        s.next_airing = json::parse_time(json::str(it, "nextAiring"));
        s.previous_airing = json::parse_time(json::str(it, "previousAiring"));
        s.quality_profile = json::integer(it, "qualityProfileId");
        s.network = json::str(it, "network");
        s.overview = json::str(it, "overview");
        s.runtime = json::integer(it, "runtime");
        /* Statistics: counts and sizes (never releaseGroups). */
        const cJSON *stats = json::get(it, "statistics");
        s.files = json::integer(stats, "episodeFileCount");
        s.episodes = json::integer(stats, "episodeCount");
        s.total = json::integer(stats, "totalEpisodeCount");
        s.size = json::i64(stats, "sizeOnDisk");
        const cJSON *season;
        cJSON_ArrayForEach(season, json::get(it, "seasons"))
        {
            model::SeasonStats ss;
            ss.number = json::integer(season, "seasonNumber");
            ss.monitored = json::boolean(season, "monitored", true);
            const cJSON *st2 = json::get(season, "statistics");
            ss.files = json::integer(st2, "episodeFileCount");
            ss.episodes = json::integer(st2, "episodeCount");
            ss.total = json::integer(st2, "totalEpisodeCount");
            ss.size = json::i64(st2, "sizeOnDisk");
            ss.next_airing = json::parse_time(json::str(st2, "nextAiring"));
            s.seasons.push_back(ss);
        }
        std::sort(s.seasons.begin(), s.seasons.end(),
                  [](const model::SeasonStats &a, const model::SeasonStats &b) {
                      /* Specials (season 0) last. */
                      return (a.number == 0 ? 1 << 20 : a.number) < (b.number == 0 ? 1 << 20 : b.number);
                  });
        out->push_back(std::move(s));
    }
    return true;
}

bool parse_movies(const std::string &body, std::vector<model::Movie> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root()) out->push_back(read_movie(it));
    return true;
}

bool parse_missing_episodes(const std::string &body, std::vector<model::MissingEpisode> *out, int *total)
{
    json::Doc d(body);
    const cJSON *records = json::get(d.root(), "records");
    if (!cJSON_IsArray(records))
        return false;
    out->clear();
    if (total)
        *total = json::integer(d.root(), "totalRecords");
    const cJSON *it;
    cJSON_ArrayForEach(it, records)
    {
        model::MissingEpisode m;
        m.series_id = json::integer(it, "seriesId");
        m.episode_id = json::integer(it, "id");
        m.series_title = json::str(json::get(it, "series"), "title");
        m.season = json::integer(it, "seasonNumber");
        m.episode = json::integer(it, "episodeNumber");
        m.title = json::str(it, "title");
        m.aired = json::parse_time(json::str(it, "airDateUtc"));
        out->push_back(m);
    }
    return true;
}

bool parse_missing_movies(const std::string &body, std::vector<model::Movie> *out, int *total)
{
    json::Doc d(body);
    const cJSON *records = json::get(d.root(), "records");
    if (!cJSON_IsArray(records))
        return false;
    out->clear();
    if (total)
        *total = json::integer(d.root(), "totalRecords");
    const cJSON *it;
    cJSON_ArrayForEach(it, records) out->push_back(read_movie(it));
    return true;
}

bool parse_quality_profiles(const std::string &body, std::vector<model::QualityProfile> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root()) out->push_back({json::integer(it, "id"), json::str(it, "name")});
    return true;
}

bool parse_episode_files(const std::string &body, model::FileSummary *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    *out = {};
    std::vector<std::pair<std::string, int>> &q = out->qualities;
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        /* The quality and the size: never relativePath, path, sceneName or releaseGroup. */
        const std::string name = json::str(json::get(json::get(it, "quality"), "quality"), "name");
        out->size += json::i64(it, "size");
        auto f = std::find_if(q.begin(), q.end(), [&](const auto &p) { return p.first == name; });
        if (f == q.end())
            q.push_back({name, 1});
        else
            f->second++;
    }
    std::stable_sort(q.begin(), q.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    return true;
}

bool parse_status(const std::string &body, std::string *version)
{
    json::Doc d(body);
    if (!cJSON_IsObject(d.root()))
        return false;
    *version = json::str(d.root(), "version");
    return !version->empty();
}

model::Health::Kind health_kind(const std::string &src, model::Health::Level level)
{
    using H = model::Health;
    const bool err = level == H::Error;
    static const struct {
        const char *source;
        H::Kind error, other;   /* the check's error variant, and the rest */
    } kChecks[] = {
        /* Sonarr's and Radarr's HealthCheck/Checks sources: which message goes with which level. */
        {"DownloadClientCheck", H::ClientUnreachable, H::ClientNone},
        {"DownloadClientStatusCheck", H::ClientsAllFailing, H::ClientSomeFailing},
        {"DownloadClientRemovesCompletedDownloadsCheck", H::ClientRemovesCompleted, H::ClientRemovesCompleted},
        {"DownloadClientRootFolderCheck", H::ClientRootFolder, H::ClientRootFolder},
        {"DownloadClientSortingCheck", H::ClientSorting, H::ClientSorting},
        {"RemotePathMappingCheck", H::RemotePath, H::RemotePath},
        {"ImportMechanismCheck", H::ImportOff, H::ImportOff},
        {"IndexerStatusCheck", H::IndexersAllDown, H::IndexerSomeDown},
        {"IndexerLongTermStatusCheck", H::IndexersAllDownLong, H::IndexerSomeDownLong},
        {"IndexerRssCheck", H::IndexersNoRss, H::IndexersRssDown},
        {"IndexerSearchCheck", H::IndexersSearch, H::IndexersSearch},
        {"IndexerDownloadClientCheck", H::IndexerBadClient, H::IndexerBadClient},
        {"IndexerJackettAllCheck", H::IndexerUnsupported, H::IndexerUnsupported},
        {"PTPOldSettingsCheck", H::IndexerOldSettings, H::IndexerOldSettings},
        {"RootFolderCheck", H::RootFolderMissing, H::RootFolderMissing},
        {"MountCheck", H::ReadOnlyMount, H::ReadOnlyMount},
        {"RecyclingBinCheck", H::RecycleBin, H::RecycleBin},
        {"ImportListRootFolderCheck", H::ListRootFolder, H::ListRootFolder},
        {"ImportListStatusCheck", H::ListsAllFailing, H::ListSomeFailing},
        {"MovieCollectionRootFolderCheck", H::CollectionRootFolder, H::CollectionRootFolder},
        {"RemovedSeriesCheck", H::SeriesRemoved, H::SeriesRemoved},
        {"RemovedMovieCheck", H::MovieRemoved, H::MovieRemoved},
        {"MetadataCheck", H::Metadata, H::Metadata},
        {"NamingConfigCheck", H::Naming, H::Naming},
        {"NotificationStatusCheck", H::NotificationsAllFailing, H::NotificationSomeFailing},
        {"UpdateCheck", H::UpdateBlocked, H::UpdateAvailable},
        {"SystemTimeCheck", H::ClockOff, H::ClockOff},
        {"ProxyCheck", H::ProxyFailing, H::ProxyFailing},
        {"AppDataLocationCheck", H::AppDataLocation, H::AppDataLocation},
        {"ApiKeyValidationCheck", H::ApiKeyShort, H::ApiKeyShort},
        {"AllowedHostsCheck", H::AllowedHosts, H::AllowedHosts},
        {"SqliteVersionCheck", H::SqliteOld, H::SqliteOld},
        {"ReleaseBranchCheck", H::UpdateBranch, H::UpdateBranch},
        {"SlackUrlCheck", H::Slack, H::Slack},
        {"PackageGlobalMessageCheck", H::Announcement, H::Announcement},
    };
    for (const auto &c : kChecks)
        if (src == c.source)
            return err ? c.error : c.other;
    return H::Unknown;
}

bool shareable(const std::string &text)
{
    if (text.empty())
        return false;
    std::string low;
    for (char c : text)
        low += (char)std::tolower((unsigned char)c);
    /* What a download client, an indexer or a tracker is called, or how one talks about them. */
    static const char *const kNames[] = {
        "client", "indexer", "indexeur", "tracker", "torrent", "usenet", "nzb", "qbit", "transmission",
        "deluge", "rtorrent", "aria2", "flood", "vuze", "hadouken", "download station", "downloadstation",
        "sabnzbd", "prowlarr", "jackett", "hydra", "torznab", "newznab", "passthepopcorn", "ptp", "seedbox",
        "http://", "https://", "ftp://",
    };
    for (const char *n : kNames)
        if (low.find(n) != std::string::npos)
            return false;
    /* A path: "/data/tv", "C:\Media", "\\nas\share". */
    for (size_t i = 0; i + 1 < text.size(); i++) {
        const char c = text[i], d = text[i + 1];
        const bool start = i == 0 || text[i - 1] == ' ' || text[i - 1] == '(' || text[i - 1] == '\'' ||
                           text[i - 1] == '"' || text[i - 1] == ':';
        if (start && c == '/' && (std::isalnum((unsigned char)d) || d == '.' || d == '_'))
            return false;
        if (c == '\\' && (std::isalnum((unsigned char)d) || d == '\\'))
            return false;
    }
    /* An address: four groups of digits. */
    int dots = 0, digits = 0;
    for (char c : text) {
        if (std::isdigit((unsigned char)c))
            digits++;
        else if (c == '.' && digits > 0)
            dots++, digits = 0;
        else {
            if (dots == 3 && digits > 0)
                return false;
            dots = digits = 0;
        }
    }
    return !(dots == 3 && digits > 0);
}

bool parse_health(const std::string &body, config::Service from, std::vector<model::Health> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::Health h;
        h.from = from;
        h.source = json::str(it, "source");
        const std::string type = json::str(it, "type");
        if (type == "ok")
            continue;
        h.level = type == "error" ? model::Health::Error : type == "notice" ? model::Health::Notice
                                                                             : model::Health::Warning;
        h.kind = health_kind(h.source, h.level);
        /* A known check is worded by the interface; the server's text is kept only
         * for an unknown one or an announcement, and only when it names nothing. */
        if (h.kind == model::Health::Unknown || h.kind == model::Health::Announcement) {
            const std::string text = json::str(it, "message");
            if (shareable(text))
                h.message = text;
        }
        out->push_back(h);
    }
    return true;
}

bool parse_diskspace(const std::string &body, config::Service from, std::vector<model::DiskSpace> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::DiskSpace s;
        s.from = from;
        s.path = json::str(it, "path");
        s.label = json::str(it, "label");
        s.free = json::i64(it, "freeSpace");
        s.total = json::i64(it, "totalSpace");
        out->push_back(s);
    }
    return true;
}

bool parse_sonarr_calendar(const std::string &body, std::vector<model::CalendarItem> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        model::CalendarItem c;
        c.from = config::Sonarr;
        c.media_id = json::integer(it, "seriesId");
        c.season = json::integer(it, "seasonNumber");
        c.episode = json::integer(it, "episodeNumber");
        c.episode_title = json::str(it, "title");
        c.when = json::parse_time(json::str(it, "airDateUtc"));
        c.has_file = json::boolean(it, "hasFile");
        c.release = model::CalendarItem::Air;
        const cJSON *series = json::get(it, "series");
        c.title = json::str(series, "title");
        c.year = json::integer(series, "year");
        out->push_back(c);
    }
    std::stable_sort(out->begin(), out->end(),
                     [](const model::CalendarItem &a, const model::CalendarItem &b) { return a.when < b.when; });
    return true;
}

bool parse_radarr_calendar(const std::string &body, time_t start, time_t end, std::vector<model::CalendarItem> *out)
{
    json::Doc d(body);
    if (!cJSON_IsArray(d.root()))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, d.root())
    {
        static const struct {
            const char *field;
            model::CalendarItem::Release kind;
        } kDates[] = {{"inCinemas", model::CalendarItem::Cinema},
                      {"digitalRelease", model::CalendarItem::Digital},
                      {"physicalRelease", model::CalendarItem::Physical}};
        for (const auto &k : kDates) {
            const time_t t = json::parse_time(json::str(it, k.field));
            if (!t || t < start || t > end)
                continue;
            model::CalendarItem c;
            c.from = config::Radarr;
            c.media_id = json::integer(it, "id");
            c.title = json::str(it, "title");
            c.year = json::integer(it, "year");
            c.has_file = json::boolean(it, "hasFile");
            c.release = k.kind;
            c.when = t;
            out->push_back(c);
        }
    }
    std::stable_sort(out->begin(), out->end(),
                     [](const model::CalendarItem &a, const model::CalendarItem &b) { return a.when < b.when; });
    return true;
}

bool parse_queue(const std::string &body, config::Service from, time_t now, std::vector<model::QueueItem> *out,
                 int *total)
{
    json::Doc d(body);
    const cJSON *records = json::get(d.root(), "records");
    if (!cJSON_IsArray(records))
        return false;
    out->clear();
    if (total)
        *total = json::integer(d.root(), "totalRecords");
    const cJSON *it;
    cJSON_ArrayForEach(it, records)
    {
        /* Never read here: title (the release), statusMessages, errorMessage,
         * downloadClient, indexer, outputPath, downloadId. */
        model::QueueItem q;
        q.from = from;
        q.id = json::integer(it, "id");
        if (from == config::Sonarr) {
            q.media_id = json::integer(it, "seriesId");
            const cJSON *series = json::get(it, "series");
            const cJSON *ep = json::get(it, "episode");
            q.title = json::str(series, "title");
            q.year = json::integer(series, "year");
            q.season = json::integer(ep, "seasonNumber", json::integer(it, "seasonNumber"));
            q.episode = json::integer(ep, "episodeNumber");
            q.episode_title = json::str(ep, "title");
        } else {
            q.media_id = json::integer(it, "movieId");
            const cJSON *movie = json::get(it, "movie");
            q.title = json::str(movie, "title");
            q.year = json::integer(movie, "year");
        }
        q.size = (int64_t)json::num(it, "size");
        q.size_left = (int64_t)json::num(it, "sizeleft");
        q.progress = q.size > 0 ? std::clamp(1.0 - (double)q.size_left / (double)q.size, 0.0, 1.0) : 0.0;
        /* The estimate when it is still ahead; one already past is stale: the timespan then. */
        const time_t eta = json::parse_time(json::str(it, "estimatedCompletionTime"));
        q.time_left = eta && eta > now ? (int64_t)(eta - now) : json::parse_timespan(json::str(it, "timeleft"));
        q.quality = json::str(json::get(json::get(it, "quality"), "quality"), "name");

        const std::string status = json::str(it, "status");
        const std::string tracked = json::str(it, "trackedDownloadStatus");
        const std::string state = json::str(it, "trackedDownloadState");
        using S = model::QueueItem;
        if (tracked == "error" || status == "failed" || state == "failed" || state == "failedPending")
            q.status = S::Error;
        else if (tracked == "warning" || status == "warning" || status == "downloadClientUnavailable" ||
                 state == "importBlocked")
            q.status = S::Warning;
        else if (state == "imported")
            q.status = S::Completed;
        else if (state == "importPending" || state == "importing" || status == "completed")
            q.status = S::Importing;
        else if (status == "downloading")
            q.status = S::Downloading;
        else if (status == "paused")
            q.status = S::Paused;
        else
            q.status = S::Queued;
        out->push_back(q);
    }
    return true;
}

} // namespace arr
