#!/usr/bin/env python3
# Regie5 — rebuilds fixtures/ from real servers, anonymised.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-fixtures.py [--config PATH] [--only GROUP[,GROUP...]]
#   groups: library, guide, journal, diag, cleanup (only these files are rebuilt, the others are left)
#
# Reads only (GET) from the services in config.json, then, before anything is
# written:
#   - drops what must never be shown or kept: release and file names, paths,
#     download clients, indexers, e-mail addresses, server addresses;
#   - replaces user names, device names and ids by made-up ones;
#   - withholds health messages about download clients and indexers.
# Cases the servers may not have right now (a queue, pending requests, a
# transcoding session, a running task) are derived from the real answers and
# marked "_regie5_synthetic". Finally every output file is checked for the API
# keys, the server addresses, the original user and device names and anything
# that looks like a release name; if one is found, nothing is written.
# The API keys are never printed.

import argparse
import copy
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "fixtures")
SALT = "regie5-fixtures"

DROP = {
    # release, files, download clients, indexers
    "sceneName", "releaseGroup", "relativePath", "originalFilePath", "movieFile", "episodeFile",
    "downloadClient", "downloadId", "indexer", "outputPath", "statusMessages", "errorMessage",
    "downloadStatus", "downloadStatus4k",   # Seerr: the queue's items, release titles included
    "releaseGroups",   # Sonarr/Radarr statistics: release group names
    # paths on the server
    "path", "Path", "Paths", "folderName", "rootFolderPath", "startupPath", "appData", "appDataPath",
    "rootFolder", "ProgramDataPath", "WebPath", "ItemsByNamePath", "CachePath", "LogPath", "InternalMetadataPath",
    "TranscodingTempPath", "EncoderLocation",
    # addresses and people
    "RemoteEndPoint", "LocalAddress", "email", "plexUsername", "plexId", "avatar", "avatarETag",
    "recoveryLinkExpirationDate", "serviceUrl", "mediaUrl", "ErrorMessage", "LongErrorMessage",
    # internet links never used
    "remoteUrl",
}
NAME_KEYS = {"UserName", "jellyfinUsername", "username", "displayName"}
DEVICE_KEYS = {"DeviceName"}
PEOPLE = ["Alice", "Bruno", "Chloé", "David", "Emma", "Farid", "Gaëlle", "Hugo"]
DEVICES = ["Salon TV", "Chambre", "Téléphone", "Tablette", "Bureau", "Cuisine"]
HEX32 = re.compile(r"^[0-9a-f]{32}$")
RELEASE = re.compile(r"\b[A-Za-z][A-Za-z0-9]*(\.[A-Za-z0-9]+){2,}-[A-Za-z0-9]+|\.(mkv|mp4|avi)\b|\b(x264|x265|WEB-DL|BluRay)\.")


class Anon:
    def __init__(self):
        self.names = {}
        self.devices = {}
        self.originals = set()

    def fake_id(self, v):
        return hashlib.sha1((SALT + v).encode()).hexdigest()[:32]

    def name(self, v):
        if not v:
            return v
        self.originals.add(v)
        if v not in self.names:
            self.names[v] = PEOPLE[len(self.names) % len(PEOPLE)]
        return self.names[v]

    def device(self, v):
        if not v:
            return v
        self.originals.add(v)
        if v not in self.devices:
            self.devices[v] = DEVICES[len(self.devices) % len(DEVICES)]
        return self.devices[v]

    def clean(self, v):
        if isinstance(v, dict):
            out = {}
            for k, x in v.items():
                if k in DROP:
                    continue
                kk = self.fake_id(k) if HEX32.match(k) else k
                if k in NAME_KEYS and isinstance(x, str):
                    out[kk] = self.name(x)
                elif k in DEVICE_KEYS and isinstance(x, str):
                    out[kk] = self.device(x)
                elif k == "DeviceId" and isinstance(x, str):   # browsers encode their user agent here
                    out[kk] = self.fake_id(x)
                else:
                    out[kk] = self.clean(x)
            return out
        if isinstance(v, list):
            return [self.clean(x) for x in v]
        if isinstance(v, str) and HEX32.match(v):
            return self.fake_id(v)
        return v


def get(cfg, service, path):
    s = cfg[service]
    if service == "jellyfin":
        headers = {"Authorization": 'MediaBrowser Token="%s"' % s["apiKey"]}
    else:
        headers = {"X-Api-Key": s["apiKey"]}
    headers["Accept"] = "application/json"
    req = urllib.request.Request(s["url"].rstrip("/") + path, headers=headers, method="GET")
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.loads(r.read())


def iso(t):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(t))


def generic_source(src):
    return any(p in src for p in ("Indexer", "Prowlarr", "Jackett", "DownloadClient", "RemotePathMapping",
                                  "ImportMechanism"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=os.path.expanduser("~/.config/regie5/config.json"))
    ap.add_argument("--only", help="rebuild only these groups (library, guide, journal, diag, cleanup), leave the other files")
    args = ap.parse_args()
    groups = set(args.only.split(",")) if args.only else set()
    unknown = groups - {"library", "guide", "journal", "diag", "cleanup"}
    if unknown:
        ap.error("unknown group: " + ", ".join(sorted(unknown)))
    cfg = json.load(open(args.config))
    a = Anon()
    files = {}
    now = int(time.time())
    start, end = iso(now - 86400), iso(now + 14 * 86400)

    def calendar(svc):
        """Three weeks: last week, this one and the next (the guide), which also
        covers the home row's next 14 days."""
        day = time.localtime(now)
        monday = now - day.tm_wday * 86400 - day.tm_hour * 3600 - day.tm_min * 60 - day.tm_sec
        extra = "&includeSeries=true" if svc == "sonarr" else ""
        return a.clean(get(cfg, svc, "/api/v3/calendar?start=%s&end=%s&unmonitored=false%s" %
                           (iso(monday - 7 * 86400), iso(monday + 21 * 86400), extra)))

    if "guide" in groups:
        for svc in ("sonarr", "radarr"):
            files[svc + "/calendar.json"] = calendar(svc)
        cal = files["sonarr/calendar.json"]
        if len(cal) < 30:   # a fuller week to show: episodes of real series, marked synthetic
            day = time.localtime(now)
            monday = now - day.tm_wday * 86400 - day.tm_hour * 3600 - day.tm_min * 60 - day.tm_sec
            shows = [x for x in get(cfg, "sonarr", "/api/v3/series") if x.get("monitored")][:9]
            n = 0
            for d in range(-7, 14):
                for k in range((d * 7 + 3) % 3 + 1):
                    sh = shows[(d * 5 + k * 3) % len(shows)]
                    when = monday + d * 86400 + (19 + k) * 3600 + (k % 2) * 1800
                    n += 1
                    cal.append({"id": 900000 + n, "seriesId": sh["id"], "seasonNumber": 1 + n % 3,
                                "episodeNumber": 1 + n % 10, "title": "", "airDateUtc": iso(when),
                                "hasFile": when < now and n % 4 != 0, "monitored": True,
                                "series": {"id": sh["id"], "title": sh["title"], "year": sh.get("year", 0)},
                                "_regie5_synthetic": True})
            cal.sort(key=lambda e: e.get("airDateUtc", ""))

    if "journal" in groups:
        since = iso(now - 14 * 86400)
        # Imports only: the release (sourceTitle), data and downloadId are never kept.
        def pick_keys(d, keys):
            return {k: d[k] for k in keys if k in d}
        for svc, inc in (("sonarr", "&includeSeries=true&includeEpisode=true"), ("radarr", "&includeMovie=true")):
            hist = get(cfg, svc, "/api/v3/history/since?date=%s&eventType=downloadFolderImported%s" % (since, inc))
            out = []
            for h in hist[:120]:
                e = pick_keys(h, ("id", "eventType", "date", "seriesId", "episodeId", "movieId"))
                q = (h.get("quality") or {}).get("quality") or {}
                e["quality"] = {"quality": pick_keys(q, ("id", "name"))}
                if "series" in h:
                    e["series"] = pick_keys(h["series"], ("id", "title", "year"))
                if "episode" in h:
                    e["episode"] = pick_keys(h["episode"], ("id", "seasonNumber", "episodeNumber", "title"))
                if "movie" in h:
                    e["movie"] = pick_keys(h["movie"], ("id", "title", "year"))
                out.append(e)
            files[svc + "/history_since.json"] = out
        # Jellyfin: users (names replaced), the activity log without its text
        # (Name, ShortOverview, Overview hold addresses and device names), and
        # the items it names.
        users = get(cfg, "jellyfin", "/Users")
        files["jellyfin/users.json"] = [
            {"Id": a.fake_id(u["Id"]), "Name": a.name(u.get("Name", "")),
             "Policy": pick_keys(u.get("Policy") or {}, ("IsDisabled", "IsAdministrator")),
             "LastActivityDate": u.get("LastActivityDate")} for u in users]
        log = get(cfg, "jellyfin", "/System/ActivityLog/Entries?minDate=%s&hasUserId=true&limit=300" % since)
        entries, ids = [], []
        for e in log.get("Items", []):
            keep = {"Id": e.get("Id"), "Type": e.get("Type"), "Date": e.get("Date"), "Severity": e.get("Severity")}
            if e.get("UserId"):
                keep["UserId"] = a.fake_id(e["UserId"])
            if e.get("ItemId"):
                keep["ItemId"] = a.fake_id(e["ItemId"])
                ids.append(e["ItemId"])
            entries.append(keep)
        files["jellyfin/system_activitylog_entries.json"] = {"Items": entries, "TotalRecordCount": len(entries)}
        named = []
        for i in range(0, len(ids), 40):
            chunk = sorted(set(ids[i:i + 40]))
            r = get(cfg, "jellyfin", "/Items?ids=%s&Fields=ProductionYear" % ",".join(chunk))
            for it in r.get("Items", []):
                named.append(dict(pick_keys(it, ("Name", "Type", "SeriesName", "ParentIndexNumber", "IndexNumber",
                                                  "ProductionYear")), Id=a.fake_id(it["Id"])))
        files["jellyfin/items_ids.json"] = {"Items": named, "TotalRecordCount": len(named)}

    if "diag" in groups:
        # Sessions: the real ones, and two transcodes to diagnose (marked synthetic):
        # picture subtitles + HDR on the CPU, falling behind; over the client's
        # bitrate limit with VAAPI, keeping ahead.
        sessions = a.clean([s for s in get(cfg, "jellyfin", "/Sessions?activeWithinSeconds=960")])
        for s in sessions:
            s.pop("NowPlayingQueue", None)
            s.pop("NowPlayingQueueFullItems", None)
        playing = [s for s in sessions if s.get("NowPlayingItem")]
        if playing:
            base = playing[0]
            runtime = base["NowPlayingItem"].get("RunTimeTicks") or 60 * 60 * 10 ** 7
            def transcode(n, user, device, reasons, info, streams, sub_index, played, ahead_s):
                t = copy.deepcopy(base)
                t["Id"] = a.fake_id("synthetic-transcode-%d" % n)
                t["UserName"], t["DeviceName"], t["Client"] = user, device, "Jellyfin Web"
                ps = t["PlayState"]
                ps.update({"PlayMethod": "Transcode", "IsPaused": False, "PositionTicks": int(runtime * played),
                           "SubtitleStreamIndex": sub_index, "AudioStreamIndex": 1})
                t["NowPlayingItem"]["MediaStreams"] = streams
                info = dict(info)
                info["TranscodeReasons"] = reasons
                info["CompletionPercentage"] = round(100.0 * played + 100.0 * ahead_s * 10 ** 7 / runtime, 2)
                t["TranscodingInfo"] = info
                t["_regie5_synthetic"] = True
                return t
            hdr_video = {"Type": "Video", "Index": 0, "Codec": "hevc", "Profile": "Main 10", "VideoRangeType": "HDR10",
                         "VideoRange": "HDR", "BitDepth": 10, "Width": 3840, "Height": 2160, "BitRate": 58000000}
            sdr_video = {"Type": "Video", "Index": 0, "Codec": "h264", "Profile": "High", "VideoRangeType": "SDR",
                         "VideoRange": "SDR", "BitDepth": 8, "Width": 1920, "Height": 1080, "BitRate": 22000000}
            truehd = {"Type": "Audio", "Index": 1, "Codec": "truehd", "Channels": 8, "Language": "eng", "BitRate": 4500000}
            ac3 = {"Type": "Audio", "Index": 1, "Codec": "ac3", "Channels": 6, "Language": "fre", "BitRate": 640000}
            pgs = {"Type": "Subtitle", "Index": 2, "Codec": "PGSSUB", "Language": "fre", "IsTextSubtitleStream": False}
            srt = {"Type": "Subtitle", "Index": 2, "Codec": "subrip", "Language": "fre", "IsTextSubtitleStream": True}
            sessions = [s for s in sessions if s.get("PlayState", {}).get("PlayMethod") != "Transcode"]
            sessions.append(transcode(1, PEOPLE[-1], DEVICES[-1],
                                      ["SubtitleCodecNotSupported", "VideoRangeTypeNotSupported", "AudioCodecNotSupported"],
                                      {"VideoCodec": "h264", "AudioCodec": "aac", "Container": "ts", "IsVideoDirect": False,
                                       "IsAudioDirect": False, "Bitrate": 12000000, "Width": 1920, "Height": 1080,
                                       "AudioChannels": 2, "Framerate": 23.976, "HardwareAccelerationType": "none"},
                                      [hdr_video, truehd, pgs], 2, 0.42, 9))
            sessions.append(transcode(2, PEOPLE[-2], DEVICES[-2], ["ContainerBitrateExceedsLimit"],
                                      {"VideoCodec": "h264", "AudioCodec": "ac3", "Container": "ts", "IsVideoDirect": False,
                                       "IsAudioDirect": True, "Bitrate": 6000000, "Width": 1920, "Height": 1080,
                                       "AudioChannels": 6, "Framerate": 23.976, "HardwareAccelerationType": "vaapi"},
                                      [sdr_video, ac3, srt], -1, 0.18, 410))
        files["jellyfin/sessions.json"] = sessions
        enc = get(cfg, "jellyfin", "/System/Configuration/encoding")
        files["jellyfin/system_configuration_encoding.json"] = {k: enc[k] for k in (
            "HardwareAccelerationType", "EnableThrottling", "ThrottleDelaySeconds") if k in enc}
        # Glances, from its documented answer (not installed here): marked synthetic.
        files["glances/quicklook.json"] = {
            "cpu": 71.4, "mem": 63.2, "swap": 4.1, "load": 58.0, "cpu_log_core": 6, "cpu_phys_core": 6,
            "cpu_name": "Intel(R) Core(TM) i5-8500T CPU @ 2.10GHz", "gpu_mem": 0, "gpu_proc": 0,
            "_regie5_synthetic": True}

    if "cleanup" in groups:
        # Who watched what, per user: ids, played, last played, favourite only.
        # Users as in the journal (names replaced, ids hashed).
        users = get(cfg, "jellyfin", "/Users")
        files["jellyfin/users.json"] = [
            {"Id": a.fake_id(u["Id"]), "Name": a.name(u.get("Name", "")),
             "Policy": {k: (u.get("Policy") or {}).get(k) for k in ("IsDisabled", "IsAdministrator")},
             "LastActivityDate": u.get("LastActivityDate")} for u in users]
        for u in users:
            r = get(cfg, "jellyfin", "/Items?userId=%s&Recursive=true&IncludeItemTypes=Movie,Series"
                                     "&Fields=ProviderIds&EnableUserData=true&EnableImages=false" % u["Id"])
            kept = []
            for it in r.get("Items", []):
                ud = it.get("UserData") or {}
                kept.append({"Id": a.fake_id(it["Id"]), "Name": it.get("Name"), "Type": it.get("Type"),
                             "ProviderIds": {k: v for k, v in (it.get("ProviderIds") or {}).items()
                                             if k in ("Tmdb", "Tvdb", "Imdb")},
                             "UserData": {k: ud[k] for k in ("Played", "IsFavorite", "LastPlayedDate",
                                                             "UnplayedItemCount", "PlayCount") if k in ud}})
            files["jellyfin/items_user_%s.json" % a.fake_id(u["Id"])] = {"Items": kept,
                                                                         "TotalRecordCount": len(kept)}
        # Every film and series with its ids, no user's data: "Ouvrir dans Jelly5".
        r = get(cfg, "jellyfin", "/Items?Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds"
                                 "&EnableUserData=false&EnableImages=false")
        index = [{"Id": a.fake_id(it["Id"]), "Name": it.get("Name"), "Type": it.get("Type"),
                  "ProviderIds": {k: v for k, v in (it.get("ProviderIds") or {}).items()
                                  if k in ("Tmdb", "Tvdb", "Imdb")}} for it in r.get("Items", [])]
        files["jellyfin/items_index.json"] = {"Items": index, "TotalRecordCount": len(index)}

    if not groups:
        for svc in ("sonarr", "radarr"):
            files[svc + "/system_status.json"] = a.clean(get(cfg, svc, "/api/v3/system/status"))
            health = get(cfg, svc, "/api/v3/health")
            for h in health:
                if generic_source(h.get("source", "")):
                    h["message"] = "[withheld: names a download client or an indexer]"
            if svc == "radarr" and len(health) < 3:   # a fuller list of alerts to show
                health += [
                    {"source": "IndexerLongTermStatusCheck", "type": "warning",
                     "message": "[synthetic: names an indexer]", "_regie5_synthetic": True},
                    {"source": "RootFolderCheck", "type": "error",
                     "message": "[synthetic: names a path]", "_regie5_synthetic": True},
                ]
            files[svc + "/health.json"] = a.clean(health)
            disks = get(cfg, svc, "/api/v3/diskspace")
            for i, d in enumerate(disks):
                d["path"] = "/mnt/disk%d" % (i + 1)
                d["label"] = ""
            files[svc + "/diskspace.json"] = [{k: v for k, v in d.items() if k not in ("id",)} for d in disks]
            files[svc + "/calendar.json"] = calendar(svc)
            q = get(cfg, svc, "/api/v3/queue?page=1&pageSize=50" +
                    ("&includeSeries=true&includeEpisode=true" if svc == "sonarr" else "&includeMovie=true"))
            for r in q.get("records", []):
                r.pop("title", None)   # the release
            files[svc + "/queue.json"] = a.clean(q)

        # A queue to show when the servers' is empty: built from the calendar's real titles.
        cal_s = files["sonarr/calendar.json"]
        if not files["sonarr/queue.json"].get("records") and cal_s:
            recs = []
            specs = [("downloading", "ok", "downloading", 0.38, 1260), ("queued", "ok", "downloading", 1.0, None),
                     ("warning", "warning", "importBlocked", 0.0, None)]
            for i, (st, tds, tdst, left, secs) in enumerate(specs):
                e = cal_s[i % len(cal_s)]
                size = 2.4e9
                recs.append({
                    "id": 9000 + i, "seriesId": e["seriesId"], "episodeId": e["id"], "seasonNumber": e["seasonNumber"],
                    "series": e.get("series"), "episode": {k: v for k, v in e.items() if k != "series"},
                    "quality": {"quality": {"id": 3, "name": "WEBDL-1080p", "source": "web", "resolution": 1080},
                                "revision": {"version": 1, "real": 0, "isRepack": False}},
                    "size": size, "sizeleft": size * left,
                    "estimatedCompletionTime": iso(now + secs) if secs else None,
                    "timeleft": time.strftime("%H:%M:%S", time.gmtime(secs)) if secs else None,
                    "added": iso(now - 3600), "status": st, "trackedDownloadStatus": tds,
                    "trackedDownloadState": tdst, "protocol": "usenet", "episodeHasFile": False,
                    "_regie5_synthetic": True,
                })
            files["sonarr/queue.json"] = {"page": 1, "pageSize": 50, "sortKey": "timeleft", "sortDirection": "ascending",
                                          "totalRecords": len(recs), "records": recs}
        cal_r = files["radarr/calendar.json"]
        if not files["radarr/queue.json"].get("records") and cal_r:
            recs = []
            specs = [("downloading", "ok", "downloading", 0.71, 3725), ("completed", "ok", "importPending", 0.0, None),
                     ("failed", "error", "failed", 0.9, None)]
            for i, (st, tds, tdst, left, secs) in enumerate(specs):
                m = cal_r[i % len(cal_r)]
                size = 8.2e9
                recs.append({
                    "id": 9100 + i, "movieId": m["id"], "movie": m,
                    "quality": {"quality": {"id": 7, "name": "Bluray-1080p", "source": "bluray", "resolution": 1080},
                                "revision": {"version": 1, "real": 0, "isRepack": False}},
                    "size": size, "sizeleft": size * left,
                    "estimatedCompletionTime": iso(now + secs) if secs else None,
                    "timeleft": time.strftime("%H:%M:%S", time.gmtime(secs)) if secs else None,
                    "added": iso(now - 7200), "status": st, "trackedDownloadStatus": tds,
                    "trackedDownloadState": tdst, "protocol": "usenet",
                    "_regie5_synthetic": True,
                })
            files["radarr/queue.json"] = {"page": 1, "pageSize": 50, "sortKey": "timeleft", "sortDirection": "ascending",
                                          "totalRecords": len(recs), "records": recs}

        # Seerr
        about = get(cfg, "seerr", "/api/v1/settings/about")
        files["seerr/settings_about.json"] = {k: v for k, v in about.items() if k in ("version", "totalMediaItems",
                                                                                      "totalRequests", "tz")}
        def requests(filt):
            page = get(cfg, "seerr", "/api/v1/request?take=20&skip=0&filter=%s&sort=added&sortDirection=desc" % filt)
            for r in page["results"]:
                for who in ("requestedBy", "modifiedBy"):
                    if isinstance(r.get(who), dict):
                        u = r[who]
                        r[who] = {"id": u.get("id"), "displayName": u.get("displayName"), "userType": u.get("userType"),
                                  "jellyfinUsername": u.get("jellyfinUsername")}
            return a.clean(page)
        all_page = requests("all")
        files["seerr/request_all.json"] = all_page
        files["seerr/request_approved.json"] = requests("approved")
        pending = requests("pending")
        if not pending["results"]:
            pending = copy.deepcopy(all_page)
            pending["results"] = pending["results"][:5]
            for r in pending["results"]:
                r["status"] = 1
                r["modifiedBy"] = None
                r["media"]["status"] = 2
                r["_regie5_synthetic"] = True
            pending["pageInfo"] = {"pages": 1, "pageSize": 20, "results": len(pending["results"]), "page": 1}
        files["seerr/request_pending.json"] = pending
        keep = ("id", "title", "name", "originalTitle", "originalName", "releaseDate", "firstAirDate", "posterPath",
                "backdropPath", "overview", "numberOfSeasons")
        seen = set()
        for page in (all_page, files["seerr/request_approved.json"], pending):
            for r in page["results"]:
                media = r.get("media") or {}
                tv = r.get("type") == "tv"
                key = ("tv" if tv else "movie", media.get("tmdbId"))
                if not media.get("tmdbId") or key in seen:
                    continue
                seen.add(key)
                d = get(cfg, "seerr", "/api/v1/%s/%d?language=fr" % key)
                files["seerr/%s_%d.json" % key] = {k: v for k, v in d.items() if k in keep}

        # Jellyfin
        info = get(cfg, "jellyfin", "/System/Info")
        info["ServerName"] = "Jellyfin"
        info.pop("CastReceiverApplications", None)
        files["jellyfin/system_info.json"] = a.clean(info)
        sessions = a.clean([s for s in get(cfg, "jellyfin", "/Sessions?activeWithinSeconds=960")])
        for s in sessions:
            s.pop("NowPlayingQueue", None)
            s.pop("NowPlayingQueueFullItems", None)
        playing = [s for s in sessions if s.get("NowPlayingItem")]
        if playing and not any(s.get("PlayState", {}).get("PlayMethod") == "Transcode" for s in playing):
            t = copy.deepcopy(playing[0])
            t["Id"] = a.fake_id("synthetic-transcode")
            t["UserName"] = PEOPLE[-1]
            t["DeviceName"] = DEVICES[-1]
            t["Client"] = "Jellyfin Web"
            t["PlayState"]["PlayMethod"] = "Transcode"
            t["PlayState"]["IsPaused"] = True
            t["TranscodingInfo"] = {"AudioCodec": "aac", "VideoCodec": "h264", "Container": "ts", "IsVideoDirect": False,
                                    "IsAudioDirect": False, "Bitrate": 8000000, "Width": 1920, "Height": 1080,
                                    "AudioChannels": 2, "HardwareAccelerationType": "vaapi",
                                    "TranscodeReasons": ["VideoCodecNotSupported", "AudioCodecNotSupported"]}
            t["_regie5_synthetic"] = True
            sessions.append(t)
        files["jellyfin/sessions.json"] = sessions
        tasks = a.clean(get(cfg, "jellyfin", "/ScheduledTasks?isHidden=false"))
        if tasks and not any(t.get("State") == "Running" for t in tasks):
            tasks[0]["State"] = "Running"
            tasks[0]["CurrentProgressPercentage"] = 42.5
            tasks[0]["_regie5_synthetic"] = True
        files["jellyfin/scheduledtasks.json"] = tasks
        files["jellyfin/items.json"] = a.clean(get(
            cfg, "jellyfin", "/Items?SortBy=DateCreated&SortOrder=Descending&Recursive=true&IncludeItemTypes=Movie,Episode"
            "&IsMissing=false&Limit=40&Fields=DateCreated&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1"))

    # Libraries (Sonarr series, Radarr films, what is missing), trimmed to what the
    # app reads: never paths, file names, scene names or release groups.
    def pick(d, keys):
        return {k: d[k] for k in keys if k in d}

    if not groups or "library" in groups:
        stats_keys = ("seasonCount", "episodeFileCount", "episodeCount", "totalEpisodeCount", "sizeOnDisk",
                      "percentOfEpisodes", "nextAiring", "previousAiring")
        series = get(cfg, "sonarr", "/api/v3/series")
        files["sonarr/series.json"] = [dict(pick(s, ("id", "title", "sortTitle", "year", "status", "ended", "monitored",
                                                      "added", "nextAiring", "previousAiring", "qualityProfileId",
                                                      "network", "overview", "runtime", "tvdbId", "tmdbId",
                                                      "imdbId")),
                                            statistics=pick(s.get("statistics", {}), stats_keys),
                                            seasons=[dict(pick(x, ("seasonNumber", "monitored")),
                                                          statistics=pick(x.get("statistics", {}), stats_keys))
                                                     for x in s.get("seasons", [])])
                                       for s in series]
        for s in series:
            ef = get(cfg, "sonarr", "/api/v3/episodefile?seriesId=%d" % s["id"])
            files["sonarr/episodefile_%d.json" % s["id"]] = [
                dict(pick(e, ("id", "seriesId", "seasonNumber", "size")),
                     quality={"quality": pick(e.get("quality", {}).get("quality", {}), ("id", "name", "resolution"))})
                for e in ef]
        movie_keys = ("id", "title", "sortTitle", "year", "status", "monitored", "hasFile", "isAvailable", "added",
                      "inCinemas", "digitalRelease", "physicalRelease", "qualityProfileId", "sizeOnDisk", "studio",
                      "overview", "runtime", "tmdbId", "imdbId")

        def movie(m):
            out = pick(m, movie_keys)
            if isinstance(m.get("movieFile"), dict):
                mf = m["movieFile"]
                out["movieFile"] = {"size": mf.get("size"),
                                    "quality": {"quality": pick(mf.get("quality", {}).get("quality", {}),
                                                                ("id", "name", "resolution"))}}
            return out

        files["radarr/movie.json"] = [movie(m) for m in get(cfg, "radarr", "/api/v3/movie")]
        mis = get(cfg, "sonarr", "/api/v3/wanted/missing?page=1&pageSize=250&monitored=true&includeSeries=true")
        files["sonarr/wanted_missing.json"] = dict(
            pick(mis, ("page", "pageSize", "totalRecords")),
            records=[dict(pick(r, ("id", "seriesId", "seasonNumber", "episodeNumber", "title", "airDateUtc", "hasFile",
                                   "monitored")),
                          series=pick(r.get("series", {}), ("id", "title", "sortTitle", "year")))
                     for r in mis.get("records", [])])
        mis = get(cfg, "radarr", "/api/v3/wanted/missing?page=1&pageSize=250&monitored=true")
        files["radarr/wanted_missing.json"] = dict(pick(mis, ("page", "pageSize", "totalRecords")),
                                                   records=[movie(m) for m in mis.get("records", [])])
        for svc in ("sonarr", "radarr"):
            files[svc + "/qualityprofile.json"] = [pick(q, ("id", "name"))
                                                   for q in get(cfg, svc, "/api/v3/qualityprofile")]

    # The check: nothing that must not be kept.
    secrets = [cfg[s]["apiKey"] for s in cfg if isinstance(cfg[s], dict) and cfg[s].get("apiKey")]
    hosts = {urllib.parse.urlparse(cfg[s]["url"]).hostname for s in cfg if isinstance(cfg[s], dict) and cfg[s].get("url")}
    problems = []
    for name, data in files.items():
        text = json.dumps(data, ensure_ascii=False)
        if any(k and k in text for k in secrets):
            problems.append("%s: an API key" % name)
        if any(h and h in text for h in hosts):
            problems.append("%s: a server address" % name)
        if re.search(r"\b(192\.168|10\.\d+|172\.(1[6-9]|2\d|3[01]))\.\d+\.\d+", text):
            problems.append("%s: a private IP address" % name)
        for o in a.originals:
            if len(o) >= 3 and re.search(r'"%s"' % re.escape(o), text):
                problems.append("%s: an original user or device name" % name)
                break
        m = RELEASE.search(text)
        if m:
            problems.append("%s: something like a release or file name (%r)" % (name, m.group(0)[:60]))
    if problems:
        print("Nothing written:\n  " + "\n  ".join(sorted(set(problems))))
        return 1

    tmp = tempfile.mkdtemp(prefix="regie5-fixtures-")
    for name, data in files.items():
        p = os.path.join(tmp, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=1)
            f.write("\n")
    if args.only:   # these files only, over the old ones
        for name in files:
            os.makedirs(os.path.dirname(os.path.join(OUT, name)), exist_ok=True)
            shutil.move(os.path.join(tmp, name), os.path.join(OUT, name))
    else:
        for svc in ("sonarr", "radarr", "seerr", "jellyfin", "glances"):
            if not os.path.isdir(os.path.join(tmp, svc)):
                continue
            dst = os.path.join(OUT, svc)
            if os.path.isdir(dst):
                shutil.rmtree(dst)
            shutil.move(os.path.join(tmp, svc), dst)
    shutil.rmtree(tmp, ignore_errors=True)
    print("fixtures/: %d files written" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
