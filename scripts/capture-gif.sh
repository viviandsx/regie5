#!/usr/bin/env bash
# Regie5 — the README's short demonstration (docs/images/demo.gif), from the
# demo data (--demo: the Blender Foundation's open films, see demo/CREDITS.md).
# No server is contacted, no action is carried out.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/capture-gif.sh                   -> docs/images/demo.gif (600 px wide, ~3 MB)
#   OUT=/tmp/gif scripts/capture-gif.sh      elsewhere
#
# The tour: Home (along the upcoming row, then the recently added one), the
# Library and a series' page, Activity, Requests, Jellyfin and a playback
# diagnosis. One frame every 1/12 s at 1280x720, then 10 frames a second in
# the GIF. Needs ffmpeg.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/linux"
OUT="${OUT:-${ROOT}/docs/images}"
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed" >&2; exit 2; }
if [[ ! -f "${BUILD}/build.ninja" ]]; then
    "${ROOT}/scripts/run-linux.sh" --version >/dev/null
fi
cmake --build "${BUILD}" >/dev/null
mkdir -p "${OUT}"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP:?}"' EXIT

script="$(python3 - "${TMP}" <<'PY'
import sys
out, fps, n, steps = sys.argv[1], 12, 0, ["4"]   # the demo data loads first, no frames
def hold(secs):
    global n
    for _ in range(round(secs * fps)):
        steps.extend([f"shot:{out}/f{n:04d}.ppm", f"{1 / fps:.4f}"])
        n += 1
def press(button, secs):
    steps.append(button)
    hold(secs)
hold(1.0)
for _ in range(3):
    press("right", 0.45)
press("down", 0.9)
press("right", 0.45)
press("right", 0.6)
press("up", 0.3)
press("r1", 1.3)
press("down", 0.8)
press("cross", 1.8)
press("circle", 0.5)
press("r1", 1.3)
press("r1", 1.3)
press("r1", 1.0)
press("cross", 1.8)
press("circle", 0.6)
steps.append("quit")
print(" ".join(steps))
PY
)"
REGIE5_MOCK_DELAY_MS=0 REGIE5_RES=1280x720 REGIE5_DISPLAY=1920x1080@60/0 REGIE5_SCRIPT="${script}" \
    "${BUILD}/regie5" --demo --lang en >/dev/null 2>&1
ffmpeg -nostdin -loglevel error -y -framerate 12 -i "${TMP}/f%04d.ppm" \
    -vf "fps=10,scale=600:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle" \
    "${TMP}/demo.gif"
mv "${TMP}/demo.gif" "${OUT}/demo.gif"
echo "-> ${OUT}/demo.gif"
