/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the screens show, as the API parsers make it. Deliberately without
 * what the app must never show: no release or tracker names, no download
 * client or indexer, no file paths from the queue (they are never copied
 * out of the JSON, so nothing can display them by mistake).
 */
#pragma once

#include "core/config.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <utility>

namespace model {

/* ---- Sonarr and Radarr ---- */

struct Health {
    enum Level { Notice, Warning, Error } level = Warning;
    config::Service from = config::Sonarr;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    std::string source;    /* the check's name ("RootFolderCheck") */
    /* What it is about, from the check and its level (the interface words each
     * one itself: the server's text may name a download client, an indexer or
     * a path, and is never shown for a known check). */
    enum Kind {
        Unknown,
        /* download clients */
        ClientNone, ClientUnreachable, ClientsAllFailing, ClientSomeFailing, ClientRemovesCompleted,
        ClientRootFolder, ClientSorting, RemotePath, ImportOff,
        /* indexers */
        IndexersAllDown, IndexerSomeDown, IndexersAllDownLong, IndexerSomeDownLong, IndexersNoRss,
        IndexersRssDown, IndexersSearch, IndexerBadClient, IndexerUnsupported, IndexerOldSettings,
        /* the library */
        RootFolderMissing, ReadOnlyMount, RecycleBin, ListRootFolder, ListsAllFailing, ListSomeFailing,
        CollectionRootFolder, SeriesRemoved, MovieRemoved, Metadata, Naming,
        /* the server */
        NotificationsAllFailing, NotificationSomeFailing, UpdateAvailable, UpdateBlocked, ClockOff,
        ProxyFailing, AppDataLocation, ApiKeyShort, AllowedHosts, SqliteOld, UpdateBranch, Slack,
        Announcement,
        KindCount
    } kind = Unknown;
    /* The server's own text: kept only for an unknown check (or an announcement)
     * and only when it names nothing it must not (arr::shareable). */
    std::string message;
};

struct DiskSpace {
    config::Service from = config::Sonarr;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    std::string path, label;
    int64_t free = 0, total = 0;
};

struct CalendarItem {
    config::Service from = config::Sonarr;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    int media_id = 0;        /* Sonarr's series id, Radarr's movie id (for mediacover) */
    std::string title;       /* the series or the film */
    int season = 0, episode = 0;   /* Sonarr */
    std::string episode_title;
    enum Release { Air, Cinema, Digital, Physical } release = Air;
    time_t when = 0;         /* UTC */
    bool has_file = false;
    int year = 0;            /* Radarr */
};

struct QueueItem {
    config::Service from = config::Sonarr;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    int id = 0;
    int media_id = 0;        /* series or movie id, 0 when unknown */
    std::string title;       /* the series or the film, never the release */
    int season = 0, episode = 0;
    std::string episode_title;
    int year = 0;
    double progress = 0;     /* 0..1 */
    int64_t time_left = -1;  /* seconds, -1 unknown */
    int64_t size = 0, size_left = 0;
    std::string quality;     /* "Bluray-1080p" */
    enum Status { Downloading, Queued, Paused, Importing, Completed, Warning, Error } status = Queued;
};

/* ---- Sonarr / Radarr libraries (read-only) ---- */

struct SeasonStats {
    int number = 0;
    bool monitored = true;
    int files = 0;      /* episodes with a file */
    int episodes = 0;   /* aired, monitored episodes (what should be there) */
    int total = 0;      /* every episode of the season, aired or not */
    int64_t size = 0;
    time_t next_airing = 0;
};

struct Series {
    int id = 0;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    std::string title, sort_title;   /* sort_title: lower case, without leading article */
    int year = 0;
    enum Status { Continuing, Ended, Upcoming, Deleted } status = Continuing;
    bool monitored = true;
    time_t added = 0, next_airing = 0, previous_airing = 0;
    int quality_profile = 0;
    std::string network, overview;
    int runtime = 0;
    int files = 0, episodes = 0, total = 0;
    int64_t size = 0;
    std::vector<SeasonStats> seasons;
    int tvdb_id = 0, tmdb_id = 0;   /* to find it in Jellyfin (the cleanup) */
    std::string imdb_id;
    std::string root;   /* its root folder's last name ("tv4k"): which Jellyfin library holds it */
    int missing() const { return episodes > files ? episodes - files : 0; }
};

struct Movie {
    int id = 0;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    std::string title, sort_title;
    int year = 0;
    enum Status { Tba, Announced, InCinemas, Released, Deleted } status = Released;
    bool monitored = true, has_file = false, available = false;
    time_t added = 0, in_cinemas = 0, digital = 0, physical = 0;
    int quality_profile = 0;
    int64_t size = 0;
    std::string file_quality;   /* its file's quality ("Bluray-1080p"), never its name */
    int tmdb_id = 0;            /* to find it in Jellyfin (the cleanup) */
    std::string imdb_id;
    std::string root;           /* its root folder's last name ("movies4k") */
    std::string studio, overview;
    int runtime = 0;
    /* The next release still to come, 0 when none. */
    time_t next_release(time_t now) const
    {
        time_t best = 0;
        for (time_t t : {in_cinemas, digital, physical})
            if (t > now && (!best || t < best))
                best = t;
        return best;
    }
};

/* An aired episode without its file (Sonarr's wanted/missing). */
struct MissingEpisode {
    int series_id = 0, episode_id = 0;
    int instance = 0;        /* which Sonarr / Radarr (config.json may list several) */
    std::string series_title;
    int season = 0, episode = 0;
    std::string title;
    time_t aired = 0;
};

struct QualityProfile {
    int id = 0;
    std::string name;
};

/* What a series' files are, by quality: never their names. */
struct FileSummary {
    std::vector<std::pair<std::string, int>> qualities;   /* name, how many files; most first */
    int64_t size = 0;
};

/* ---- Seerr ---- */

struct Request {
    int id = 0;
    enum Status { Pending = 1, Approved, Declined, Failed, Completed } status = Pending;
    bool tv = false;
    bool is4k = false;
    int tmdb_id = 0;
    int media_status = 0;           /* Seerr's MediaStatus (5 available ...), in the request's quality */
    std::string requested_by;       /* display name */
    std::string modified_by;        /* who approved or declined it (display name) */
    time_t created = 0;
    time_t updated = 0;             /* its last change */
    time_t media_added = 0;         /* when the media became available, 0 unknown */
    std::string jellyfin_id;        /* the media in Jellyfin, once available (in the request's quality) */
    std::vector<int> seasons;
};

/* A problem someone reported on a title in Seerr (video, audio, subtitles),
 * and what was said about it. The users' display names only. */
struct IssueComment {
    std::string author;   /* display name; empty in a list (Seerr sends it with the issue alone) */
    std::string message;
    time_t created = 0;
};

struct Issue {
    int id = 0;
    enum Type { Video = 1, Audio, Subtitles, Other } type = Other;
    bool resolved = false;
    bool tv = false;
    int tmdb_id = 0;
    int season = 0, episode = 0;   /* 0: the whole title, or the whole season */
    std::string reported_by;       /* display name */
    std::string modified_by;       /* who last changed its status */
    time_t created = 0, updated = 0;
    std::string jellyfin_id;
    std::vector<IssueComment> comments;   /* the first one is the report itself */
};

struct IssuePage {
    std::vector<Issue> issues;
    int total = 0;
};

/* An import into the library (Sonarr/Radarr history). Only what the app
 * shows: the release it came from is never read. */
struct ImportEvent {
    config::Service from = config::Sonarr;
    int instance = 0;              /* which Sonarr / Radarr (config.json may list several) */
    time_t when = 0;
    int media_id = 0;              /* series or film */
    std::string title;             /* the series or the film */
    int season = 0, episode = 0;   /* Sonarr */
    std::string episode_title;
    int year = 0;
    std::string quality;           /* "WEBDL-1080p" */
};

/* An entry of Jellyfin's activity log: its kind, who, what and when. Never its
 * text (Name, Overview): it holds addresses and device names. */
struct ActivityEntry {
    time_t when = 0;
    std::string type;      /* "VideoPlayback", "SessionStarted"... */
    std::string user_id;
    std::string item_id;
};

/* One user's view of a film or a series in Jellyfin (the cleanup). */
struct UserItem {
    std::string id;
    bool series = false;
    int tmdb_id = 0, tvdb_id = 0;
    std::string imdb_id;
    bool played = false, favorite = false;
    time_t last_played = 0;
    int unplayed = -1;   /* series: episodes left to watch, -1 unknown */
};

/* A Jellyfin library: its name and kind, and its folders' last names (to
 * tell which Radarr / Sonarr feeds it; the paths are never shown). */
struct JellyfinLibrary {
    std::string id, name;
    enum Kind { Movies, Shows, Collections, Other } kind = Other;
    std::vector<std::string> folders;
};

struct JellyfinUser {
    std::string id, name;
    bool disabled = false;
    bool admin = false;
    time_t last_activity = 0;
};

/* An item named by its id (a film, an episode, a series). */
struct JellyfinItem {
    std::string id, name, type;     /* type: "Movie", "Episode", "Series" */
    std::string series_name;        /* episodes */
    int season = 0, episode = 0, year = 0;
};

struct RequestPage {
    std::vector<Request> requests;
    int total = 0, pages = 0, page = 0;
};

/* A title's details from Seerr (TMDB data through Seerr). */
struct MediaDetail {
    int tmdb_id = 0;
    bool tv = false;
    std::string title;
    int year = 0;
    std::string poster_path, backdrop_path;   /* TMDB paths: through /imageproxy/tmdb */
    int media_id = 0;          /* Seerr's own record of it (mediaInfo), 0 when none */
    int media_status = 0, media_status4k = 0;
};

/* ---- Seerr: discover, search and new requests ---- */

/* Where a title stands in Seerr (its MediaStatus): 1 unknown (never asked
 * for), 2 pending, 3 processing, 4 partly available, 5 available,
 * 6 blocklisted, 7 deleted (may be asked for again). 0: Seerr has no record. */
enum SeerrStatus { SeerrNone = 0, SeerrUnknown = 1, SeerrPending, SeerrProcessing, SeerrPartial, SeerrAvailable,
                   SeerrBlocklisted, SeerrDeleted };

/* A film or a series as search and discover list them. */
struct SeerrTitle {
    int tmdb_id = 0;
    bool tv = false;
    std::string title, overview;
    std::string poster_path, backdrop_path;   /* TMDB paths: through /imageproxy/tmdb */
    int year = 0;
    int status = SeerrNone, status4k = SeerrNone;
    std::string jellyfin_id;                  /* the media server's item (Jellyfin or Emby) */
};

struct SeerrSeason {
    int number = 0;
    std::string name;
    int episodes = 0;
    int status = SeerrNone, status4k = SeerrNone;   /* on the media server */
    bool requested = false, requested4k = false;    /* in a request still pending or approved */
};

/* A title's page: films and series. */
struct SeerrDetail {
    SeerrTitle title;
    std::vector<std::string> genres;
    int runtime = 0;   /* minutes: a film's, a series' episodes' */
    int tvdb_id = 0;   /* series */
    std::vector<SeerrSeason> seasons;   /* series, specials (0) included */
};

/* A Radarr or Sonarr server as Seerr has it set up, with its defaults; the
 * profiles and folders come with its details. */
struct SeerrServer {
    int id = 0;
    std::string name;
    bool is4k = false, is_default = false;
    int profile_id = 0;
    std::string folder;
    std::vector<std::pair<int, std::string>> profiles;   /* id, name */
    std::vector<std::string> folders;
};

/* What Seerr lets everyone know that a request depends on. */
struct SeerrPublic {
    bool partial_requests = true;   /* a series' seasons may be asked for one by one */
    bool special_episodes = false;  /* season 0 may be asked for */
};

/* What becomes of a new request. */
struct SeerrCreated {
    int id = 0;       /* 0: Seerr made none (nothing left to ask for) */
    int status = 0;   /* the request's: 1 pending, 2 approved */
};

/* ---- Jellyfin ---- */

struct Session {
    std::string id;
    std::string user, device, client;
    std::string item_id, item_name, item_type;   /* "Episode", "Movie" ... */
    std::string series_id, series_name;
    int season = 0, episode = 0;
    int64_t position_ticks = 0, runtime_ticks = 0;   /* 10,000,000 a second */
    bool paused = false;
    enum Method { DirectPlay, DirectStream, Transcode } method = DirectPlay;
    bool video_direct = true, audio_direct = true;
    std::vector<std::string> transcode_reasons;   /* Jellyfin's TranscodeReason names */
    /* What comes out (TranscodingInfo). */
    std::string video_codec, audio_codec, container_out;
    int width = 0, height = 0, audio_channels = 0;
    int64_t bitrate = 0;
    double framerate = 0;
    std::string hw_accel;        /* "none", "vaapi", "qsv"... empty: unknown */
    double completion = -1;      /* how far the transcode has gone, percent of the item; < 0 unknown */
    /* What goes in: the file's streams, and those chosen. */
    struct Stream {
        enum Kind { Video, Audio, Subtitle, Other } kind = Other;
        int index = -1;
        std::string codec, profile, range, language;   /* range: "SDR", "HDR10", "DOVI"... */
        int width = 0, height = 0, channels = 0, bit_depth = 0;
        int64_t bitrate = 0;
        bool text = false;       /* a subtitle in text (SRT...), not a picture (PGS...) */
    };
    std::vector<Stream> streams;
    std::string container_in;
    int audio_index = -1, subtitle_index = -1;
};

/* The server's load (Glances, optional): percentages. */
struct ServerLoad {
    double cpu = 0, mem = 0, swap = 0, load = 0;
    int cores = 0;
    std::string cpu_name;
};

/* Jellyfin's transcoding settings that the diagnosis needs. */
struct EncodingInfo {
    std::string hw_accel;   /* "none", "vaapi"... */
    bool throttling = false;
    int throttle_delay = 0;   /* seconds the transcode may run ahead before it is held */
};

struct Task {
    std::string id, key, name, category;
    enum State { Idle, Running, Cancelling } state = Idle;
    double progress = 0;   /* percent, while running */
    enum Result { Never, Completed, Failed, Cancelled, Aborted } last = Never;
    time_t last_end = 0;
};

struct RecentItem {
    std::string id;          /* the item (a film, an episode) */
    std::string series_id;   /* an episode's series */
    std::string image_id;    /* whose Primary image to show (an episode: its series) */
    std::string image_tag;
    std::string blurhash;
    std::string backdrop_id; /* whose backdrop for the ambient background */
    std::string title;       /* the film, or the series */
    std::string type;        /* "Movie", "Episode" */
    int year = 0;
    int season = 0, episode = 0;
    std::string episode_title;
    int new_episodes = 1;    /* a series: how many of its episodes were in the list */
    time_t added = 0;
};

/* ---- Every service: what the connection test found ---- */

struct ServiceStatus {
    enum State { Unknown, Disabled, NotConfigured, Testing, Ok, Failed } state = Unknown;
    std::string version;   /* when Ok */
    int error = 0;         /* api::Error when Failed */
    int http_status = 0;
};

} // namespace model
