#!/usr/bin/env python3
# Regie5 — replaces the titles of a library by made-up ones in fixtures/.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/anonymize_titles.py [DIR]      (default: fixtures/)
#
# The fixtures are real answers: user names, devices and addresses are already
# replaced by make-fixtures.py, and this pass does the same for what would show
# whose library it is:
#   - every title (films, series, episodes, original titles, collections) gets
#     a made-up one, the same string always the same made-up title, so the
#     services still agree with one another;
#   - TMDB, TVDB and IMDb ids get made-up ones (same id, same made-up id
#     everywhere: Jellyfin, Radarr/Sonarr and Seerr still match), and the
#     Seerr detail files are renamed after them;
#   - overviews, studios, networks, keywords, websites, trailers, TMDB picture
#     paths, external links and blur hashes are replaced or dropped.
# Run on real answers only (make-fixtures.py calls it on what it fetched): the
# same original always gives the same made-up title and id (hashes, no
# randomness), so files rebuilt one group at a time still agree.

import hashlib
import json
import os
import re
import sys
import unicodedata

SALT = "regie5-titles"

ADJ = ["Silent", "Golden", "Hidden", "Broken", "Northern", "Last", "Crimson", "Distant", "Paper", "Iron",
       "Hollow", "Bright", "Frozen", "Velvet", "Wild", "Lost", "Quiet", "Electric", "Burning", "Glass",
       "Sleeping", "Restless", "Endless", "Pale", "Secret", "Little", "Copper", "Midnight", "Salt", "Silver",
       "Falling", "Wandering", "Borrowed", "Second", "Lonely", "Painted", "Sunken", "Open", "Shifting", "Amber"]
NOUN = ["Harbor", "Orchard", "Lantern", "Signal", "Meridian", "Tide", "Garden", "Archive", "Compass", "Island",
        "Engine", "Valley", "Mirror", "Canyon", "Station", "Ledger", "Comet", "Forest", "Bridge", "Ember",
        "Atlas", "Circuit", "Horizon", "Kingdom", "Lighthouse", "Machine", "Ocean", "Pilot", "Quarry", "River",
        "Summit", "Tower", "Voyage", "Winter", "Harvest", "Echo", "Frontier", "Citadel", "Paradox", "Satellite"]
PLACE = ["Avalon", "Brightwater", "Calder", "Dunmore", "Elmstead", "Fairhaven", "Glenrock", "Hollis", "Ironvale",
         "Juniper", "Kestrel", "Larkspur", "Marlow", "Northgate", "Oakridge", "Pemberly", "Quillon", "Ravensby",
         "Stonebridge", "Thornfield", "Umber", "Valemont", "Westmere", "Yarrow", "Zephyr Point"]
WHO = ["Mara", "Theo", "Inès", "Jonah", "Léa", "Oskar", "Nadia", "Elias", "Maud", "Rafael", "Sacha", "Yuki",
       "Camille", "Ezra", "Lou", "Iris"]
STUDIO = ["Northlight Pictures", "Blue Orchard Films", "Harbor Lane Studios", "Paper Moon Pictures",
          "Quiet Fox Media", "Lantern House", "Ninefold Films", "Cinder & Co."]
NETWORK = ["Channel Nine North", "Orbit+", "Lumen TV", "Studio Ouest", "Streamline", "Atlas One"]
DO = ["doit retrouver", "découvre", "tente de protéger", "cherche", "se retrouve face à",
      "accepte de garder", "hérite de", "essaie d'oublier"]
WHAT = ["un secret de famille", "une carte oubliée", "un ancien ami", "une ville engloutie", "un signal venu de loin",
        "une promesse faite il y a dix ans", "le dernier train de la nuit", "une lettre jamais envoyée"]
WHEN = ["avant l'hiver", "en une seule nuit", "malgré tout", "au bout du monde", "sans rien dire à personne",
        "avant qu'il ne soit trop tard"]

# Keys of a title, by service; derived keys follow the made-up title of their object.
TITLE_KEYS = {"title", "originalTitle", "originalName", "SeriesName", "OriginalTitle", "seriesTitle", "SortName"}
JELLYFIN_ITEMS = {"Movie", "Series", "Episode", "Season", "Video", "BoxSet", "Trailer"}
DROP = {"keywords", "website", "youTubeTrailerId", "ExternalUrls", "ImageBlurHashes", "Taglines", "homepage",
        "alternateTitles", "secondaryYearSourceId", "imdb", "tmdb", "Tags", "People", "RemoteTrailers"}
ID_KEYS = {"tmdbId": "tmdb", "tvdbId": "tvdb", "imdbId": "imdb", "Tmdb": "tmdb", "Tvdb": "tvdb", "Imdb": "imdb",
           "TmdbCollection": "tmdb", "tvRageId": "tvrage", "tvMazeId": "tvmaze"}


def h(*parts):
    return int(hashlib.sha1((SALT + "\x1f".join(str(p) for p in parts)).encode()).hexdigest(), 16)


def slug(t):
    t = unicodedata.normalize("NFKD", t).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "-", t).strip("-")


def clean_title(t):
    return re.sub(r"[^a-z0-9]", "", unicodedata.normalize("NFKD", t).encode("ascii", "ignore").decode().lower())


class Titles:
    def __init__(self):
        self.made = {}       # original -> made-up
        self.taken = {}      # made-up -> original
        self.ids = {}        # (kind, original) -> made-up
        self.ids_taken = set()

    def title(self, original):
        if not isinstance(original, str) or not original.strip():
            return original
        if original in self.made:
            return self.made[original]
        n = h("t", original)
        for k in range(50):
            x = n + k * 7919
            a, b, p, w = ADJ[x % len(ADJ)], NOUN[(x // 40) % len(NOUN)], PLACE[(x // 1600) % len(PLACE)], \
                WHO[(x // 40000) % len(WHO)]
            form = (x // 640000) % 6
            t = ["The %s %s" % (a, b), "%s %s" % (a, b), "%s of %s" % (b, p), "%s and the %s %s" % (w, a, b),
                 "%s: %s %s" % (p, a, b), "%s's %s" % (w, b)][form]
            if k:
                t += " " + ["II", "III", "IV", "V", "VI", "VII", "VIII", "IX"][(k - 1) % 8] + ("" if k < 9 else str(k))
            if t not in self.taken:
                break
        self.made[original] = t
        self.taken[t] = original
        return t

    def ident(self, kind, v):
        if v in (None, "", 0):
            return v
        key = (kind, str(v))
        if key not in self.ids:
            n = h("id", kind, v)
            while True:
                if kind == "imdb":
                    new = "tt9%07d" % (n % 10_000_000)
                else:
                    new = 9_000_000 + n % 900_000
                if (kind, new) not in self.ids_taken:
                    break
                n += 1
            self.ids_taken.add((kind, new))
            self.ids[key] = new
        new = self.ids[key]
        if kind == "imdb":
            return new
        return str(new) if isinstance(v, str) else new

    def overview(self, original):
        n = h("o", original)
        return "%s %s %s %s." % (WHO[n % len(WHO)], DO[(n >> 4) % len(DO)], WHAT[(n >> 8) % len(WHAT)],
                                 WHEN[(n >> 12) % len(WHEN)])


def is_made(t, titles):
    return t in titles.taken


def walk(o, titles, svc, parent=None):
    if isinstance(o, list):
        return [walk(x, titles, svc, parent) for x in o]
    if not isinstance(o, dict):
        return o
    out = {}
    jf_item = svc == "jellyfin" and (o.get("Type") in JELLYFIN_ITEMS or parent == "NowPlayingItem")
    for k, v in o.items():
        if k in DROP or (parent == "ProviderIds" and k not in ID_KEYS) or (parent == "externalIds" and k not in ID_KEYS):
            continue
        if k == "id" and svc == "seerr" and parent == "root" and isinstance(v, int):
            out[k] = titles.ident("tmdb", v)   # a Seerr detail: its id is TMDB's
            continue
        if k in ID_KEYS and isinstance(v, (str, int)) and not isinstance(v, bool):
            out[k] = titles.ident(ID_KEYS[k], v)
        elif k in TITLE_KEYS and isinstance(v, str):
            out[k] = titles.title(v)
        elif k == "Name" and isinstance(v, str) and jf_item:
            out[k] = titles.title(v)
        elif k == "name" and isinstance(v, str) and svc == "seerr" and parent in (None, "media", "root"):
            out[k] = titles.title(v)
        elif k in ("overview", "Overview") and isinstance(v, str):
            out[k] = titles.overview(v) if v else v
        elif k in ("studio", "network") and isinstance(v, str):
            pool = STUDIO if k == "studio" else NETWORK
            out[k] = pool[h(k, v) % len(pool)] if v else v
        elif k == "Studios" and isinstance(v, list):
            out[k] = [{"Name": STUDIO[h("studio", s.get("Name", "")) % len(STUDIO)], "Id": s.get("Id")} for s in v]
        elif k in ("posterPath", "backdropPath") and isinstance(v, str) and v:
            out[k] = v if v.startswith("/r5") else "/r5%012x.jpg" % (h(k, v) % (1 << 48))
        elif k == "collection" and isinstance(v, dict):
            out[k] = walk(v, titles, svc, k)
        else:
            out[k] = walk(v, titles, svc, k)
    # derived keys, after the title of this object
    t = out.get("title")
    if isinstance(t, str) and t:
        if "sortTitle" in out:
            out["sortTitle"] = re.sub(r"^(the|a|an) ", "", t.lower())
        if "cleanTitle" in out:
            out["cleanTitle"] = clean_title(t)
        if "titleSlug" in out:
            ts = out["titleSlug"]
            out["titleSlug"] = str(titles.ident("tmdb", ts)) if isinstance(ts, str) and ts.isdigit() else slug(t)
    for k in ("externalServiceSlug",):
        if k in out and isinstance(out[k], str):
            base = out.get("title") or out.get("name") or ""
            out[k] = slug(base) if base else slug(titles.title(out[k]))
    return out


SEERR_FILE = re.compile(r"^(movie|tv)_(\d+)\.json$")


def run(root):
    titles = Titles()
    files = []
    for base, _, names in os.walk(root):
        for n in sorted(names):
            if n.endswith(".json"):
                files.append(os.path.join(base, n))
    data = {}
    for p in sorted(files):
        svc = os.path.relpath(p, root).split(os.sep)[0]
        with open(p, encoding="utf-8") as f:
            data[p] = walk(json.load(f), titles, svc, "root")
    # The last sweep: a title still written inside some longer text (a whole
    # string equal to a title is left: outside a title key it is something else,
    # a user called like a film for one).
    originals = sorted((o for o in titles.made if len(o) >= 4 and re.search(r"[^\W\d_]", o)),
                       key=len, reverse=True)   # "2012" is a film, and a year in every date
    if originals:
        pattern = re.compile(r"(?<!\w)(%s)(?!\w)" % "|".join(re.escape(o) for o in originals))

        def sweep(o):
            if isinstance(o, dict):
                return {k: sweep(v) for k, v in o.items()}
            if isinstance(o, list):
                return [sweep(v) for v in o]
            if isinstance(o, str) and o not in titles.made and o not in titles.taken:
                return pattern.sub(lambda m: titles.made[m.group(1)], o)
            return o
        data = {p: sweep(d) for p, d in data.items()}
    data = {p: json.dumps(d, ensure_ascii=False, indent=1) for p, d in data.items()}
    for p, text in data.items():
        dst = p
        m = SEERR_FILE.match(os.path.basename(p))
        if m and os.path.relpath(p, root).startswith("seerr"):
            dst = os.path.join(os.path.dirname(p), "%s_%s.json" % (m.group(1), titles.ident("tmdb", int(m.group(2)))))
        with open(dst, "w", encoding="utf-8") as f:
            f.write(text + "\n")
        if dst != p:
            os.remove(p)
    return titles


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), "fixtures")
    titles = run(root)
    print("%d titles and %d ids replaced in %s" % (len(titles.made), len(titles.ids), root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
