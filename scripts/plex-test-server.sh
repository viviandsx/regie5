#!/usr/bin/env bash
# Regie5 — a throwaway Plex Media Server on this machine, to develop and test
# Plex support without anybody's real server, and without a Plex account.
#   scripts/plex-test-server.sh start    make the demo library (once), start Plex
#   scripts/plex-test-server.sh setup    libraries, a Régie5 config (once;
#                                        scripts/plex-test-setup.py)
#   scripts/plex-test-server.sh stop     stop the container (its data is kept)
#   scripts/plex-test-server.sh status
#   scripts/plex-test-server.sh claim    link it to a Plex test account, to try signing
#                                        in from the phone page: asks for a claim code
#                                        (https://www.plex.tv/claim, signed in to that
#                                        account), never shown nor kept; Docker's networks
#                                        no longer get in without a token
#   scripts/plex-test-server.sh unclaim  sign it out of that account again (remove it
#                                        from the account on plex.tv too), back to no account
#
# Plex listens on 127.0.0.1:32400 only (REGIE5_PLEX_ADDR=<this machine's LAN
# address> to reach it from the console too). The server is not signed in
# to a Plex account (but after "claim"): it answers without a token to the
# addresses in ALLOWED_NETWORKS, Docker's own networks, which is how this
# machine reaches it. Everything it keeps is under build/plex-test/ (never
# versioned): config/ (Plex's own), transcode/, media/ (short clips drawn by
# ffmpeg, named after the Blender Foundation's free films, with posters and
# backdrops at a real server's size) and config.json (a Régie5 config with
# Plex alone:  scripts/run-linux.sh --config build/plex-test/config.json).
# Plex's agents fetch the films' metadata from Plex's servers (no account).
# Remote access is off (PublishServerOnPlexOnlineKey), linked to an account or not.
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

# The container, made afresh (its data stays in build/plex-test/): every
# argument is an -e for it. Docker's networks get in without a token unless
# told otherwise (ALLOWED_NETWORKS=127.0.0.2/32: nobody).
run_container() {
    docker run -d --name "${NAME}" -p "${ADDR}:${PORT}:32400/tcp" \
        -e TZ="$(cat /etc/timezone 2>/dev/null || echo UTC)" \
        -e PLEX_UID="$(id -u)" -e PLEX_GID="$(id -g)" \
        "$@" \
        -v "${DIR}/config:/config" -v "${DIR}/transcode:/transcode" \
        -v "${DIR}/media:/data:ro" "${IMAGE}" >/dev/null
}

claimed() {   # claimed: true / false, as /identity says it
    curl -s -m 3 -H 'Accept: application/json' "http://127.0.0.1:${PORT}/identity" |
        python3 -I -c 'import json,sys; print(str(json.load(sys.stdin)["MediaContainer"].get("claimed", False)).lower())' 2>/dev/null
}

case "${1:-}" in
start)
    make_media
    mkdir -p "${DIR}/config" "${DIR}/transcode"
    if docker ps -a --format '{{.Names}}' | grep -qx "${NAME}"; then
        docker start "${NAME}" >/dev/null
    else
        run_container -e ALLOWED_NETWORKS=172.16.0.0/12
    fi
    echo "Plex: http://${ADDR}:${PORT} (container ${NAME}, data in build/plex-test/)"
    [[ "$(docker port "${NAME}" 32400/tcp)" == "${ADDR}:${PORT}" ]] ||
        echo "note: the container was made for $(docker port "${NAME}" 32400/tcp); remove it (docker rm ${NAME}) to change the address" >&2
    ;;
setup) exec python3 "${ROOT}/scripts/plex-test-setup.py" "http://127.0.0.1:${PORT}" "${DIR}" ;;
claim)
    [[ "$(claimed)" == false ]] || { echo "the test server is not running unclaimed (start it first)" >&2; exit 1; }
    read -rsp "Claim code from https://www.plex.tv/claim (not shown): " code
    echo
    [[ "${code}" =~ ^claim-[A-Za-z0-9_-]+$ ]] || { echo "not a claim code (claim-...)" >&2; exit 1; }
    docker rm -f "${NAME}" >/dev/null
    run_container -e PLEX_CLAIM="${code}" -e ALLOWED_NETWORKS=127.0.0.2/32
    for _ in $(seq 1 60); do
        [[ "$(claimed)" == true ]] && { echo "linked to the account; a token is needed now"; exit 0; }
        sleep 2
    done
    echo "not linked after 2 minutes: the code lasts a few minutes only, take a new one" >&2
    exit 1
    ;;
unclaim)
    # The server's account token goes from its own settings (Plex's tool, in
    # the container), then the container is made as at "start".
    docker exec "${NAME}" sh -c 'f="/config/Library/Application Support/Plex Media Server/Preferences.xml";
        for a in PlexOnlineToken PlexOnlineUsername PlexOnlineMail PlexOnlineHome; do
            xmlstarlet ed --inplace -d "/Preferences/@$a" "$f"; done'
    docker rm -f "${NAME}" >/dev/null
    run_container -e ALLOWED_NETWORKS=172.16.0.0/12
    for _ in $(seq 1 60); do
        [[ "$(claimed)" == false ]] && { echo "back to no account (remove it from the account on plex.tv too)"; exit 0; }
        sleep 2
    done
    echo "still linked: see docs/PLEX.md" >&2
    exit 1
    ;;
stop) docker stop "${NAME}" >/dev/null && echo "stopped (data kept in build/plex-test/)" ;;
status) docker ps -a --filter "name=^${NAME}$" --format '{{.Names}}: {{.Status}} ({{.Image}})' ;;
*) sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
