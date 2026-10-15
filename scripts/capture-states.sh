#!/usr/bin/env bash
# Regie5 — captures every screen in every state, in French and English, for
# review: data, loading, empty, errors, services off, no configuration, two
# Radarr instances, versions too old, Emby in Jellyfin's place, the welcome
# screens. The Ménage is turned on (REGIE5_MOCK_CLEANUP, in memory).
# Runs on fixtures/ (--mock): no server is contacted, no action is carried out;
# the welcome screens' phone step is the demo one (nothing listens); "What's
# new" (whatsnew); Demandes › Découvrir (discover); Quick Connect (quickconnect).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/capture-states.sh                 all states, both languages
#   scripts/capture-states.sh empty offline   only these states
#   OUT=/tmp/states scripts/capture-states.sh
#   LANGS=de scripts/capture-states.sh        other languages (default: fr en)
#
# Writes <OUT>/<state>-<lang>-<screen>.png and one sheet per state and
# language, <OUT>/<state>-<lang>.png (ImageMagick; without it, PPM files only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/linux"
OUT="${OUT:-${ROOT}/build/states}"
ALL_STATES=(data loading empty offline unauthorized mixed off noconfig multi old emby welcome whatsnew discover quickconnect)
STATES=("$@")
[[ ${#STATES[@]} -eq 0 ]] && STATES=("${ALL_STATES[@]}")

if [[ ! -f "${BUILD}/build.ninja" ]]; then
    "${ROOT}/scripts/run-linux.sh" --version >/dev/null
fi
cmake --build "${BUILD}" >/dev/null
mkdir -p "${OUT}"

ALL=sonarr,radarr,seerr,jellyfin

# The tour: Home, the Library's views (series, films, guide, missing, cleanup),
# Activity (queue, journal), Requests, Jellyfin, Settings. With data, also the
# sheets: alerts, the Options menus (a recent addition, a series' page, a
# request, a cleanup candidate), a request, a session's diagnosis; opened,
# captured, closed with circle. Mock only: nothing could be changed anyway.
# A series' page has no Options menu with Emby (its titles are not in Emby's
# fixtures): detail_options=0 leaves that shot out.
tour() {
    local p="$1" sheets="$2" cleanup="$3" detail_options="${4:-1}" s=""
    s+="3 shot:${p}-home.ppm "
    [[ ${sheets} == 1 ]] && s+="up cross 1 shot:${p}-home-alerts.ppm circle 0.6 down "
    [[ ${sheets} == 1 ]] && s+="down options 1 shot:${p}-home-options.ppm circle 0.6 up "
    s+="r1 2.5 shot:${p}-library-series.ppm "
    if [[ ${sheets} == 1 ]]; then
        s+="down cross 1.5 shot:${p}-library-detail.ppm "
        [[ ${detail_options} == 1 ]] && s+="options 1 shot:${p}-library-detail-options.ppm circle 0.6 "
        s+="circle 0.6 up "
    fi
    s+="right cross 2.5 shot:${p}-library-movies.ppm "
    s+="right cross 1 down 2 shot:${p}-library-guide.ppm up right cross 2.5 shot:${p}-library-missing.ppm "
    # The Ménage, "seen by at least one" (no title is seen by everyone in the fixtures).
    [[ ${cleanup} == 1 ]] && s+="right cross 3 right cross 1 right cross 3 shot:${p}-library-cleanup.ppm "
    [[ ${cleanup} == 1 && ${sheets} == 1 ]] && s+="down options 1 shot:${p}-library-cleanup-options.ppm circle 0.6 "
    s+="r1 2.5 shot:${p}-activity-queue.ppm right cross 2.5 shot:${p}-activity-journal.ppm "
    s+="r1 2.5 shot:${p}-requests.ppm "
    [[ ${sheets} == 1 ]] && s+="down cross 1 shot:${p}-requests-sheet.ppm circle 0.6 options 1 shot:${p}-requests-options.ppm circle 0.6 up left "
    [[ ${sheets} == 1 ]] || s+="right "
    s+="cross 2.5 shot:${p}-requests-issues.ppm "
    [[ ${sheets} == 1 ]] && s+="down cross 1.5 shot:${p}-requests-issue-thread.ppm circle 0.6 options 1 shot:${p}-requests-issue-options.ppm circle 0.6 "
    s+="r1 2.5 shot:${p}-jellyfin.ppm "
    [[ ${sheets} == 1 ]] && s+="cross 1 shot:${p}-jellyfin-diagnosis.ppm circle 0.6 "
    s+="r1 3 shot:${p}-settings.ppm "
    # Settings' buttons: Test all, Cleanup, Display refresh, Setup (the welcome screens).
    [[ ${sheets} == 1 ]] && s+="down down down down down right right right 1 shot:${p}-settings-buttons.ppm "
    s+="quit"
    echo "${s}"
}

# Demandes › Découvrir: the rows, a search, a series' page and its request
# (the seasons; never sent: --mock), a film's page and its request (4K).
discover_tour() {
    local p="$1" s=""
    s+="3 r1 r1 r1 2 right right cross 2.5 shot:${p}-1-rows.ppm "
    s+="down 0.5 cross 0.5 text:yuki 2.5 shot:${p}-2-search.ppm "
    s+="cross 2 shot:${p}-3-series.ppm cross 1.5 shot:${p}-4-request.ppm circle 0.6 circle 0.6 "
    s+="up 0.3 cross 0.5 text:mirror 2.5 cross 2 shot:${p}-5-film.ppm cross 1.5 shot:${p}-6-request.ppm "
    s+="down down 0.5 cross 1 shot:${p}-7-confirm.ppm quit"
    echo "${s}"
}

# The welcome screens (REGIE5_WELCOME=1 over --mock): the first screen, the
# media server, the phone (Jellyfin, then Emby: the texts follow the choice),
# the check, then Start and the Home screen behind them.
welcome_tour() {
    local p="$1" s=""
    s+="3 shot:${p}-1-welcome.ppm cross 1.5 shot:${p}-2-server.ppm "
    s+="cross 1.5 shot:${p}-3-phone.ppm circle 1 right cross 1.5 shot:${p}-3-phone-emby.ppm "
    s+="cross 3 shot:${p}-4-check.ppm cross 3 shot:${p}-5-home.ppm quit"
    echo "${s}"
}

for state in "${STATES[@]}"; do
    for lang in ${LANGS:-fr en}; do
        p="${OUT}/${state}-${lang}"
        rm -f "${p}"-*.ppm "${p}"-*.png
        env=(REGIE5_MOCK_DELAY_MS=0 REGIE5_MOCK_CLEANUP=1)
        args=(--mock --lang "${lang}")
        sheets=0
        cleanup=1
        detail_options=1
        script=""
        case "${state}" in
        data) sheets=1 ;;
        loading) env=(REGIE5_MOCK_DELAY_MS=600000 REGIE5_MOCK_CLEANUP=1) ;;
        empty) env+=(REGIE5_MOCK_EMPTY="${ALL}") ;;
        offline) env+=(REGIE5_MOCK_FAIL="sonarr:unreachable,radarr:unreachable,seerr:unreachable,jellyfin:unreachable") ;;
        unauthorized) env+=(REGIE5_MOCK_FAIL="sonarr:unauthorized,radarr:unauthorized,seerr:unauthorized,jellyfin:unauthorized") ;;
        mixed) env+=(REGIE5_MOCK_FAIL="radarr:unreachable,seerr:badjson" REGIE5_MOCK_EMPTY=sonarr) ;;
        off) env+=(REGIE5_MOCK_OFF="${ALL}") ;;
        noconfig)
            # No config.json and "Later" already chosen: the screens without
            # the welcome on top (the "welcome" state shows them). settings.json
            # lives beside the missing config.json, in a folder of <OUT>.
            mkdir -p "${OUT}/noconfig"
            printf '{ "welcome": "later" }\n' > "${OUT:?}/noconfig/settings.json"
            args=(--config "${OUT}/noconfig/config.json" --lang "${lang}") cleanup=0 ;;
        multi) env+=(REGIE5_MOCK_MULTI=1) ;;
        old) env+=(REGIE5_MOCK_OLD="sonarr,jellyfin") ;;
        emby) env+=(REGIE5_MOCK_EMBY=1) sheets=1 detail_options=0 ;;
        welcome) env+=(REGIE5_WELCOME=1) script="$(welcome_tour "${p}")" ;;
        whatsnew) env+=(REGIE5_WHATSNEW=1) script="3 shot:${p}-whatsnew.ppm quit" ;;
        discover) script="$(discover_tour "${p}")" ;;
        quickconnect)   # Jellyfin with no key: the welcome screens' check, Quick Connect, then Settings
            env+=(REGIE5_MOCK_QC=1 REGIE5_WELCOME=1 REGIE5_MOCK_DELAY_MS=300)
            script="3 cross 1 cross 1.5 cross 3 shot:${p}-1-check.ppm right cross 1.5 shot:${p}-2-code.ppm "
            script+="3 shot:${p}-3-done.ppm cross 3 shot:${p}-4-checked.ppm quit" ;;
        *) echo "unknown state: ${state} (${ALL_STATES[*]})" >&2; exit 2 ;;
        esac
        echo "${state} (${lang})"
        [[ -n ${script} ]] || script="$(tour "${p}" "${sheets}" "${cleanup}" "${detail_options}")"
        env "${env[@]}" REGIE5_SCRIPT="${script}" "${BUILD}/regie5" "${args[@]}" >/dev/null 2>&1
        if command -v convert >/dev/null; then
            for f in "${p}"-*.ppm; do
                convert "${f}" -resize 960x540 "${f%.ppm}.png" && rm "${f}"
            done
            montage "${p}"-*.png -tile 3x -geometry +6+6 -background '#202024' "${p}.png" 2>/dev/null || true
        fi
    done
done
echo "-> ${OUT}"
