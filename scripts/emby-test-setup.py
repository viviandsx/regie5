#!/usr/bin/env python3
# Regie5 — sets up the throwaway Emby of scripts/emby-test-server.sh (run by
# its "setup"): the first-run wizard with a test admin, an API key for Régie5,
# two libraries (Films, Séries), a second user who has watched a few things,
# and a Régie5 config.json with Emby alone. Run again, it only fills what is
# missing. Talks to 127.0.0.1 only; the passwords are made here and kept in
# build/emby-test/credentials.json (never versioned, never printed).
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import secrets
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE, DIR = sys.argv[1], sys.argv[2]
CREDS = os.path.join(DIR, "credentials.json")
AUTH = 'Emby Client="Regie5 test setup", Device="laptop", DeviceId="regie5-emby-test-setup", Version="1"'


def call(method, path, body=None, token=None, ok=(200, 204)):
    data = json.dumps(body).encode() if body is not None else (b"" if method == "POST" else None)
    req = urllib.request.Request(BASE + path, data=data, method=method)
    req.add_header("X-Emby-Authorization", AUTH)
    if body is not None:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("X-Emby-Token", token)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            text = r.read().decode()
            return json.loads(text) if text.strip().startswith(("{", "[")) else text
    except urllib.error.HTTPError as e:
        if e.code in ok:
            return None
        raise SystemExit(f"{method} {path.split('?')[0]} -> HTTP {e.code}: {e.read()[:200]!r}")


def wait_up():
    for _ in range(60):
        try:
            return call("GET", "/System/Info/Public")
        except (urllib.error.URLError, ConnectionError, SystemExit):
            time.sleep(2)
    raise SystemExit(f"Emby does not answer at {BASE}: scripts/emby-test-server.sh start")


def load_creds():
    if os.path.exists(CREDS):
        with open(CREDS, encoding="utf-8") as f:
            return json.load(f)
    return {}


def save_creds(c):
    fd = os.open(CREDS, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(c, f, indent=2)


def login(name, password):
    r = call("POST", "/Users/AuthenticateByName", {"Username": name, "Pw": password})
    return r["AccessToken"], r["User"]["Id"]


def main():
    info = wait_up()
    print(f"Emby {info['Version']} at {BASE}")
    c = load_creds()
    if "admin" not in c:
        c["admin"] = {"name": "regie5-admin", "password": secrets.token_urlsafe(18)}
        c["viewer"] = {"name": "Lea", "password": secrets.token_urlsafe(18)}
        save_creds(c)
    # The first-run wizard (answers 401 once it is done).
    try:
        call("POST", "/Startup/Configuration", {"UICulture": "en-US", "MetadataCountryCode": "FR",
                                                "PreferredMetadataLanguage": "en"})
        call("POST", "/Startup/User", {"Name": c["admin"]["name"], "Password": c["admin"]["password"]})
        call("POST", "/Startup/Complete")
        print("wizard: done")
    except SystemExit as e:
        print(f"wizard: already done ({e})")
    token, admin_id = login(c["admin"]["name"], c["admin"]["password"])

    # Régie5's API key.
    keys = call("GET", "/Auth/Keys", token=token) or {}
    mine = [k for k in keys.get("Items", []) if k.get("AppName") == "Regie5"]
    if not mine:
        call("POST", "/Auth/Keys?App=Regie5", token=token)
        mine = [k for k in call("GET", "/Auth/Keys", token=token)["Items"] if k.get("AppName") == "Regie5"]
    c["apiKey"] = mine[0]["AccessToken"]
    save_creds(c)
    print("API key: in build/emby-test/credentials.json")

    # The libraries, from the clips' .nfo files only (nothing from the internet).
    have = {f["Name"] for f in call("GET", "/Library/VirtualFolders", token=token)}
    for name, kind, path in (("Films", "movies", "/media/movies"), ("Séries", "tvshows", "/media/shows")):
        if name in have:
            continue
        q = urllib.parse.urlencode({"name": name, "collectionType": kind, "refreshLibrary": "true"})
        call("POST", "/Library/VirtualFolders?" + q, {"LibraryOptions": {
            "PathInfos": [{"Path": path}], "EnableInternetProviders": False, "DownloadImagesInAdvance": False,
            "SaveLocalMetadata": False, "EnableRealtimeMonitor": False}}, token=token)
        print(f"library: {name} ({path})")
    call("POST", "/Library/Refresh", token=token)   # files added since (posters...)
    for _ in range(60):   # the first scan
        items = call("GET", "/Items?Recursive=true&IncludeItemTypes=Movie,Episode", token=token)
        if items.get("TotalRecordCount", 0) >= 7:
            break
        time.sleep(2)
    print(f"items: {items.get('TotalRecordCount', 0)} films and episodes")

    # A second user, who has seen two films (one long ago) and likes one.
    users = {u["Name"]: u["Id"] for u in call("GET", "/Users", token=token)}
    if c["viewer"]["name"] not in users:
        u = call("POST", "/Users/New", {"Name": c["viewer"]["name"]}, token=token)
        call("POST", f"/Users/{u['Id']}/Password", {"NewPw": c["viewer"]["password"]}, token=token)
        users[c["viewer"]["name"]] = u["Id"]
        print(f"user: {c['viewer']['name']}")
    vid = users[c["viewer"]["name"]]
    films = {i["Name"]: i["Id"] for i in call(
        "GET", f"/Users/{vid}/Items?Recursive=true&IncludeItemTypes=Movie", token=token)["Items"]}
    if "Sintel" in films:
        call("POST", f"/Users/{vid}/PlayedItems/{films['Sintel']}?DatePlayed=20250301120000", token=token)
        call("POST", f"/Users/{admin_id}/PlayedItems/{films['Sintel']}?DatePlayed=20250302120000", token=token)
    if "Big Buck Bunny" in films:
        call("POST", f"/Users/{vid}/PlayedItems/{films['Big Buck Bunny']}", token=token)
        call("POST", f"/Users/{vid}/FavoriteItems/{films['Big Buck Bunny']}", token=token)
    login(c["viewer"]["name"], c["viewer"]["password"])   # an activity log entry, and a last activity

    # Régie5's config.json: Emby alone.
    cfg = {"emby": {"url": BASE, "apiKey": c["apiKey"]}}
    fd = os.open(os.path.join(DIR, "config.json"), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
    print("Régie5: scripts/run-linux.sh --config build/emby-test/config.json")


if __name__ == "__main__":
    main()
