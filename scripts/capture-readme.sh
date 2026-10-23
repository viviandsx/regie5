#!/usr/bin/env bash
# Regie5 — the README screenshots, in English, from the demo data (--demo:
# the Blender Foundation's open films, see demo/CREDITS.md). No server is
# contacted, no action is carried out.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/capture-readme.sh                  -> docs/images/*.jpg (1920x1080)
#   OUT=/tmp/shots scripts/capture-readme.sh   elsewhere
#
# Needs ImageMagick (convert).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/linux"
OUT="${OUT:-${ROOT}/docs/images}"
command -v convert >/dev/null || { echo "ImageMagick (convert) is needed" >&2; exit 2; }
if [[ ! -f "${BUILD}/build.ninja" ]]; then
    "${ROOT}/scripts/run-linux.sh" --version >/dev/null
fi
cmake --build "${BUILD}" >/dev/null
mkdir -p "${OUT}"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP:?}"' EXIT

# Home (focus on the recently added row), the Library (a series' page, the
# films, the guide, the cleanup), the queue, the requests, the issues and Discover,
# Jellyfin, a playback diagnosis, the disks and a disk's sheet, the libraries,
# Statistics, Settings; then the welcome screens.
s="4 down 2.5 shot:${TMP}/home.ppm up "
s+="r1 2.5 down cross 2 shot:${TMP}/series-detail.ppm circle 0.6 up "
s+="right cross 2.5 shot:${TMP}/movies.ppm "
s+="right cross 1.5 down 2 shot:${TMP}/guide.ppm up "
s+="right cross 1 right cross 3 right cross 1 right cross 3 shot:${TMP}/cleanup.ppm "
s+="r1 3 shot:${TMP}/queue.ppm "
s+="r1 2.5 shot:${TMP}/requests.ppm right cross 2.5 shot:${TMP}/issues.ppm "
s+="right cross 2.5 down 0.5 down 0.5 right right right right 2.5 shot:${TMP}/discover.ppm "
s+="r1 2.5 shot:${TMP}/jellyfin.ppm cross 1.5 shot:${TMP}/diagnosis.ppm circle 0.6 "
# The disks (demo/scrutiny: sdb failing) and sdb's sheet, the libraries,
# then up to the pills and Statistics over 12 months (the focus came in on
# the first playback: 3 playbacks, the scan, 4 disks, then the films and the series).
s+="down down down down down 2 shot:${TMP}/disks.ppm cross 1.5 shot:${TMP}/disk.ppm circle 0.6 "
s+="down down down down 2 shot:${TMP}/libraries.ppm "
s+="up up up up up up up up up up 1 right cross 1 right right right cross 3 shot:${TMP}/statistics.ppm "
s+="r1 3 shot:${TMP}/settings.ppm quit"

# Settings' display line as for the screenshots' size (else this monitor's).
REGIE5_MOCK_DELAY_MS=0 REGIE5_MOCK_CLEANUP=1 REGIE5_RES=1920x1080 REGIE5_DISPLAY=1920x1080@60/0 REGIE5_SCRIPT="${s}" \
    "${BUILD}/regie5" --demo --lang en >/dev/null 2>&1
# The welcome screens (REGIE5_WELCOME=1): the media server's choice.
REGIE5_MOCK_DELAY_MS=0 REGIE5_WELCOME=1 REGIE5_RES=1920x1080 REGIE5_SCRIPT="3 cross 2 shot:${TMP}/welcome.ppm quit" \
    "${BUILD}/regie5" --demo --lang en >/dev/null 2>&1
for f in "${TMP}"/*.ppm; do
    n="$(basename "${f}" .ppm)"
    convert "${f}" -strip -quality 84 "${OUT}/${n}.jpg"
done
echo "-> ${OUT}"
