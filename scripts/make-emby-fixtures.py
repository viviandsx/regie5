#!/usr/bin/env python3
# Regie5 — fixtures/emby/ from the throwaway Emby of scripts/emby-test-server.sh
# (start, then setup). Only that server is ever asked: 127.0.0.1:18096.
#
#   scripts/make-emby-fixtures.py [--out DIR]     (default fixtures/emby)
#
# Every GET Régie5 sends to its media server is asked with the API key, and
# kept under the name the mock transport looks for (src/api/mock_transport.cpp).
# For a session to show, the test viewer plays the 4K HEVC clip as a player
# would, transcoded to H.264, while the answers are taken (it writes play
# states into the test server only). Kept out, as in fixtures/jellyfin/: the
# activity log's text (Name, ShortOverview, Overview: addresses, devices),
# the sessions' addresses and device ids, the files' paths, the server's
# addresses.
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = "http://127.0.0.1:18096"
CREDS = os.path.join(ROOT, "build", "emby-test", "credentials.json")
PLAYER = 'Emby Client="Test Player", Device="Salon TV", DeviceId="regie5-test-player", Version="1"'
DROP = {"Name", "ShortOverview", "Overview"}            # activity entries
DROP_SESSION = {"RemoteEndPoint", "DeviceId", "Path"}   # anywhere in the sessions


def request(method, path, key=None, player_token=None, body=None, raw=False):
    data = json.dumps(body).encode() if body is not None else (b"" if method == "POST" else None)
    r = urllib.request.Request(BASE + path, data=data, method=method)
    r.add_header("Content-Type", "application/json")
    if key:
        r.add_header("X-Emby-Token", key)
    else:
        r.add_header("X-Emby-Authorization", PLAYER + (f', Token="{player_token}"' if player_token else ""))
    resp = urllib.request.urlopen(r, timeout=60)
    if raw:
        return resp
    text = resp.read().decode()
    return json.loads(text) if text.strip() and text.strip()[0] in "{[" else text


def strip(v, names):
    if isinstance(v, dict):
        return {k: strip(x, names) for k, x in v.items() if k not in names}
    if isinstance(v, list):
        return [strip(x, names) for x in v]
    return v


def main():
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(ROOT, "fixtures", "emby")
    with open(CREDS, encoding="utf-8") as f:
        c = json.load(f)
    key = c["apiKey"]
    files = {}

    def get(name, path):
        files[name] = request("GET", path, key=key)
        return files[name]

    # The viewer starts a transcoded play of the 4K clip (see emby-test-server.sh).
    a = request("POST", "/Users/AuthenticateByName", body={"Username": c["viewer"]["name"], "Pw": c["viewer"]["password"]})
    tok, uid = a["AccessToken"], a["User"]["Id"]
    films = request("GET", f"/Users/{uid}/Items?Recursive=true&IncludeItemTypes=Movie", player_token=tok)["Items"]
    item = next(i["Id"] for i in films if i["Name"] == "Tears of Steel")
    ps = "regie5-fixtures"
    request("POST", "/Sessions/Playing", player_token=tok,
            body={"ItemId": item, "PlaySessionId": ps, "PlayMethod": "Transcode", "PositionTicks": 0})
    stream = request("GET", f"/Videos/{item}/stream.ts?" + urllib.parse.urlencode({
        "VideoCodec": "h264", "AudioCodec": "aac", "MaxWidth": 1280, "VideoBitrate": 3000000, "AudioBitrate": 128000,
        "PlaySessionId": ps, "DeviceId": "regie5-test-player", "api_key": tok}), raw=True)
    reading = threading.Thread(target=lambda: [time.sleep(0.05) for _ in iter(lambda: stream.read(65536), b"")],
                               daemon=True)
    reading.start()
    time.sleep(6)
    request("POST", "/Sessions/Playing/Progress", player_token=tok,
            body={"ItemId": item, "PlaySessionId": ps, "PlayMethod": "Transcode", "PositionTicks": 60000000})
    time.sleep(1)

    # What Régie5 asks (src/api/jellyfin.cpp), named as the mock transport reads it.
    sessions = get("sessions", "/Sessions?activeWithinSeconds=960")
    files["sessions"] = strip(sessions, DROP_SESSION)
    request("POST", "/Sessions/Playing/Stopped", player_token=tok,
            body={"ItemId": item, "PlaySessionId": ps, "PositionTicks": 70000000})
    files["system_info"] = strip(request("GET", "/System/Info", key=key),
                                 {"LocalAddresses", "RemoteAddresses", "WakeOnLanInfo"})
    get("scheduledtasks", "/ScheduledTasks?isHidden=false")
    get("items", "/Items?SortBy=DateCreated&SortOrder=Descending&Recursive=true&IncludeItemTypes=Movie,Episode"
                 "&IsMissing=false&Limit=40&Fields=DateCreated&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
    get("system_configuration_encoding", "/System/Configuration/encoding")
    users = get("users", "/Users")
    since = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(time.time() - 14 * 86400))
    log = get("system_activitylog_entries", f"/System/ActivityLog/Entries?minDate={since}&hasUserId=true&limit=300")
    files["system_activitylog_entries"] = strip(log, DROP)
    for n in sorted({e["UserId"] for e in log["Items"] if e.get("UserId")}):
        if n not in {u["Id"] for u in users}:
            get(f"users_{n}", f"/Users/{n}")   # Emby names users by its own number there
    ids = sorted({e["ItemId"] for e in log["Items"] if e.get("ItemId")})
    if ids:
        get("items_ids", f"/Items?ids={','.join(ids)}&Fields=ProductionYear")
    folders = get("library_virtualfolders", "/Library/VirtualFolders")
    files["library_virtualfolders"] = strip(folders, {"LibraryOptions"})
    for lib in folders:
        p = lib["ItemId"]
        get(f"items_parent_{p}", f"/Items?ParentId={p}&Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds"
                                 "&EnableUserData=false&EnableImages=false")
        tail = "&Limit=0&EnableTotalRecordCount=true&EnableImages=false&EnableUserData=false"
        get(f"items_parent_{p}_episode", f"/Items?ParentId={p}&Recursive=true&IncludeItemTypes=Episode" + tail)
        get(f"items_parent_{p}_files", f"/Items?ParentId={p}&Recursive=true&IsFolder=false" + tail)
    get("items_index", "/Items?Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds&EnableUserData=false"
                       "&EnableImages=false")
    for u in users:
        get(f"items_user_{u['Id']}", f"/Items?userId={u['Id']}&Recursive=true&IncludeItemTypes=Movie,Series"
                                     "&Fields=ProviderIds,UserDataLastPlayedDate&EnableUserData=true&EnableImages=false")
        get(f"items_user_{u['Id']}_played", f"/Items?userId={u['Id']}&Recursive=true&IncludeItemTypes=Episode"
                                            "&IsPlayed=true&Fields=UserDataLastPlayedDate&EnableUserData=true"
                                            "&EnableImages=false")

    os.makedirs(out, exist_ok=True)
    for name, body in sorted(files.items()):
        with open(os.path.join(out, name + ".json"), "w", encoding="utf-8") as f:
            json.dump(body, f, indent=1, ensure_ascii=False)
            f.write("\n")
    print(f"{len(files)} files in {os.path.relpath(out, ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except urllib.error.URLError as e:
        sys.exit(f"the test Emby does not answer at {BASE} ({e}): scripts/emby-test-server.sh start, then setup")
