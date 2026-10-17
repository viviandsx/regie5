#!/usr/bin/env python3
# Regie5 — fixtures/plex/ from the throwaway Plex of scripts/plex-test-server.sh
# (start, then setup). Only that server is ever asked: 127.0.0.1:32400, and it
# is never signed in to a Plex account (no token is sent, none is kept).
#
#   scripts/make-plex-fixtures.py [--out DIR]     (default fixtures/plex)
#
# Every GET Régie5 sends to Plex is asked and kept under the name the mock
# transport looks for (src/api/mock_transport.cpp, plex_file_for). For the
# sessions, two playbacks run while the answers are taken
# (scripts/plex-test-play.sh: a 4K HEVC film transcoded, an episode played
# directly; they write play states and history into the test server only).
#
# Kept out: every address (Player.address, remotePublicAddress), the server's
# and the players' identifiers, the owner's features, files' paths
# (Part.file), a history entry's device, the films' texts and people (Plex's
# metadata: summaries, casts with pictures on plex.tv), and every setting but
# the transcoder's throttle. Made up, and marked "synthetic" in the files'
# list below: a second account ("Lea", who watched two things) and the
# sessions' accounts (a server with no Plex account names none).
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = "http://127.0.0.1:32400"
HEADERS = {"Accept": "application/json", "X-Plex-Client-Identifier": "regie5-fixtures", "X-Plex-Product": "Regie5"}
MACHINE = "0123456789abcdef0123456789abcdef01234567"   # made up
DROP = {"address", "remotePublicAddress", "file", "deviceID", "ownerFeatures", "myPlexUsername", "summary",
        "tagline", "Role", "Director", "Writer", "Producer", "Country", "Genre", "Rating", "Similar", "Image",
        "librarySectionUUID", "diagnostics", "countryCode", "transcoderVideoBitrates", "transcoderVideoQualities",
        "transcoderVideoResolutions", "Directory", "audienceRatingImage", "studio", "slug", "platformVersion"}
KEEP_DIRECTORY = {"library_sections_all.json"}   # the libraries are Directory elements there


def get(path):
    req = urllib.request.Request(BASE + path, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode())


def clean(o, name, ids):
    """Drops what must not be kept; replaces identifiers by made-up ones."""
    if isinstance(o, list):
        return [clean(x, name, ids) for x in o]
    if not isinstance(o, dict):
        return o
    out = {}
    for k, v in o.items():
        if k in DROP and not (k == "Directory" and name in KEEP_DIRECTORY):
            continue
        if k == "machineIdentifier":
            v = MACHINE if isinstance(v, str) and len(v) == 40 else ids.setdefault(v, f"player-{len(ids) + 1}")
        elif k == "friendlyName":
            v = "Plex"
        elif k == "uuid":
            v = "00000000-0000-0000-0000-000000000001"
        elif k == "path" and isinstance(v, str) and v.startswith("/"):   # a library folder: its last name only
            v = "/media/" + v.rstrip("/").rsplit("/", 1)[-1]
        elif k == "id" and isinstance(v, str) and v.startswith("regie5-"):   # a test session's id
            v = ids.setdefault(v, f"session-{len(ids) + 1}")
        elif k == "key" and isinstance(v, str) and "regie5-" in v:
            v = "/transcode/sessions/session-1"
        out[k] = clean(v, name, ids)
    return out


def play(mode, key, seconds):
    return subprocess.Popen([os.path.join(ROOT, "scripts", "plex-test-play.sh"), mode, str(key), str(seconds)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def find(items, **want):
    for m in items:
        if all(m.get(k) == v for k, v in want.items()):
            return m
    raise SystemExit(f"not on the test server: {want} (scripts/plex-test-server.sh setup)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "fixtures", "plex"))
    out_dir = ap.parse_args().out
    if get("/identity")["MediaContainer"].get("claimed"):
        raise SystemExit("this Plex is signed in to an account: not the test server")
    sections = get("/library/sections/all")["MediaContainer"]["Directory"]
    movies = find(sections, type="movie")["key"]
    shows = find(sections, type="show")["key"]
    films = get(f"/library/sections/{movies}/all")["MediaContainer"]["Metadata"]
    episodes = get(f"/library/sections/{shows}/all?type=4")["MediaContainer"]["Metadata"]
    film4k = find(films, title="Tears of Steel")["ratingKey"]
    episode = episodes[0]["ratingKey"]

    players = [play("transcode", film4k, 40), play("direct", episode, 40)]
    for _ in range(30):
        s = get("/status/sessions")["MediaContainer"]
        if s.get("size", 0) >= 2 and any("TranscodeSession" in m for m in s.get("Metadata", [])):
            break
        time.sleep(1)
    time.sleep(4)   # the transcode some way ahead
    page = "X-Plex-Container-Start=0&X-Plex-Container-Size="
    asks = {
        "identity.json": "/identity",
        "root.json": "/",
        "prefs.json": "/:/prefs",
        "status_sessions.json": "/status/sessions",
        "library_sections_all.json": "/library/sections/all",
        f"library_sections_{movies}_all.json": f"/library/sections/{movies}/all?includeGuids=1",
        f"library_sections_{shows}_all.json": f"/library/sections/{shows}/all?includeGuids=1",
        f"library_sections_{movies}_all_recent.json": f"/library/sections/{movies}/all?type=1&sort=addedAt:desc&{page}20",
        f"library_sections_{shows}_all_recent.json": f"/library/sections/{shows}/all?type=4&sort=addedAt:desc&{page}60",
        f"library_sections_{movies}_all_count.json": f"/library/sections/{movies}/all?{page}0",
        f"library_sections_{shows}_all_count.json": f"/library/sections/{shows}/all?{page}0",
        f"library_sections_{shows}_all_count_episode.json": f"/library/sections/{shows}/all?type=4&{page}0",
        "butler.json": "/butler",
        "activities.json": "/activities",
        "accounts.json": "/accounts",
        "accounts_1.json": "/accounts/1",
        "status_sessions_history_all.json": f"/status/sessions/history/all?sort=viewedAt:desc&{page}300",
    }
    answers = {name: get(path) for name, path in asks.items()}
    sessions = answers["status_sessions.json"]["MediaContainer"].get("Metadata", [])
    for m in sessions:
        answers[f"library_metadata_{m['ratingKey']}.json"] = get(f"/library/metadata/{m['ratingKey']}")
    for p in players:
        p.wait()
    history = answers["status_sessions_history_all.json"]["MediaContainer"].get("Metadata", [])
    keys = sorted({h["ratingKey"] for h in history} | {films[0]["ratingKey"], episodes[-1]["ratingKey"]})
    answers["library_metadata_ids.json"] = get("/library/metadata/" + ",".join(keys))

    # Synthetic: the settings kept, a second account and her history, the sessions' accounts.
    prefs = answers["prefs.json"]["MediaContainer"]
    prefs["Setting"] = [s for s in prefs.get("Setting", []) if s.get("id") == "TranscoderThrottleBuffer"]
    prefs["size"] = len(prefs["Setting"])
    acc = answers["accounts.json"]["MediaContainer"]
    acc["Account"].append({"id": 2, "key": "/accounts/2", "name": "Lea", "thumb": ""})
    acc["size"] = len(acc["Account"])
    answers["accounts_2.json"] = {"MediaContainer": {"size": 1, "Account": [acc["Account"][-1]]}}
    now = int(time.time())
    for i, key in enumerate([films[0]["ratingKey"], episodes[-1]["ratingKey"]]):
        history.append({"historyKey": f"/status/sessions/history/{900 + i}", "key": f"/library/metadata/{key}",
                        "ratingKey": key, "title": "synthetic", "type": "movie" if i == 0 else "episode",
                        "viewedAt": now - (i + 1) * 86400, "accountID": 2})
    history.sort(key=lambda h: -h["viewedAt"])
    answers["status_sessions_history_all.json"]["MediaContainer"].update(size=len(history), totalSize=len(history))
    for m, who in zip(sessions, [("1", "Administrator"), ("2", "Lea")]):
        m["User"] = {"id": who[0], "title": who[1]}

    os.makedirs(out_dir, exist_ok=True)
    ids = {}
    for name, body in sorted(answers.items()):
        with open(os.path.join(out_dir, name), "w", encoding="utf-8") as f:
            json.dump(clean(body, name, ids), f, indent=2, ensure_ascii=False)
            f.write("\n")
    print(f"{len(answers)} files in {out_dir}")


main()
