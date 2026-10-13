#!/usr/bin/env python3
# Regie5 — writes fixtures/seerr/issue_*.json: Seerr issues for --mock.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-issue-fixtures.py
#
# Entirely made up (no server is read): the titles are the fixtures' own
# made-up ones (fixtures/seerr/movie_*.json, tv_*.json), the users the
# fixtures' requesters. Shaped as Seerr 3.x answers (server/routes/issue.ts):
# the list joins the comments but not their users; GET /issue/{id} has them.
# Run scripts/make-demo.py afterwards to carry them into demo/.

import hashlib
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEERR = os.path.join(ROOT, "fixtures", "seerr")

USERS = {name: {"id": i + 2, "displayName": name, "email": "%s@example.invalid" % name.lower().replace("é", "e"),
                "userType": 3, "jellyfinUsername": name}
         for i, name in enumerate(["Alice", "Bruno", "Chloé", "David", "Emma"])}
USERS["Admin"] = {"id": 1, "displayName": "Admin", "email": "admin@example.invalid", "userType": 3,
                  "jellyfinUsername": "Admin"}

# id, mediaType, tmdbId, issueType (1 video, 2 audio, 3 subtitles, 4 other),
# status (1 open, 2 resolved), season, episode, by, modifiedBy, comments (who, when, text)
ISSUES = [
    (41, "tv", 9248046, 3, 1, 1, 3, "Alice", None, [
        ("Alice", "2026-10-12T19:04:00.000Z", "The French subtitles are about two seconds late."),
    ]),
    (40, "movie", 9083184, 1, 1, 0, 0, "Bruno", None, [
        ("Bruno", "2026-10-11T21:40:00.000Z", "The picture freezes around 1:12:00, every time."),
        ("Admin", "2026-10-12T08:15:00.000Z", "I'm looking into it."),
    ]),
    (39, "movie", 9108727, 2, 1, 0, 0, "Chloé", None, [
        ("Chloé", "2026-10-11T18:22:00.000Z", "No sound on the French track; the English one is fine."),
    ]),
    (38, "tv", 9303458, 4, 1, 1, 0, "David", None, [
        ("David", "2026-10-10T20:51:00.000Z", "Episode 4 is missing from season 1."),
    ]),
    (37, "movie", 9068481, 1, 2, 0, 0, "Emma", "Admin", [
        ("Emma", "2026-10-09T17:30:00.000Z", "Green blocks all over the picture in the last scene."),
        ("Admin", "2026-10-10T09:02:00.000Z", "Thanks, it's fixed: please try again."),
        ("Emma", "2026-10-10T20:11:00.000Z", "Works now, thank you!"),
    ]),
    (36, "tv", 9364504, 2, 2, 1, 1, "Alice", "Admin", [
        ("Alice", "2026-10-08T21:05:00.000Z", "The sound and the picture drift apart after ten minutes."),
        ("Admin", "2026-10-09T10:40:00.000Z", "The file has been replaced."),
    ]),
]


def fake_hex(seed):
    return hashlib.sha256(("regie5-issue-" + seed).encode()).hexdigest()[:32]


def issue(spec, with_users):
    iid, kind, tmdb, itype, status, season, episode, by, mod, comments = spec
    media = {"id": 900 + iid, "mediaType": kind, "tmdbId": tmdb, "tvdbId": None, "status": 5, "status4k": 1,
             "jellyfinMediaId": fake_hex("%s-%d" % (kind, tmdb)), "jellyfinMediaId4k": None}
    out = {
        "id": iid, "issueType": itype, "status": status, "problemSeason": season, "problemEpisode": episode,
        "createdAt": comments[0][1], "updatedAt": comments[-1][1],
        "media": media, "createdBy": USERS[by], "modifiedBy": USERS[mod] if mod else None,
        "comments": [],
        "_regie5_synthetic": True,
    }
    for n, (who, when, text) in enumerate(comments):
        c = {"id": iid * 10 + n, "message": text, "createdAt": when, "updatedAt": when}
        if with_users:
            c["user"] = USERS[who]
        out["comments"].append(c)
    return out


def page(specs):
    return {"pageInfo": {"pages": 1, "pageSize": 50, "results": len(specs), "page": 1},
            "results": [issue(s, False) for s in specs]}


def write(name, data):
    with open(os.path.join(SEERR, name), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


def main():
    for kind, tmdb in {(s[1], s[2]) for s in ISSUES}:   # the titles must be the fixtures' own
        assert os.path.exists(os.path.join(SEERR, "%s_%d.json" % (kind, tmdb))), (kind, tmdb)
    by_modified = sorted(ISSUES, key=lambda s: s[9][-1][1], reverse=True)   # sort=modified
    write("issue_open.json", page([s for s in by_modified if s[4] == 1]))
    write("issue_resolved.json", page([s for s in by_modified if s[4] == 2]))
    write("issue_all.json", page(by_modified))
    for s in ISSUES:
        write("issue_%d.json" % s[0], issue(s, True))
    print("fixtures/seerr: %d issues" % len(ISSUES))


if __name__ == "__main__":
    main()
