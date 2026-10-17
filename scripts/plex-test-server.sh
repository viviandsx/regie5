#!/usr/bin/env bash
# Regie5 — a throwaway Plex Media Server on this machine, to develop and test
# Plex support without anybody's real server, and without a Plex account.
#   scripts/plex-test-server.sh start    make the demo library (once), start Plex
#   scripts/plex-test-server.sh setup    libraries, a Régie5 config (once;
#                                        scripts/plex-test-setup.py)
#   scripts/plex-test-server.sh stop     stop the container (its data is kept)
#   scripts/plex-test-server.sh status
#
# Plex listens on 127.0.0.1:32400 only (REGIE5_PLEX_ADDR=<this machine's LAN
# address> to reach it from the console too). The server is never signed in
# to a Plex account (no PLEX_CLAIM): it answers without a token to the
# addresses in ALLOWED_NETWORKS, Docker's own networks, which is how this
# machine reaches it. Everything it keeps is under build/plex-test/ (never
# versioned): config/ (Plex's own), transcode/, media/ (short clips drawn by
# ffmpeg, named after the Blender Foundation's free films, with posters and
# backdrops at a real server's size) and config.json (a Régie5 config with
# Plex alone:  scripts/run-linux.sh --config build/plex-test/config.json).
# Plex's agents fetch the films' metadata from Plex's servers (no account).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="${ROOT}/build/plex-test"
IMAGE="${REGIE5_PLEX_IMAGE:-plexinc/pms-docker:1.43.4.10903-e5521bd8c}"
NAME=regie5-plex-test
PORT=32400
ADDR="${REGIE5_PLEX_ADDR:-127.0.0.1}"

clip() {   # clip <file> <seconds> <size> <video codec> <colour source>
    local out="$1" secs="$2" size="$3" vcodec="$4" src="$5"
    [[ -e "${out}" ]] && return 0
    mkdir -p "$(dirname "${out}")"
    ffmpeg -nostdin -loglevel error -n \
        -f lavfi -i "${src}=size=${size}:rate=24:duration=${secs}" \
        -f lavfi -i "sine=frequency=440:duration=${secs}" \
        -c:v "${vcodec}" -pix_fmt yuv420p -x265-params log-level=error -c:a aac -ac 2 -shortest "${out}"
}

picture() {   # picture <file> <size> <colour> <text>: a poster or a backdrop, drawn once
    [[ -e "$1" ]] && return 0
    ffmpeg -nostdin -loglevel error -n -f lavfi -i "color=c=$3:size=$2" -frames:v 1 \
        -vf "drawtext=fontfile=${ROOT}/assets/fonts/Inter-Bold.ttf:text='$4':fontcolor=white:fontsize=h/12:x=(w-tw)/2:y=(h-th)/2" \
        -q:v 3 "$1"
}

# Posters 2000×3000 and backdrops 3840×2160, as on a real server: what shows
# whether Plex sends pictures at the size asked for (Emby did not).
movie() {   # movie <title> <year> <size> <codec> <source> <colour>
    local d="${DIR}/media/movies/$1 ($2)"
    clip "${d}/$1 ($2).mkv" 20 "$3" "$4" "$5"
    picture "${d}/poster.jpg" 2000x3000 "$6" "$1"
    picture "${d}/fanart.jpg" 3840x2160 "$6" "$1"
}

make_media() {
    command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to draw the clips" >&2; exit 1; }
    movie "Big Buck Bunny" 2008 1280x720 libx264 testsrc2 0x3a7d44
    movie "Elephants Dream" 2006 1280x720 libx264 smptebars 0x7d4a3a
    movie "Sintel" 2010 1920x1080 libx264 testsrc 0x3a5a7d
    # HEVC 4K: what a client that cannot play it makes Plex transcode.
    movie "Tears of Steel" 2012 3840x2160 libx265 testsrc2 0x5a3a7d
    local s="${DIR}/media/shows/Caminandes (2013)"
    mkdir -p "${s}/Season 01"
    picture "${s}/poster.jpg" 2000x3000 0x7d6a3a "Caminandes"
    picture "${s}/fanart.jpg" 3840x2160 0x7d6a3a "Caminandes"
    for e in 1 2 3; do
        clip "${s}/Season 01/Caminandes - S01E0${e}.mkv" 15 1280x720 libx264 testsrc2
    done
}

case "${1:-}" in
start)
    make_media
    mkdir -p "${DIR}/config" "${DIR}/transcode"
    if docker ps -a --format '{{.Names}}' | grep -qx "${NAME}"; then
        docker start "${NAME}" >/dev/null
    else
        docker run -d --name "${NAME}" -p "${ADDR}:${PORT}:32400/tcp" \
            -e TZ="$(cat /etc/timezone 2>/dev/null || echo UTC)" \
            -e PLEX_UID="$(id -u)" -e PLEX_GID="$(id -g)" \
            -e ALLOWED_NETWORKS=172.16.0.0/12 \
            -v "${DIR}/config:/config" -v "${DIR}/transcode:/transcode" \
            -v "${DIR}/media:/data:ro" "${IMAGE}" >/dev/null
    fi
    echo "Plex: http://${ADDR}:${PORT} (container ${NAME}, data in build/plex-test/)"
    [[ "$(docker port "${NAME}" 32400/tcp)" == "${ADDR}:${PORT}" ]] ||
        echo "note: the container was made for $(docker port "${NAME}" 32400/tcp); remove it (docker rm ${NAME}) to change the address" >&2
    ;;
setup) exec python3 "${ROOT}/scripts/plex-test-setup.py" "http://127.0.0.1:${PORT}" "${DIR}" ;;
stop) docker stop "${NAME}" >/dev/null && echo "stopped (data kept in build/plex-test/)" ;;
status) docker ps -a --filter "name=^${NAME}$" --format '{{.Names}}: {{.Status}} ({{.Image}})' ;;
*) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
