#!/usr/bin/env python3
# Regie5 — checks a title's param.json against the profiles tried on a console.
# SPDX-License-Identifier: GPL-3.0-or-later
"""
    scripts/check-param.py <param.json> [--release]

The fields that decide where the tile goes, whether the title starts, 120 Hz
and whether the app may listen (Settings > From your phone) are compared with
ps5/app/param-profiles.json. 0.3.0 shipped a profile that starts but cannot
listen; this check is there so that it does not happen again.

Without --release (test builds): a warning, the build goes on.
With --release: refused (exit 1) unless the profile is known to start and to
listen. A new profile is added to param-profiles.json after a console test.
"""
import json
import os
import sys

FIELDS = ("applicationCategoryType", "contentBadgeType", "attribute", "attribute2", "attribute3",
          "amm", "kernel", "gameIntent")


def profile_of(param):
    return {k: param.get(k) for k in FIELDS}


def main(argv):
    if len(argv) not in (2, 3) or (len(argv) == 3 and argv[2] != "--release"):
        sys.exit(__doc__.strip())
    release = len(argv) == 3
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(argv[1], encoding="utf-8") as f:
        mine = profile_of(json.load(f))
    with open(os.path.join(root, "ps5", "app", "param-profiles.json"), encoding="utf-8") as f:
        known = json.load(f)["profiles"]
    match = next((p for p in known if p["fields"] == mine), None)
    what = "release refused" if release else "warning"
    if match is None:
        print(f"param.json: {what}: this profile was never tried on a console "
              f"(category {mine['applicationCategoryType']}, attribute {mine['attribute']:#x}, "
              f"attribute3 {mine['attribute3']:#x}); check the tile, the start and the phone page, "
              "then add it to ps5/app/param-profiles.json", file=sys.stderr)
        return 1 if release else 0
    c = match["console"]
    hz = "120 Hz" if c.get("hz120") else "60 Hz only"
    if not c.get("starts"):
        print(f"param.json: {what}: profile '{match['name']}' does not start ({c.get('note', '')})", file=sys.stderr)
        return 1 if release else 0
    if not c.get("listens"):
        print(f"param.json: {what}: profile '{match['name']}' starts but CANNOT LISTEN: "
              f"Settings > From your phone fails ({c.get('note', '')})", file=sys.stderr)
        return 1 if release else 0
    print(f"param.json: profile '{match['name']}' ({c['area']}, starts, listens, {hz}; {c['seen']})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
