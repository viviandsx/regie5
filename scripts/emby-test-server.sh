#!/usr/bin/env bash
# Regie5 — a throwaway Emby server on this machine, to develop and test Emby
# support without anybody's real server.
#   scripts/emby-test-server.sh start    make the demo library (once), start Emby
#   scripts/emby-test-server.sh setup    first-run wizard, API key, libraries, a
#                                        second user (once; scripts/emby-test-setup.py)
#   scripts/emby-test-server.sh stop     stop the container (its data is kept)
#   scripts/emby-test-server.sh status
#
# Emby listens on 127.0.0.1:18096 only. Everything it keeps is under
# build/emby-test/ (never versioned): config/ (Emby's own), media/ (short
# clips drawn by ffmpeg, named after the Blender Foundation's free films, with
# .nfo files so that Emby needs nothing from the internet), credentials.json
# (the test admin and the API key) and config.json (a Régie5 config with Emby
# alone:  scripts/run-linux.sh --config build/emby-test/config.json).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="${ROOT}/build/emby-test"
IMAGE="${REGIE5_EMBY_IMAGE:-emby/embyserver:latest}"
NAME=regie5-emby-test
PORT=18096

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
        "$1"
}

nfo() {   # nfo <file> <text>: written once, never over an existing file
    [[ -e "$1" ]] || printf '%s\n' "$2" >"$1"
}

movie() {   # movie <title> <year> <tmdb> <imdb> <size> <codec> <source> <colour>
    local d="${DIR}/media/movies/$1 ($2)"
    clip "${d}/$1 ($2).mkv" 20 "$5" "$6" "$7"
    picture "${d}/poster.jpg" 500x750 "$8" "$1"
    picture "${d}/fanart.jpg" 1280x720 "$8" "$1"
    nfo "${d}/$1 ($2).nfo" "<movie><title>$1</title><year>$2</year><uniqueid type=\"tmdb\">$3</uniqueid><uniqueid type=\"imdb\">$4</uniqueid><plot>A test clip for Régie5.</plot></movie>"
}

make_media() {
    command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to draw the clips" >&2; exit 1; }
    movie "Big Buck Bunny" 2008 10378 tt1254207 1280x720 libx264 testsrc2 0x3a7d44
    movie "Elephants Dream" 2006 9761 tt0807840 1280x720 libx264 smptebars 0x7d4a3a
    movie "Sintel" 2010 45745 tt1727587 1920x1080 libx264 testsrc 0x3a5a7d
    # HEVC 4K: what a client that cannot play it makes Emby transcode.
    movie "Tears of Steel" 2012 133701 tt2285752 3840x2160 libx265 testsrc2 0x5a3a7d
    local s="${DIR}/media/shows/Caminandes"
    mkdir -p "${s}/Season 01"
    picture "${s}/poster.jpg" 500x750 0x7d6a3a "Caminandes"
    picture "${s}/fanart.jpg" 1280x720 0x7d6a3a "Caminandes"
    nfo "${s}/tvshow.nfo" "<tvshow><title>Caminandes</title><year>2013</year><uniqueid type=\"tvdb\">276536</uniqueid><plot>A test series for Régie5.</plot></tvshow>"
    for e in 1 2 3; do
        clip "${s}/Season 01/Caminandes S01E0${e}.mkv" 15 1280x720 libx264 testsrc2
        nfo "${s}/Season 01/Caminandes S01E0${e}.nfo" "<episodedetails><title>Episode ${e}</title><season>1</season><episode>${e}</episode></episodedetails>"
    done
}

case "${1:-}" in
start)
    make_media
    mkdir -p "${DIR}/config"
    if docker ps -a --format '{{.Names}}' | grep -qx "${NAME}"; then
        docker start "${NAME}" >/dev/null
    else
        docker run -d --name "${NAME}" -p "127.0.0.1:${PORT}:8096" \
            -e UID="$(id -u)" -e GID="$(id -g)" -e GIDLIST="$(id -g)" \
            -v "${DIR}/config:/config" -v "${DIR}/media:/media:ro" "${IMAGE}" >/dev/null
    fi
    echo "Emby: http://127.0.0.1:${PORT} (container ${NAME}, data in build/emby-test/)"
    ;;
setup) exec python3 "${ROOT}/scripts/emby-test-setup.py" "http://127.0.0.1:${PORT}" "${DIR}" ;;
stop) docker stop "${NAME}" >/dev/null && echo "stopped (data kept in build/emby-test/)" ;;
status) docker ps -a --filter "name=^${NAME}$" --format '{{.Names}}: {{.Status}} ({{.Image}})' ;;
*) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
