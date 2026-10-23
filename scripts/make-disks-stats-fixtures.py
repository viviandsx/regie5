#!/usr/bin/env python3
# Regie5 — writes the fixtures of the disks (Scrutiny) and of the viewing
# statistics (Jellyfin's Playback Reporting plugin), for --mock and the tests.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 -I scripts/make-disks-stats-fixtures.py           # fixtures/
#   python3 -I scripts/make-disks-stats-fixtures.py --demo    # demo/ (its users and films)
#   python3 -I scripts/make-disks-stats-fixtures.py --out DIR # another copy (make-demo.py --out)
#
# make-fixtures.py and make-demo.py run it at their end (they write
# jellyfin/ afresh, without these files).
#
# Entirely made up (no server is read). The disks have no serial number, a
# made-up model name, WWNs and UUIDs of a counter; their readings are dated
# as of 2026-10-23, which --mock moves to today (api/mock_transport, dated).
# Shaped as Scrutiny 0.9.5 answers (webapp/backend/pkg/models), plus the
# summary of a 0.8 (keyed by WWN) for the tests; Playback Reporting 19's as
# its Api/PlaybackReportingActivityController.cs and Data/ActivityRepository.cs
# make them. The users are the fixtures' (jellyfin/users.json), the titles
# Radarr's and Sonarr's, so posters are found by name.

import argparse
import datetime
import json
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANCHOR = datetime.date(2026, 10, 23)


def write(base, rel, data):
    path = os.path.join(base, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


def iso(day, hour=0, minute=0, second=0):
    return "%sT%02d:%02d:%02dZ" % (day.isoformat(), hour, minute, second)


# ---- Scrutiny ----

# device, label, protocol, rotation, capacity, status, temp, hours, cycles,
# days since the last reading, temperatures of the week, failing attributes
DISKS = [
    ("sda", "", "ATA", 0, 512110190592, 0, 36, 8213, 412, 0, [35, 36, 35, 37, 36, 36, 36], {}),
    ("sdb", "", "ATA", 7200, 8001563222016, 1, 41, 39875, 96, 0, [40, 41, 41, 42, 41, 40, 41],
     {"5": (1, 24, "now"), "197": (4, 8, ""), "198": (2, 2, "")}),
    ("sdc", "Sauvegardes", "ATA", 5400, 8001563222016, 0, 57, 21450, 133, 0, [52, 54, 55, 56, 55, 57, 57], {}),
    ("nvme0", "", "NVMe", 0, 1000204886016, 0, 48, 6120, 280, 4, [46, 47, 48, 47, 48, 48, 48], {}),
]

ATA_OK = [1, 3, 4, 5, 7, 9, 10, 12, 187, 188, 190, 192, 193, 194, 197, 198, 199]
NVME_OK = ["critical_warning", "temperature", "available_spare", "percentage_used", "media_errors",
           "num_err_log_entries", "power_on_hours", "power_cycles", "unsafe_shutdowns"]


def device(i, d, uuid):
    name, label, protocol, rotation, capacity, status = d[:6]
    return {
        "CreatedAt": "2026-06-01T08:00:00Z", "UpdatedAt": iso(ANCHOR - datetime.timedelta(days=d[9] if len(d) > 9 else 30)),
        "DeletedAt": None, "archived": False,
        "wwn": "0x500000000000000%d" % (i + 1), "device_name": name, "device_uuid": "", "device_serial_id": "",
        "device_label": "", "manufacturer": "", "model_name": "Disque de démonstration",
        "interface_type": "", "interface_speed": "", "serial_number": "", "firmware": "",
        "rotational_speed": rotation, "capacity": capacity,
        "form_factor": "" if protocol == "NVMe" else ("2.5 inches" if rotation == 0 else "3.5 inches"),
        "smart_support": True, "device_protocol": protocol, "device_type": "nvme" if protocol == "NVMe" else "sat",
        "label": label, "host_id": "", "device_status": status, **({"scrutiny_uuid": uuid} if uuid else {}),
    }


def attrs(d):
    protocol, temps, failing = d[2], d[10], d[11]
    out = {}
    if protocol == "NVMe":
        for k in NVME_OK:
            v = {"temperature": d[6], "available_spare": 100, "power_on_hours": d[7], "power_cycles": d[8]}.get(k, 0)
            out[k] = {"attribute_id": k, "value": v, "thresh": 10 if k == "available_spare" else -1,
                      "transformed_value": 0, "status": 0}
        return out
    for n in ATA_OK:
        st, raw, when = failing.get(str(n), (0, 0, ""))
        raw = {9: d[7], 12: d[8], 194: d[6], 190: d[6]}.get(n, raw)
        a = {"attribute_id": n, "value": 100, "thresh": 10 if n in (5, 197, 198) else 0, "worst": 100,
             "raw_value": raw, "raw_string": str(raw), "when_failed": when, "transformed_value": 0, "status": st}
        if st:
            a["value"] = 5 if st & 1 else 100
            a["status_reason"] = "made up for Regie5's fixtures"
            a["failure_rate"] = 0.2
        out[str(n)] = a
    return out


def scrutiny(base):
    summary, old = {}, {}
    for i, d in enumerate(DISKS):
        uuid = "00000000-0000-4000-8000-%012d" % (i + 1)
        last = ANCHOR - datetime.timedelta(days=d[9])
        history = [{"date": iso(last - datetime.timedelta(days=6 - k), 1), "temp": t} for k, t in enumerate(d[10])]
        smart = {"collector_date": iso(last, 0, 0, 6), "temp": d[6], "power_on_hours": d[7]}
        summary[uuid] = {"device": device(i, d, uuid), "smart": smart, "temp_history": history}
        old["0x500000000000000%d" % (i + 1)] = {"device": device(i, d, None), "smart": smart,
                                                 "temp_history": history}
        results = []
        for k, t in enumerate(d[10]):
            day = last - datetime.timedelta(days=6 - k)
            results.append({"date": iso(day), "device_wwn": "", "scrutiny_uuid": uuid,
                            "device_protocol": d[2], "temp": t, "power_on_hours": d[7] - 24 * (6 - k),
                            "power_cycle_count": d[8], "attrs": attrs(d), "Status": d[5]})
        write(base, "scrutiny/device_%s_details.json" % uuid,
              {"success": True, "data": {"device": device(i, d, uuid), "smart_results": results}, "metadata": {}})
    # An archived disk: never shown.
    archived = device(4, ("sdz", "", "ATA", 7200, 2000398934016, 0), "00000000-0000-4000-8000-000000000099")
    archived["archived"] = True
    summary["00000000-0000-4000-8000-000000000099"] = {"device": archived}
    write(base, "scrutiny/summary.json", {"data": {"summary": summary}, "success": True})
    if base == os.path.join(ROOT, "fixtures"):   # for the tests
        write(base, "scrutiny/summary_0.8.json", {"data": {"summary": old}, "success": True})
    write(base, "scrutiny/health.json", {"success": True})
    write(base, "scrutiny/settings.json", {"settings": {
        "theme": "system", "layout": "material", "dashboard_display": "name", "dashboard_sort": "status",
        "temperature_unit": "celsius", "file_size_si_units": True, "line_stroke": "smooth",
        "powered_on_hours_unit": "humanize", "collector": {"discard_sct_temp_history": False},
        "metrics": {"notify_level": 2, "status_filter_attributes": 0, "status_threshold": 3,
                    "repeat_notifications": True}}, "success": True})


# ---- Playback Reporting ----

PLUGINS = [
    ("TMDb", "12.2.0.0", "b8715ed16c4745289ad3f72deb539cd4"),
    ("Playback Reporting", "19.0.0.0", "5c53438191a343cb907a35aa02eb9d2c"),
    ("Open Subtitles", "25.0.0.0", "4b9ed42f5925486b8d2c1b7b6f4cd9d5"),
]


def plays(users, movies, series, rng):
    """A year of plays: (day, user, kind, title, seconds)."""
    out = []
    taste = {u: (rng.sample(movies, min(12, len(movies))), rng.sample(series, min(5, len(series)))) for u in users}
    weight = {u: rng.choice([0.2, 0.5, 0.8, 1.2, 1.6]) for u in users}
    for back in range(365):
        day = ANCHOR - datetime.timedelta(days=back)
        weekend = day.weekday() >= 4
        for u in users:
            chance = weight[u] * (0.55 if weekend else 0.3) * (1.25 if day.month in (12, 1, 2) else 1.0)
            while rng.random() < chance:
                fav_movies, fav_series = taste[u]
                if rng.random() < 0.35:
                    title = rng.choice(fav_movies[:4]) if rng.random() < 0.6 else rng.choice(movies)
                    out.append((day, u, "Movie", title, rng.randint(80, 140) * 60))
                else:
                    title = rng.choice(fav_series[:2]) if rng.random() < 0.7 else rng.choice(series)
                    out.append((day, u, "Episode", title, rng.randint(20, 55) * 60))
                chance *= 0.45
    return out


def stats(base, users, movies, series):
    rng = random.Random(5)
    log = plays([u["Name"] for u in users], movies, series, rng)
    ids = {u["Name"]: u["Id"] for u in users}
    write(base, "jellyfin/plugins.json", [
        {"Name": n, "Version": v, "ConfigurationFileName": "", "Description": "", "Id": i, "CanUninstall": True,
         "HasImage": False, "Status": "Active"} for n, v, i in PLUGINS])
    for days in (7, 30, 365):
        first = ANCHOR - datetime.timedelta(days=days - 1)
        within = [p for p in log if p[0] >= first]
        per_user = {}
        for day, u, kind, title, s in within:
            e = per_user.setdefault(u, {"count": 0, "time": 0, "last": day})
            e["count"] += 1
            e["time"] += s
            e["last"] = max(e["last"], day)
        activity = [{"latest_date": iso(e["last"], 21, 12) .replace("Z", ""), "user_id": ids[u],
                     "total_count": e["count"], "total_time": e["time"], "item_name": "", "client_name": "",
                     "user_name": u, "has_image": False, "last_seen": "", "total_play_time": ""}
                    for u, e in sorted(per_user.items(), key=lambda kv: kv[1]["last"], reverse=True)]
        write(base, "jellyfin/user_usage_stats_user_activity_%d.json" % days, activity)
        dates = [(first + datetime.timedelta(days=k)).isoformat() for k in range(days)]
        rows = []
        for u in sorted(per_user, key=str.lower):
            usage = {d: 0 for d in dates}
            for day, who, kind, title, s in within:
                if who == u:
                    usage[day.isoformat()] += s
            rows.append({"user_id": ids[u], "user_name": u, "user_usage": usage})
        rows.append({"user_id": "labels_user", "user_name": "labels_user", "user_usage": {d: 0 for d in dates}})
        write(base, "jellyfin/user_usage_stats_playactivity_%d.json" % days, rows)
        for kind, rel in (("Movie", "moviesreport"), ("Episode", "gettvshowsreport")):
            agg = {}
            for day, who, k, title, s in within:
                if k == kind:
                    e = agg.setdefault(title, {"label": title, "count": 0, "time": 0})
                    e["count"] += 1
                    e["time"] += s
            write(base, "jellyfin/user_usage_stats_%s_%d.json" % (rel, days),
                  sorted(agg.values(), key=lambda e: e["label"].lower()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--demo", action="store_true", help="demo/ in place of fixtures/")
    ap.add_argument("--out", help="another folder shaped as fixtures/ (its users and titles are read)")
    args = ap.parse_args()
    base = os.path.abspath(args.out) if args.out else os.path.join(ROOT, "demo" if args.demo else "fixtures")
    with open(os.path.join(base, "jellyfin", "users.json"), encoding="utf-8") as f:
        users = [u for u in json.load(f) if not u.get("Policy", {}).get("IsDisabled")]
    with open(os.path.join(base, "radarr", "movie.json"), encoding="utf-8") as f:
        movies = [m["title"] for m in json.load(f) if m.get("hasFile")]
    with open(os.path.join(base, "sonarr", "series.json"), encoding="utf-8") as f:
        series = [s["title"] for s in json.load(f)]
    scrutiny(base)
    stats(base, users, movies[:60], series[:20])


if __name__ == "__main__":
    main()
