#!/usr/bin/env python3
# Regie5 — writes Seerr's discover, search and request fixtures for --mock and
# --demo: fixtures/seerr/ and demo/seerr/ (each from its own titles).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-discover-fixtures.py
#
# Entirely made up (no server is read). The titles are the folder's own
# (fixtures/: made-up ones; demo/: the Blender Foundation's open films), shaped
# as Seerr 3.x answers them (server/routes/discover.ts, search.ts, movie.ts,
# tv.ts, service.ts; the same fields Jelly5-Seerr reads on a real Seerr):
#   discover_trending, discover_movies, discover_tv, discover_movies_upcoming,
#   discover_tv_upcoming, search (the mock keeps the titles that match the
#   query), settings_public, service_radarr (a 1080p and a 4K server),
#   service_sonarr, service_<arr>_<id> (profiles and folders).
# The films' and series' pages (movie_*.json, tv_*.json) gain what a request
# needs: a runtime, genres, the seasons and where each stands (mediaInfo).
# Running it again gives the same files.

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def save(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


def statuses(folder):
    """What the folder's requests say of each title: tmdbId -> (media status, Jellyfin item)."""
    out = {}
    path = os.path.join(folder, "request_all.json")
    if os.path.exists(path):
        for r in load(path).get("results", []):
            m = r.get("media") or {}
            if m.get("tmdbId"):
                out[(r.get("type") == "tv", m["tmdbId"])] = (m.get("status", 1), m.get("jellyfinMediaId"),
                                                            r.get("status", 1), r.get("is4k", False),
                                                            [s.get("seasonNumber") for s in r.get("seasons", [])])
    return out


def media_info(tv, d, known, i):
    """mediaInfo: as the requests have it, else one of a few made-up states."""
    tmdb = d["id"]
    if (tv, tmdb) in known:
        status, item, req_status, is4k, seasons = known[(tv, tmdb)]
    else:
        status, item, req_status, is4k, seasons = [(1, None, 0, False, []), (5, "demo%07d" % tmdb, 0, False, []),
                                                   (1, None, 0, False, []), (4, "demo%07d" % tmdb, 0, False, [1])][i % 4]
    info = {"id": 900000 + i, "tmdbId": tmdb, "mediaType": "tv" if tv else "movie", "status": status, "status4k": 1,
            "jellyfinMediaId": item, "seasons": [], "requests": []}
    if req_status in (1, 2):
        info["requests"].append({"id": 800000 + i, "status": req_status, "is4k": is4k,
                                 "seasons": [{"seasonNumber": n} for n in seasons if n is not None]})
    return info, seasons


def enrich(folder, known):
    movies, series = [], []
    for name in sorted(os.listdir(folder)):
        if not (name.startswith("movie_") or name.startswith("tv_")) or not name.endswith(".json"):
            continue
        tv = name.startswith("tv_")
        path = os.path.join(folder, name)
        d = load(path)
        i = len(series if tv else movies)
        info, asked = media_info(tv, d, known, i)
        d["genres"] = [{"id": 18, "name": "Drama"}] if i % 2 else [{"id": 12, "name": "Adventure"}]
        if tv:
            n = max(int(d.get("numberOfSeasons") or 1), 1 + d["id"] % 3)
            d["numberOfSeasons"] = n
            d["episodeRunTime"] = [30 + d["id"] % 25]
            d["externalIds"] = {"tvdbId": 300000 + d["id"] % 100000}
            first = int((d.get("firstAirDate") or "2020")[:4])
            d["seasons"] = ([{"seasonNumber": 0, "name": "Specials", "episodeCount": 2, "airDate": None}] if i % 3 == 0
                            else []) + [
                {"seasonNumber": s, "name": "Season %d" % s, "episodeCount": 6 + (d["id"] + s) % 5,
                 "airDate": "%d-03-01" % (first + s - 1)} for s in range(1, n + 1)]
            for s in range(1, n + 1):   # on the media server: what is available (all, or the first seasons)
                st = 5 if info["status"] == 5 or (info["status"] == 4 and s == 1) else 1
                if s in asked and st != 5:
                    st = 2 if info["status"] in (2, 3) else st
                info["seasons"].append({"seasonNumber": s, "status": st, "status4k": 1})
        else:
            d["runtime"] = 85 + d["id"] % 50
        d["mediaInfo"] = info
        save(path, d)
        (series if tv else movies).append(d)
    return movies, series


def result(d, tv):
    """A title as a discover or search page lists it."""
    r = {"id": d["id"], "mediaType": "tv" if tv else "movie", "overview": d.get("overview", ""),
         "posterPath": d.get("posterPath"), "backdropPath": d.get("backdropPath"),
         "genreIds": [g["id"] for g in d.get("genres", [])], "voteAverage": 6.5 + (d["id"] % 30) / 10}
    if tv:
        r.update({"name": d["name"], "originalName": d.get("originalName", d["name"]),
                  "firstAirDate": d.get("firstAirDate")})
    else:
        r.update({"title": d["title"], "originalTitle": d.get("originalTitle", d["title"]),
                  "releaseDate": d.get("releaseDate")})
    info = d.get("mediaInfo") or {}
    if info.get("status", 1) != 1 or info.get("requests"):
        r["mediaInfo"] = {k: info[k] for k in ("id", "tmdbId", "status", "status4k", "jellyfinMediaId")}
    return r


def page(results):
    return {"page": 1, "totalPages": 1, "totalResults": len(results), "results": results}


def year(d, tv):
    return int((d.get("firstAirDate" if tv else "releaseDate") or "0")[:4] or 0)


def write(folder):
    known = statuses(folder)
    movies, series = enrich(folder, known)
    m = [result(d, False) for d in movies]
    t = [result(d, True) for d in series]
    mixed = [x for pair in zip(m, t) for x in pair] + m[len(t):] + t[len(m):]
    save(os.path.join(folder, "discover_trending.json"), page(mixed[:20]))
    save(os.path.join(folder, "discover_movies.json"), page(m[::-1][:20]))
    save(os.path.join(folder, "discover_tv.json"), page(t[::-1][:20]))
    save(os.path.join(folder, "discover_movies_upcoming.json"),
         page([result(d, False) for d in movies if year(d, False) >= 2026][:20]))
    save(os.path.join(folder, "discover_tv_upcoming.json"),
         page([result(d, True) for d in series if year(d, True) >= 2026][:20]))
    # Search: every title, and a person (left out by Régie5); --mock keeps those that match.
    save(os.path.join(folder, "search.json"),
         page(m + t + [{"id": 1, "mediaType": "person", "name": "A Made-up Person", "profilePath": None}]))
    save(os.path.join(folder, "settings_public.json"),
         {"applicationTitle": "Seerr", "mediaServerType": 2, "partialRequestsEnabled": True,
          "enableSpecialEpisodes": False, "localLogin": True, "mediaServerLogin": True})
    profiles = [{"id": 1, "name": "Any"}, {"id": 4, "name": "HD-1080p"}, {"id": 5, "name": "Ultra-HD"},
                {"id": 6, "name": "HD - 720p/1080p"}]
    servers = {
        "radarr": [({"id": 0, "name": "Radarr", "is4k": False, "isDefault": True, "activeProfileId": 4,
                     "activeDirectory": "/data/films"}, ["/data/films", "/data/films-kids"]),
                   ({"id": 1, "name": "Radarr 4K", "is4k": True, "isDefault": True, "activeProfileId": 5,
                     "activeDirectory": "/data/films-4k"}, ["/data/films-4k"])],
        "sonarr": [({"id": 0, "name": "Sonarr", "is4k": False, "isDefault": True, "activeProfileId": 6,
                     "activeDirectory": "/data/series"}, ["/data/series", "/data/anime"])],
    }
    for arr, items in servers.items():
        save(os.path.join(folder, "service_%s.json" % arr), [s for s, _ in items])
        for s, folders in items:
            save(os.path.join(folder, "service_%s_%d.json" % (arr, s["id"])),
                 {"server": s, "profiles": profiles,
                  "rootFolders": [{"id": n + 1, "path": p, "freeSpace": 4_200_000_000_000 - n * 900_000_000_000}
                                  for n, p in enumerate(folders)]})


for base in ("fixtures", "demo"):
    folder = os.path.join(ROOT, base, "seerr")
    if os.path.isdir(folder):
        write(folder)
        print("->", os.path.relpath(folder, ROOT))
