#!/usr/bin/env bash
# Regie5 — a playback on the throwaway Plex of scripts/plex-test-server.sh,
# without Plex Web and without an account: what the Plex tab, the playback
# diagnosis and "Stop playback" are tried against.
#   scripts/plex-test-play.sh transcode <ratingKey> [seconds]
#       a transcode to 720p H.264 (a 4K HEVC film is the best case), kept
#       alive by fetching its playlist and reporting the timeline
#   scripts/plex-test-play.sh direct <ratingKey> [seconds]
#       a direct play: the file read slowly while the timeline says "playing"
# ratingKey: a film or an episode of the test server (curl -H 'Accept:
# application/json' http://127.0.0.1:32400/library/sections/1/all lists them).
# Plex counts a playback past 90 % as watched (history), like a real client.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

B="http://127.0.0.1:32400"
MODE="${1:-}" KEY="${2:-}" SECS="${3:-90}"
[[ "${MODE}" =~ ^(transcode|direct)$ && "${KEY}" =~ ^[0-9]+$ ]] || { sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
SESSION="regie5-${MODE}-${KEY}-$$"
if [[ "${MODE}" == transcode ]]; then
    HD=(-H "X-Plex-Client-Identifier: ${SESSION}" -H 'X-Plex-Product: Plex Web' -H 'X-Plex-Platform: Firefox'
        -H 'X-Plex-Device-Name: Test browser' -H 'X-Plex-Version: 4.150.0' -H 'Accept: application/json')
else
    HD=(-H "X-Plex-Client-Identifier: ${SESSION}" -H 'X-Plex-Product: Plex for Roku' -H 'X-Plex-Platform: Roku'
        -H 'X-Plex-Device-Name: Test TV' -H 'Accept: application/json')
fi
SID="X-Plex-Session-Identifier=${SESSION}"

meta="$(curl -sf "${HD[@]}" "${B}/library/metadata/${KEY}")"
DURATION="$(python3 -I -c 'import json,sys; print(json.load(sys.stdin)["MediaContainer"]["Metadata"][0]["duration"])' <<<"${meta}")"
PART="$(python3 -I -c 'import json,sys; print(json.load(sys.stdin)["MediaContainer"]["Metadata"][0]["Media"][0]["Part"][0]["key"])' <<<"${meta}")"

timeline() {   # timeline <state> <offset ms>
    curl -s -X POST "${HD[@]}" -o /dev/null \
        "${B}/:/timeline?ratingKey=${KEY}&key=%2Flibrary%2Fmetadata%2F${KEY}&state=$1&time=$2&duration=${DURATION}&${SID}"
}

if [[ "${MODE}" == transcode ]]; then
    Q="path=%2Flibrary%2Fmetadata%2F${KEY}&mediaIndex=0&partIndex=0&protocol=hls&fastSeek=1&directPlay=0&directStream=0"
    Q+="&location=lan&videoResolution=1280x720&maxVideoBitrate=2000&session=${SESSION}&${SID}"
    curl -sf "${HD[@]}" -o /dev/null "${B}/video/:/transcode/universal/decision?${Q}"
    curl -sf "${HD[@]}" -o /dev/null "${B}/video/:/transcode/universal/start.m3u8?${Q}"
else
    curl -s "${HD[@]}" --limit-rate 20k --max-time "${SECS}" -o /dev/null "${B}${PART}?${SID}" &
fi
echo "playing ${KEY} (${MODE}) for ${SECS} s, session ${SESSION}"

end=$((SECONDS + SECS)) offset=0 step=$((DURATION / SECS + 1))
while ((SECONDS < end)); do
    [[ "${MODE}" == transcode ]] &&
        curl -s "${HD[@]}" -o /dev/null "${B}/video/:/transcode/universal/session/${SESSION}/base/index.m3u8"
    timeline playing "${offset}"
    offset=$((offset + step)); ((offset > DURATION)) && offset=${DURATION}
    sleep 1
done
timeline stopped "${offset}"
wait
echo "stopped"
