#!/usr/bin/env python3
# Regie5 — sets up the throwaway Plex of scripts/plex-test-server.sh (run by
# its "setup"): two libraries (Films, Séries) with Plex's own agents, and a
# Régie5 config.json with Plex alone (no token: the test server lets Docker's
# networks in without one). Run again, it only adds what is missing. Talks to
# 127.0.0.1 only; nothing here involves a Plex account.
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE, DIR = sys.argv[1], sys.argv[2]
HEADERS = {
    "Accept": "application/json",
    "X-Plex-Product": "Regie5 test setup",
    "X-Plex-Client-Identifier": "regie5-plex-test-setup",
}

LIBRARIES = [
    # name, type, agent, scanner, folder in the container
    ("Films", "movie", "tv.plex.agents.movie", "Plex Movie", "/data/movies"),
    ("Séries", "show", "tv.plex.agents.series", "Plex TV Series", "/data/shows"),
]


def call(method, path, query=None):
    url = BASE + path + ("?" + urllib.parse.urlencode(query, doseq=True) if query else "")
    req = urllib.request.Request(url, data=b"" if method in ("POST", "PUT") else None, method=method)
    for k, v in HEADERS.items():
        req.add_header(k, v)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            text = r.read().decode()
            return json.loads(text) if text.strip().startswith("{") else text
    except urllib.error.HTTPError as e:
        raise SystemExit(f"{method} {path} -> HTTP {e.code}: {e.read()[:200]!r}")


def wait_up():
    for _ in range(90):
        try:
            ident = call("GET", "/identity")["MediaContainer"]
            if "startState" not in ident:
                return ident
        except (urllib.error.URLError, ConnectionError, SystemExit, KeyError):
            pass
        time.sleep(2)
    raise SystemExit(f"Plex does not answer at {BASE}: scripts/plex-test-server.sh start")


def main():
    ident = wait_up()
    print(f"Plex {ident['version']}, claimed: {ident['claimed']}")
    have = {d["title"] for d in call("GET", "/library/sections/all")["MediaContainer"].get("Directory", [])}
    for name, kind, agent, scanner, folder in LIBRARIES:
        if name in have:
            continue
        call("POST", "/library/sections", {
            "name": name, "type": kind, "agent": agent, "scanner": scanner,
            "language": "en-US", "location": folder,
        })
        print(f"library {name} added")
    conf = os.path.join(DIR, "config.json")
    if not os.path.exists(conf):
        with open(conf, "w", encoding="utf-8") as f:
            json.dump({"plex": {"url": BASE}}, f, indent=2)
            f.write("\n")
        print(f"Régie5 config: {conf}")


main()
