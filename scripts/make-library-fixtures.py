#!/usr/bin/env python3
# Regie5 — writes the Jellyfin libraries of --mock into fixtures/jellyfin/.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-library-fixtures.py
#
# Made up from the fixtures themselves (no server is read): three libraries,
# Films and Series holding the films and series of items_index.json (already
# made-up titles and ids), and a home-videos one counted only. Shaped as
# Jellyfin 10.9+ answers: GET /Library/VirtualFolders, and GET /Items with a
# ParentId (the titles, or Limit=0 for a count). Also gives the users of
# users.json distinct names (the anonymizer had given some the same one).
# Run scripts/make-demo.py afterwards to carry them into demo/.

import hashlib
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JF = os.path.join(ROOT, "fixtures", "jellyfin")
SONARR = os.path.join(ROOT, "fixtures", "sonarr")

SPARE_NAMES = ["Inès", "Jules", "Karim", "Léon", "Maya"]


def fake_id(seed):
    return hashlib.sha256(("regie5-library-" + seed).encode()).hexdigest()[:32]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write(name, data):
    with open(os.path.join(JF, name), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


def folder(name, kind, locations):
    return {"Name": name, "Locations": locations, "CollectionType": kind,
            "LibraryOptions": {"Enabled": True, "EnableRealtimeMonitor": True},
            "ItemId": fake_id(name), "PrimaryImageItemId": fake_id(name), "RefreshStatus": "Idle",
            "_regie5_synthetic": True}


def main():
    index = load(os.path.join(JF, "items_index.json"))["Items"]
    films = [i for i in index if i["Type"] == "Movie"]
    series = [i for i in index if i["Type"] == "Series"]
    episodes = sum(s.get("statistics", {}).get("episodeFileCount", 0) for s in load(os.path.join(SONARR, "series.json")))
    libs = [folder("Films", "movies", ["/media/movies"]),
            folder("Series", "tvshows", ["/media/tv"]),
            folder("Concerts", "homevideos", ["/media/concerts"])]
    write("library_virtualfolders.json", libs)
    write("items_parent_%s.json" % libs[0]["ItemId"], {"Items": films, "TotalRecordCount": len(films)})
    write("items_parent_%s.json" % libs[1]["ItemId"], {"Items": series, "TotalRecordCount": len(series)})
    write("items_parent_%s_episode.json" % libs[1]["ItemId"], {"Items": [], "TotalRecordCount": episodes})
    write("items_parent_%s_files.json" % libs[2]["ItemId"], {"Items": [], "TotalRecordCount": 37})

    users = load(os.path.join(JF, "users.json"))
    seen, spare = set(), iter(SPARE_NAMES)
    for u in users:
        if u["Name"] in seen:
            u["Name"] = next(spare)
        seen.add(u["Name"])
    write("users.json", users)
    print("fixtures/jellyfin: %d libraries (%d films, %d series, %d episodes), %d users"
          % (len(libs), len(films), len(series), episodes, len(users)))


if __name__ == "__main__":
    main()
