#!/usr/bin/env python3
# Regie5 — builds demo/, the data of `regie5 --demo` (README screenshots).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-demo.py [--pictures DIR]
#
# demo/ is fixtures/ (made-up titles, see anonymize_titles.py) where the titles
# seen first become the Blender Foundation's open films and the series
# Caminandes, all under Creative Commons Attribution, with their posters and
# backdrops (demo/images/, credits in demo/CREDITS.md). Everything else keeps
# its made-up title and its generated card. The metadata is in English.
#
# --pictures DIR: the originals as downloaded from Wikimedia Commons (names in
# PICTURES below), resized and saved as JPEG into demo/images/. Without it,
# the pictures already in demo/images/ are kept.
# Nothing here is shipped with the app: demo/ is not in the PS5 zip.

import argparse
import copy
import json
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURES = os.path.join(ROOT, "fixtures")
OUT = os.path.join(ROOT, "demo")

# key: (Commons file, author, licence, what was done)
PICTURES = {
    "bbb-poster": ("Big buck bunny poster big.jpg", "Blender Foundation", "CC BY 3.0", "resized"),
    "bbb-fanart": ("Big.Buck.Bunny.-.Landscape.png", "Blender Foundation", "CC BY 3.0", "resized"),
    "ed-poster": ("ElephantsDreamPoster.jpg", "Blender Foundation", "CC BY 4.0", "resized"),
    "sintel-poster": ("Sintel poster.jpg", "Blender Foundation", "CC BY 3.0", "resized"),
    "sintel-fanart": ("Durian - Sintel-wallpaper-dragon.jpg", "Blender Foundation (Durian team)", "CC BY 3.0", "resized"),
    "tos-poster": ("Tos-poster.png", "Blender Foundation (Project Mango)", "CC BY 3.0", "resized"),
    "tos-fanart": ("Tears of Steel frame 03 1a.jpg", "Blender Foundation (Project Mango)", "CC BY 3.0", "resized"),
    "cosmos-poster": ("CosmosLaundromatPoster.jpg", "Blender Foundation", "CC BY 4.0", "resized"),
    "cosmos-fanart": ("Franck the caterpillar.jpg", "Blender Foundation", "CC BY 4.0", "resized"),
    "spring-poster": ("Spring2019AlphaPosterBlender.jpg", "Blender Foundation", "CC BY 4.0", "resized"),
    "spring-fanart": ("Blender Open Movie - Spring (2019).png", "Blender Foundation (frame), Alexandru.rosu (upload)", "CC0", "resized"),
    "coffee-poster": ("Coffee Run-movie poster.png", "Blender Studio", "CC BY 4.0", "resized"),
    "coffee-fanart": ("Coffee Run - screenshot-Location 08 Family-Run 3-skateboard.png", "Blender Studio", "CC BY 4.0", "resized"),
    "sprite-poster": ("Sprite Fright-movie poster.jpg", "Blender Studio", "CC BY 4.0", "resized"),
    "sprite-fanart": ("Sprite Fright-concept art-group.png", "Blender Studio", "CC BY 4.0", "resized"),
    "charge-poster": ("Charge-movie poster.jpg", "Blender Studio", "CC BY 4.0", "resized"),
    "charge-fanart": ("Charge-concept art-robot 01.png", "Blender Studio", "CC BY 4.0", "resized"),
    "caminandes-1": ("Pablo Vazquez - Caminandes - Episode 1 - Llama Drama - Cover thumbnail.png", "Pablo Vazquez, Blender Foundation", "CC BY 3.0", "resized"),
    "caminandes-2": ("Blender Foundation - Caminandes - Episode 2 - Gran Dillama - Cover thumbnail.png", "Blender Foundation", "CC BY 3.0", "resized"),
    "caminandes-3": ("Blender Foundation - Caminandes - Episode 3 - Llamigos - Cover thumbnail.png", "Blender Foundation", "CC BY 4.0", "resized"),
    "caminandes-poster": ("Blender Foundation - Caminandes - Episode 3 - Llamigos - Cover thumbnail.png", "Blender Foundation", "CC BY 4.0", "cropped to a portrait and resized"),
}

# made-up title in fixtures/ -> the film it becomes (title, year, minutes, overview, poster, backdrop)
FILMS = {
    "Iris and the Paper Winter": ("Sintel", 2010, 15, "A lonely young woman crosses a hostile land to find the baby dragon she once rescued.", "sintel-poster", "sintel-fanart"),
    "The Quiet Tower": ("Spring", 2019, 8, "A shepherd girl and her dog face ancient spirits to keep the cycle of life going.", "spring-poster", "spring-fanart"),
    "Iris's Mirror": ("Big Buck Bunny", 2008, 10, "A gentle giant rabbit has had enough of three bullying rodents, and plans a sweet revenge.", "bbb-poster", "bbb-fanart"),
    "Léa and the Secret Station": ("Elephants Dream", 2006, 11, "Two men wander through an endless machine that only one of them believes in.", "ed-poster", None),
    "Léa's Tower": ("Tears of Steel", 2012, 12, "In a future Amsterdam, a group of scientists stages a last stand against the robots.", "tos-poster", "tos-fanart"),
    "Ocean of Ravensby": ("Cosmos Laundromat", 2015, 12, "A suicidal sheep is offered a strange deal: a new life, again and again.", "cosmos-poster", "cosmos-fanart"),
    "Léa and the Endless Tower": ("Coffee Run", 2020, 3, "A young woman runs for her coffee, and through the memories of a whole life.", "coffee-poster", "coffee-fanart"),
    "Iris and the Quiet Frontier": ("Charge", 2022, 4, "In a frozen world, an old man breaks into a factory guarded by a robot to steal energy.", "charge-poster", "charge-fanart"),
    "Camille's Satellite": ("Sprite Fright", 2021, 10, "A group of teenagers on a forest trip upsets the wrong mushroom people.", "sprite-poster", "sprite-fanart"),
    "Winter of Stonebridge": ("Glass Half", 2015, 3, "Two art critics in a gallery cannot agree on anything.", None, None),
    "The Glass Machine": ("Hero", 2018, 4, "A young hero fights her way through a world that keeps changing around her.", None, None),
}
SERIES = "Kingdom of Calder"   # the made-up series that becomes Caminandes
SESSIONS = ("Sintel", "Tears of Steel", "Cosmos Laundromat")   # what the three sessions play
# made-up series kept for the cleanup view (watched and ended)
KEEP_SERIES = ("Iris's Meridian", "Jonah and the Second Kingdom", "Wild Kingdom", "Valemont: Quiet Signal")
CAMINANDES = {
    "title": "Caminandes", "year": 2013,
    "overview": "Koro the llama keeps finding new ways to get to the other side of the fence, in Patagonia.",
    "episodes": [("Llama Drama", "2013-09-20", "caminandes-1"), ("Gran Dillama", "2013-11-22", "caminandes-2"),
                 ("Llamigos", "2016-04-15", "caminandes-3")],
}
WHO = ["Mara", "Theo", "Ines", "Jonah", "Lea", "Oskar", "Nadia", "Elias", "Maud", "Rafael", "Sacha", "Yuki"]
DO = ["must find", "discovers", "tries to protect", "goes looking for", "comes face to face with", "inherits"]
WHAT = ["a family secret", "a forgotten map", "an old friend", "a sunken town", "a signal from far away",
        "a ten-year-old promise", "the last night train", "a letter never sent"]
WHEN = ["before winter", "in a single night", "against all odds", "at the end of the world", "without telling anyone"]

TITLE_KEYS = ("title", "originalTitle", "SeriesName", "OriginalTitle", "originalName", "seriesTitle")


def english_overview(seed):
    n = sum(ord(c) * (i + 1) for i, c in enumerate(seed))
    return "%s %s %s %s." % (WHO[n % len(WHO)], DO[(n // 7) % len(DO)], WHAT[(n // 11) % len(WHAT)],
                             WHEN[(n // 13) % len(WHEN)])


def sort_title(t):
    return re.sub(r"^(the|a|an) ", "", t.lower())


class Demo:
    def __init__(self, files):
        self.files = files
        self.images = {}
        self.renamed = {}   # made-up -> film title

    def picture(self, key, name):
        if name:
            self.images[key] = name + ".jpg"

    def film_of(self, o):
        for k in TITLE_KEYS + ("Name", "name"):
            v = o.get(k)
            if isinstance(v, str) and v in FILMS:
                return FILMS[v]
        return None

    def walk(self, o, svc, parent=None):
        if isinstance(o, list):
            for x in o:
                self.walk(x, svc, parent)
            return
        if not isinstance(o, dict):
            return
        film = self.film_of(o) if not (svc == "jellyfin" and o.get("Type") == "Episode") else None
        if film:
            title, year, minutes, overview, poster, fanart = film
            for k in TITLE_KEYS + ("Name", "name"):
                if isinstance(o.get(k), str) and o[k] in FILMS:
                    o[k] = title
            for k, v in (("sortTitle", sort_title(title)), ("cleanTitle", re.sub(r"[^a-z0-9]", "", title.lower())),
                         ("year", year), ("ProductionYear", year), ("runtime", minutes),
                         ("overview", overview), ("Overview", overview), ("studio", "Blender Studio")):
                if k in o:
                    o[k] = v
            if "RunTimeTicks" in o:
                o["RunTimeTicks"] = minutes * 60 * 10_000_000
            for k in ("releaseDate", "PremiereDate"):
                if isinstance(o.get(k), str) and re.match(r"\d{4}", o[k]):
                    o[k] = str(year) + o[k][4:]
            if svc == "radarr" and isinstance(o.get("id"), int) and ("tmdbId" in o or "hasFile" in o):
                self.picture("radarr/mediacover/%d/poster" % o["id"], poster)
                self.picture("radarr/mediacover/%d/fanart" % o["id"], fanart)
            if svc == "jellyfin" and isinstance(o.get("Id"), str):
                self.picture("jellyfin/items/%s/primary" % o["Id"], poster)
                for t in ("backdrop", "thumb"):
                    self.picture("jellyfin/items/%s/%s" % (o["Id"], t), fanart)
            if svc == "seerr":
                if o.get("posterPath"):
                    self.picture("seerr/" + o["posterPath"].lstrip("/"), poster)
                if o.get("backdropPath"):
                    self.picture("seerr/" + o["backdropPath"].lstrip("/"), fanart)
        elif any(isinstance(o.get(k), str) for k in ("overview", "Overview")):
            seed = str(o.get("title") or o.get("Name") or o.get("name") or o.get("id") or "")
            for k in ("overview", "Overview"):
                if isinstance(o.get(k), str) and o[k]:
                    o[k] = english_overview(seed)
        for k, v in list(o.items()):
            if isinstance(v, str) and k in ("SeasonName", "name") and v.startswith("Saison "):
                o[k] = "Season " + v[7:]
            if isinstance(v, str) and k == "name" and v == "Épisodes spéciaux":
                o[k] = "Specials"
            self.walk(v, svc, k)

    def caminandes(self):
        c = CAMINANDES
        files = self.files
        series = [s for s in files["sonarr/series.json"] if s["title"] == SERIES]
        if len(series) != 1:
            sys.exit("make-demo: the series %r is not in fixtures/sonarr/series.json" % SERIES)
        s = series[0]
        sid = s["id"]
        size = 3 * 400_000_000
        s.update(title=c["title"], sortTitle="caminandes", year=c["year"], status="ended", ended=True,
                 network="Blender Studio", overview=c["overview"], runtime=3, previousAiring="2016-04-15T12:00:00Z")
        s["statistics"].update(seasonCount=1, episodeFileCount=3, episodeCount=3, totalEpisodeCount=3,
                               sizeOnDisk=size, percentOfEpisodes=100)
        s["seasons"] = [
            {"seasonNumber": 1, "monitored": True,
             "statistics": {"episodeFileCount": 3, "episodeCount": 3, "totalEpisodeCount": 3, "sizeOnDisk": size,
                            "percentOfEpisodes": 100, "previousAiring": "2016-04-15T12:00:00Z"}}]
        self.picture("sonarr/mediacover/%d/poster" % sid, "caminandes-poster")
        self.picture("sonarr/mediacover/%d/fanart" % sid, "caminandes-1")
        name = "sonarr/episodefile_%d.json" % sid
        if name in files and files[name]:
            proto = files[name][0]
            files[name] = []
            for i in range(3):
                f = copy.deepcopy(proto)
                f.update(id=proto["id"] + i, seriesId=sid, seasonNumber=1, size=size // 3)
                files[name].append(f)
        # Jellyfin: the series, and three of its new episodes in the recent row
        for rel, d in files.items():
            if not rel.startswith("jellyfin/"):
                continue
            for it in (d.get("Items", []) if isinstance(d, dict) else []):
                if it.get("Type") == "Series" and it.get("Name") == SERIES:
                    it["Name"] = c["title"]
                    if "ProductionYear" in it:
                        it["ProductionYear"] = c["year"]
                    self.picture("jellyfin/items/%s/primary" % it["Id"], "caminandes-poster")
                    self.picture("jellyfin/items/%s/backdrop" % it["Id"], "caminandes-1")
        items = files["jellyfin/items.json"]["Items"]
        eps = [it for it in items if it.get("Type") == "Episode" and it.get("SeriesName") == SERIES]
        keep = eps[:3]
        items[:] = [it for it in items if it not in eps or it in keep]
        for n, (it, (title, aired, pic)) in enumerate(zip(keep, c["episodes"])):
            it.update(Name=title, SeriesName=c["title"], IndexNumber=n + 1, ParentIndexNumber=1,
                      SeasonName="Season 1", ProductionYear=int(aired[:4]),
                      PremiereDate=aired + "T00:00:00.0000000Z", RunTimeTicks=3 * 60 * 10_000_000)
            self.picture("jellyfin/items/%s/primary" % it["Id"], pic)
            series_id = it.get("SeriesId")
            if series_id:
                self.picture("jellyfin/items/%s/primary" % series_id, "caminandes-poster")
                self.picture("jellyfin/items/%s/backdrop" % series_id, "caminandes-1")
                self.picture("jellyfin/items/%s/thumb" % series_id, "caminandes-1")
        # everywhere else: the series' made-up name
        for rel, d in files.items():
            self.rename(d, SERIES, c["title"])

    def rename(self, o, old, new):
        if isinstance(o, dict):
            for k, v in o.items():
                if isinstance(v, str) and v == old and k in TITLE_KEYS + ("Name", "name"):
                    o[k] = new
                else:
                    self.rename(v, old, new)
        elif isinstance(o, list):
            for v in o:
                self.rename(v, old, new)


def make_pictures(src, dst):
    from PIL import Image
    os.makedirs(dst, exist_ok=True)
    for key, (commons, _, _, done) in PICTURES.items():
        path = os.path.join(src, (key if key != "caminandes-poster" else "caminandes-3") + ".bin")
        im = Image.open(path).convert("RGB")
        if key == "caminandes-poster":   # a portrait from the 16:9 cover: Koro, left of the title
            w, h = im.size
            cw, x = h * 2 // 3, w * 3 // 64
            im = im.crop((x, 0, x + cw, h))
        width = 500 if ("poster" in key) else 960
        if im.width > width:
            im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        im.save(os.path.join(dst, key + ".jpg"), "JPEG", quality=84, optimize=True, progressive=True)


def credits():
    lines = ["# Demo pictures", "",
             "The pictures in `demo/images/` are used only by `regie5 --demo` (the README",
             "screenshots); they are not part of the app. They come from Wikimedia Commons, from the",
             "Blender Foundation's open films, under the licences below.", "",
             "| File | Work | Author | Licence | Source | Changes |", "|---|---|---|---|---|---|"]
    works = {"bbb": "Big Buck Bunny (2008)", "ed": "Elephants Dream (2006)", "sintel": "Sintel (2010)",
             "tos": "Tears of Steel (2012)", "cosmos": "Cosmos Laundromat (2015)", "spring": "Spring (2019)",
             "coffee": "Coffee Run (2020)", "sprite": "Sprite Fright (2021)", "charge": "Charge (2022)",
             "caminandes": "Caminandes (2013-2016)"}
    lic = {"CC BY 3.0": "https://creativecommons.org/licenses/by/3.0/",
           "CC BY 4.0": "https://creativecommons.org/licenses/by/4.0/",
           "CC0": "https://creativecommons.org/publicdomain/zero/1.0/"}
    for key, (commons, author, licence, done) in PICTURES.items():
        url = "https://commons.wikimedia.org/wiki/File:" + commons.replace(" ", "_")
        lines.append("| `%s.jpg` | %s | %s | [%s](%s) | [%s](%s) | %s, JPEG |" % (
            key, works[key.split("-")[0]], author, licence, lic[licence], commons, url.replace("(", "%28").replace(")", "%29"), done))
    lines += ["", "Titles, years and one-line overviews of these films are written for the demo. Every",
              "other title in `demo/` is made up, as in `fixtures/`.", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pictures", help="the originals downloaded from Wikimedia Commons (<key>.bin)")
    args = ap.parse_args()
    files = {}
    for base, _, names in os.walk(FIXTURES):
        for n in sorted(names):
            p = os.path.join(base, n)
            with open(p, encoding="utf-8") as f:
                files[os.path.relpath(p, FIXTURES)] = json.load(f)
    missing = [t for t in FILMS if not any(t == m.get("title") for m in files["radarr/movie.json"])]
    if missing:
        sys.exit("make-demo: not in fixtures/radarr/movie.json: " + ", ".join(missing))
    demo = Demo(files)
    ratios = [s["PlayState"]["PositionTicks"] / s["NowPlayingItem"]["RunTimeTicks"]
              for s in files["jellyfin/sessions.json"]]
    for rel, d in files.items():
        demo.walk(d, rel.split(os.sep)[0])
    demo.caminandes()
    # Three people watching three films, each at the same point as in fixtures/.
    index = {it["Name"]: it for it in files["jellyfin/items_index.json"]["Items"] if it.get("Type") == "Movie"}
    for s, ratio, title in zip(files["jellyfin/sessions.json"], ratios, SESSIONS):
        np = s["NowPlayingItem"]
        if title != np["Name"]:
            film = next(f for f in FILMS.values() if f[0] == title)
            np.update(Name=title, OriginalTitle=title, Id=index[title]["Id"], ProductionYear=film[1], Overview=film[3],
                      RunTimeTicks=film[2] * 60 * 10_000_000)
            for k in ("ItemId", "MediaSourceId"):
                if k in s.get("PlayState", {}):
                    s["PlayState"][k] = index[title]["Id"]
        s["PlayState"]["PositionTicks"] = int(np["RunTimeTicks"] * ratio)
    for g in files.get("glances/quicklook.json", {}), :
        if "cpu_name" in g:
            g["cpu_name"] = "Intel(R) Core(TM) i5 CPU"
    # A small library, so the grids show the films first: these, and what the
    # calendars, queues and the cleanup refer to.
    films = {f[0] for f in FILMS.values()}
    keep = {m["id"] for m in files["radarr/calendar.json"]} | \
        {q.get("movieId") for q in files["radarr/queue.json"]["records"]}
    files["radarr/movie.json"] = [m for m in files["radarr/movie.json"] if m["title"] in films or m["id"] in keep]
    keep = {e["seriesId"] for e in files["sonarr/calendar.json"]} | \
        {q.get("seriesId") for q in files["sonarr/queue.json"]["records"]}
    files["sonarr/series.json"] = [s for s in files["sonarr/series.json"]
                                   if s["id"] in keep or s["title"] in KEEP_SERIES + (CAMINANDES["title"],)]

    images = os.path.join(OUT, "images")
    for svc in ("sonarr", "radarr", "seerr", "jellyfin", "glances"):
        if os.path.isdir(os.path.join(OUT, svc)):
            shutil.rmtree(os.path.join(OUT, svc))
    for rel, d in files.items():
        p = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False, indent=1)
            f.write("\n")
    if args.pictures:
        make_pictures(args.pictures, images)
    have = set(os.listdir(images)) if os.path.isdir(images) else set()
    lost = sorted(set(demo.images.values()) - have)
    if lost:
        sys.exit("make-demo: pictures missing in demo/images/ (use --pictures): " + ", ".join(lost))
    with open(os.path.join(OUT, "images.json"), "w", encoding="utf-8") as f:
        json.dump(dict(sorted(demo.images.items())), f, indent=1)
        f.write("\n")
    with open(os.path.join(OUT, "CREDITS.md"), "w", encoding="utf-8") as f:
        f.write(credits())
    print("demo/: %d files, %d pictures mapped" % (len(files), len(demo.images)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
