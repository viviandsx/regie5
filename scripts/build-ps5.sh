#!/usr/bin/env bash
# Regie5 — builds the PS5 title folder.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/build-ps5.sh <name> [probe|app]
#       probe (default): the probe, PPSA99521, demo data, e.g. probe-7
#       app: Regie5 itself, PPSA99520, the real servers (config.json), e.g. app-1
#
# Output: build/ps5/<name>/<TITLE_ID>/ (eboot.bin, sce_sys/, sce_module/,
# assets/), the folder to copy into /data/homebrew/ on the console, and its
# .zip. <name> must be new: nothing already built is ever overwritten.
#
# The compile, link, FSELF signing and packaging steps are those of
# ps5-homebrew-ui (BlackBearReloaded, GPL-3.0-or-later), from the
# ps5-native-app-boilerplate, run on a tree assembled from Regie5's sources in
# a fresh temporary folder (TMPDIR). Graphics: ps5-opengl (OpenGL 4.6 Core,
# static). FreeType comes from the pacbrew sysroot.
#
# Needs: scripts/setup-ps5-toolchain.sh run once; clang, lld, ninja, python3.
# Development builds send their log over UDP to this machine
# (REGIE5_LOG_HOST overrides the address; REGIE5_LOG_HOST=none: no UDP log).
# REGIE5_PARAM=<file>: another param.json for an app test build (to compare
# categories), instead of ps5/app/sce_sys/param.json.
# REGIE5_DRY_REQUESTS=1: Seerr requests are logged, never sent (console tests).
# REGIE5_SHOW_VERSION=<v>: the version a test build shows (contentVersion kept).
#
# Category: 0.3.1 is back in the Games area with 0.2.0's param.json and
# attribute3's 120 Hz bits (0x80040) added: it starts, listens and gets 120 Hz
# (app-14). Jelly5's param.json profile starts in the Media
# area on firmware 13.60 (category 65536, badge 1, the launchActivity intent,
# attribute 0, attribute3 0x80040, no amm or kernel block) but the console
# then refuses to let the app listen (0.3.0: the phone page fails). With ps5-homebrew-ui's
# Media profile (badge 2, no game intent: app-10) or with our former
# attributes (app-10c) the console said "cannot start the application".
# ps5/app/param-profiles.json keeps what each profile did on the console
# (scripts/check-param.py). ps5-homebrew-ui's check is widened, in its
# temporary copy, to accept 65536 with badge 1 and launchActivity.
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:-}"
KIND="${2:-probe}"
[[ "${NAME}" =~ ^[A-Za-z0-9._-]{1,40}$ && "${KIND}" =~ ^(probe|app)$ ]] || { sed -n '4,12p' "$0" >&2; exit 2; }
OUT="${ROOT}/build/ps5/${NAME}"
[[ ! -e "${OUT}" ]] || { echo "${OUT} exists: choose a new name" >&2; exit 2; }

TC="${ROOT}/toolchain"
HUI="${TC}/ps5-homebrew-ui"
HUI_COMMIT=aed286b81cdf70a814d3059f54633ba34844e46e
OGL_VER=1.0.1
OGL_SHA=aaa2e8957f55e1fc0b654dcb35a36e585f7e635d63952da40a28be7f64fde741
OGL="${TC}/ps5-opengl-sdk-${OGL_VER}/sdk"
HB="${TC}/ps5-payload-sdk-v0.44/target/user/homebrew"

# --- The toolchain pieces, fetched once into toolchain/ (not versioned) ------
[[ -d "${HB}/include/freetype2" ]] || { echo "run scripts/setup-ps5-toolchain.sh first" >&2; exit 2; }
if [[ ! -d "${HUI}/.git" ]]; then
    git clone -q https://github.com/blackbearreloaded/ps5-homebrew-ui.git "${HUI}"
fi
[[ "$(git -C "${HUI}" rev-parse HEAD)" == "${HUI_COMMIT}" ]] ||
    git -C "${HUI}" -c advice.detachedHead=false checkout -q "${HUI_COMMIT}"
if [[ ! -f "${OGL}/manifest.sha256" ]]; then
    mkdir -p "${TC}/dl"
    arch="${TC}/dl/ps5-opengl-sdk-${OGL_VER}.tar.gz"
    [[ -f "${arch}" ]] || {
        curl -fSL -o "${arch}.part" \
            "https://github.com/blackbearreloaded/ps5-opengl/releases/download/v${OGL_VER}/ps5-opengl-sdk-${OGL_VER}.tar.gz"
        mv "${arch}.part" "${arch}"
    }
    echo "${OGL_SHA}  ${arch}" | sha256sum -c - >/dev/null
    tar -xzf "${arch}" -C "${TC}"
fi

# --- The title -------------------------------------------------------------------
SCE_SYS="${ROOT}/ps5/sce_sys"            # the probe's
[[ "${KIND}" == app ]] && SCE_SYS="${ROOT}/ps5/app/sce_sys"
PARAM="${SCE_SYS}/param.json"
if [[ -n "${REGIE5_PARAM:-}" ]]; then
    [[ "${KIND}" == app && -f "${REGIE5_PARAM}" ]] || { echo "REGIE5_PARAM: an app build and an existing file" >&2; exit 2; }
    PARAM="${REGIE5_PARAM}"
fi
# The profile against those tried on a console (0.3.0's could not listen):
# a warning here, a refusal in release-ps5.sh.
[[ "${KIND}" == app ]] && python3 "${ROOT}/scripts/check-param.py" "${PARAM}"
TITLE_ID="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "${PARAM}")"
# contentVersion 00.001.000 is shown as 0.1.0
VERSION="$(python3 -c 'import json,sys; print(".".join(str(int(p)) for p in json.load(open(sys.argv[1]))["contentVersion"].split(".")))' "${PARAM}")"
# REGIE5_SHOW_VERSION=0.5.0-test: a test build shown as the version it tries
# out (Settings, What's new), its contentVersion unchanged.
[[ -z "${REGIE5_SHOW_VERSION:-}" ]] || VERSION="${REGIE5_SHOW_VERSION}"
[[ "${VERSION}" =~ ^[0-9A-Za-z.+-]{1,24}$ ]] || { echo "bad version: ${VERSION}" >&2; exit 2; }
# 120 Hz may be asked for only with attribute3's bit 0x40 (ps5-opengl's
# display-modes.md, the boilerplate's DISPLAY_MODES.md).
HZ120="$(python3 -c 'import json,sys; print(1 if json.load(open(sys.argv[1])).get("attribute3", 0) & 0x40 else 0)' "${PARAM}")"
LOG_HOST="${REGIE5_LOG_HOST:-}"
if [[ -z "${LOG_HOST}" ]]; then   # this machine's address on the local network
    LOG_HOST="$(ip route get 1.1.1.1 2>/dev/null |
        awk '{for (i = 1; i < NF; i++) if ($i == "src") { print $(i + 1); exit }}' || true)"
fi
[[ "${LOG_HOST}" == none ]] && LOG_HOST=""

# --- The tree, in a fresh temporary folder -------------------------------------
WORK="$(mktemp -d)"
T="${WORK:?}/tree"
mkdir -p "${T}"
for item in Makefile tools tooling runtime; do
    cp -a "${HUI}/${item}" "${T}/"
done
mkdir -p "${T}/sce_sys" "${T}/assets" "${T}/src/platform" "${T}/src/runtime" "${T}/src/third_party" "${T}/deps/lib"
if [[ "${KIND}" == app ]]; then   # icon0.png is kept in the repository (the catalog links it); pic0, pic1 are drawn
    cp "${PARAM}" "${T}/sce_sys/param.json"
    cp "${SCE_SYS}/icon0.png" "${T}/sce_sys/"
    python3 "${ROOT}/scripts/make-icons.py" "${T}/sce_sys" --backgrounds
    bash "${HUI}/tools/validate-assets.sh" "${T}/sce_sys"
else
    cp "${SCE_SYS}/"* "${T}/sce_sys/"
fi
# Jelly5's Media profile (see the top of this file), in the temporary copy of
# ps5-homebrew-ui's check only.
python3 - "${T}/tools/build.sh" <<'PY'
import sys
path = sys.argv[1]
s = open(path, encoding="utf-8").read()
old_set = "if (category, badge) not in {(0, 1), (65536, 2)}:"
old_intent = 'elif "gameIntent" in value:'
assert s.count(old_set) == 1 and s.count(old_intent) == 1, "ps5-homebrew-ui's param.json check changed: look at it again"
s = s.replace(old_set, "if (category, badge) not in {(0, 1), (65536, 1), (65536, 2)}:")
s = s.replace(old_intent, 'elif badge == 2 and "gameIntent" in value:')
open(path, "w", encoding="utf-8").write(s)
PY
cp -a "${ROOT}/assets/fonts" "${T}/assets/fonts"
[[ "${KIND}" == probe ]] && cp -a "${ROOT}/fixtures" "${T}/assets/fixtures"   # the demo data
for d in core api ui; do
    cp -a "${ROOT}/src/${d}" "${T}/src/${d}"
done
cp "${ROOT}/src/platform/platform.h" "${T}/src/platform/"
cp -a "${ROOT}/src/platform/gl" "${T}/src/platform/gl"
mkdir -p "${T}/src/platform/ps5"
cp "${ROOT}/src/platform/ps5/"*.c "${ROOT}/src/platform/ps5/"*.cpp "${ROOT}/src/platform/ps5/"*.h "${T}/src/platform/ps5/"
cp -a "${ROOT}/src/platform/posix" "${T}/src/platform/posix"
cp "${ROOT}/src/platform/ps5/runtime/"*.c "${T}/src/runtime/"
cp -a "${ROOT}/third_party/cjson" "${T}/src/third_party/cjson"
cp -a "${ROOT}/third_party/stb" "${T}/src/third_party/stb"
cp -a "${ROOT}/third_party/qrcodegen" "${T}/src/third_party/qrcodegen"
# HTTPS: Mbed TLS's library with Regie5's configuration in place of its own,
# and (the app) the certificate authorities it checks against.
MBED="$("${ROOT}/scripts/fetch-mbedtls.sh")"
mkdir -p "${T}/src/third_party/mbedtls"
cp -a "${MBED}/include" "${MBED}/library" "${T}/src/third_party/mbedtls/"
cp "${ROOT}/src/platform/posix/mbedtls_config.h" "${T}/src/third_party/mbedtls/include/mbedtls/mbedtls_config.h"
if [[ "${KIND}" == app ]]; then
    mkdir -p "${T}/assets/certs"
    cp "$(ls "${ROOT}"/toolchain/dl/cacert-*.pem | sort | tail -1)" "${T}/assets/certs/cacert.pem"
fi
ln -s "${HB}/include/freetype2" "${T}/deps/freetype2"
for a in libfreetype libpng16 libz libbz2; do
    ln -s "${HB}/lib/${a}.a" "${T}/deps/lib/${a}.a"
done
{
    echo "/* Written by scripts/build-ps5.sh. */"
    echo "#pragma once"
    echo "#define REGIE5_VERSION \"${VERSION}\""
    echo "#define REGIE5_LOG_HOST \"${LOG_HOST}\""
    echo "#define REGIE5_LOG_PORT 5555"
    echo "#define REGIE5_TITLE_ID \"${TITLE_ID}\""
    echo "#define REGIE5_HZ120 ${HZ120}"
} > "${T}/src/platform/ps5/build_info.h"

echo "==> ${TITLE_ID} ${VERSION}, 120 Hz $([[ ${HZ120} == 1 ]] && echo possible || echo "not declared"), log -> ${LOG_HOST:-none}:5555, tree ${T}"
DEFS="REGIE5_TLS=1 REGIE5_PROBE=1"
[[ "${KIND}" == probe ]] || DEFS="REGIE5_TLS=1"
# REGIE5_DRY_REQUESTS=1: a test build that never sends a Seerr request (logged instead).
[[ "${REGIE5_DRY_REQUESTS:-0}" == 1 ]] && DEFS+=" REGIE5_DRY_REQUESTS=1"
# The system keyboard (src/platform/ps5/text_ps5.cpp) needs
# sceCommonDialogInitialize, which the SDK has no import stub for: a stub
# library naming the module and that one symbol, as Jelly5 makes it.
mkdir -p "${T}/deps/stubs"
echo 'void sceCommonDialogInitialize(void) {}' > "${T}/deps/stubs/libSceCommonDialog.c"
"${HUI}/.deps/native/ps5-payload-sdk/bin/prospero-clang" -shared -nostdlib -nodefaultlibs -fPIC \
    -Wl,-soname,libSceCommonDialog.sprx -o "${T}/deps/stubs/libSceCommonDialog.so" "${T}/deps/stubs/libSceCommonDialog.c"
(
    cd "${T}"
    PS5_CLANG="${PS5_CLANG:-clang}" USE_CCACHE=0 BUILD_JOBS="${BUILD_JOBS:-4}" \
        PS5_OPENGL_PREFIX="${OGL}" \
        APP_DEFINITIONS="${DEFS}" \
        APP_IMPORT_STUBS="deps/stubs/libSceCommonDialog.so" \
        APP_INCLUDE_PATHS="src/third_party/cjson src/third_party/stb src/third_party/qrcodegen src/third_party/mbedtls/include src/third_party/mbedtls/library deps/freetype2" \
        APP_STATIC_ARCHIVES="deps/lib/libfreetype.a deps/lib/libpng16.a deps/lib/libz.a deps/lib/libbz2.a" \
        make app
) > "${WORK}/build.log" 2>&1 || { tail -40 "${WORK}/build.log"; echo "build failed: ${WORK}/build.log" >&2; exit 1; }

mkdir -p "${OUT}"
cp -a "${T}/dist/${TITLE_ID}" "${OUT}/${TITLE_ID}"
cp "${T}/dist/${TITLE_ID}.zip" "${OUT}/${TITLE_ID}.zip"
cp "${WORK}/build.log" "${OUT}/build.log"
cp "${T}/build/llvm-pie.elf" "${OUT}/llvm-pie.elf"   # to find a crash address's function
echo "==> ${OUT}/${TITLE_ID}/ ($(du -sh "${OUT}/${TITLE_ID}" | cut -f1))"
echo "    (the temporary tree ${WORK} can be removed)"
