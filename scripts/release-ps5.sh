#!/usr/bin/env bash
# Regie5 — builds the PS5 release zip.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/release-ps5.sh            build, then zip
#   scripts/release-ps5.sh --repack   zip again the folder already built (a new
#                                     zip name each time is not needed: upload/
#                                     must not hold one yet)
#
# Output: build/ps5/release-<version>/upload/PPSA99520.zip, one folder at the top, as
# the homebrew catalog (homebrew.page) wants it, to copy into /data/homebrew/:
#   PPSA99520/                  the title (eboot.bin, sce_sys/, sce_module/, assets/)
#   PPSA99520/config.example.json, INSTALL.txt, LICENSE, THIRD_PARTY_NOTICES.md
#   PPSA99520/licenses/ps5-opengl/   the notices and licences of what ps5-opengl
#                               (linked into eboot.bin) is built from: Mesa…
#   PPSA99520/licenses/mbedtls/LICENSE   Mbed TLS (HTTPS), Apache-2.0
# The version is the app's param.json contentVersion (00.002.000 -> 0.2.0); the
# asset keeps its name from one release to the next (the catalog's update job).
#
# A release never holds a config.json: the zip is checked, and the script stops
# if one is found. Copying the folder over an installed one therefore keeps the
# user's config.json (INSTALL.txt says so). No UDP log in a release.
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PARAM="${ROOT}/ps5/app/sce_sys/param.json"
VERSION="$(python3 -c 'import json,sys; print(".".join(str(int(p)) for p in json.load(open(sys.argv[1]))["contentVersion"].split(".")))' "${PARAM}")"
TITLE_ID="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "${PARAM}")"
# Only a profile seen on a console to start and to listen (0.3.0 could not).
python3 "${ROOT}/scripts/check-param.py" "${PARAM}" --release
NAME="${REGIE5_RELEASE_NAME:-release-${VERSION}}"   # another name: a second candidate of one version
OUT="${ROOT}/build/ps5/${NAME}"
REPACK=0
[[ "${1:-}" == --repack ]] && REPACK=1
if [[ ${REPACK} == 0 ]]; then
    [[ ! -e "${OUT}" ]] || { echo "${OUT} exists: raise contentVersion in ps5/app/sce_sys/param.json (or --repack)" >&2; exit 2; }
else
    [[ -f "${OUT}/${TITLE_ID}/eboot.bin" ]] || { echo "nothing built in ${OUT}: run without --repack" >&2; exit 2; }
fi
if [[ -n "$(git -C "${ROOT}" status --porcelain 2>/dev/null)" ]]; then
    echo "note: the working tree has changes not committed; the release is built from it as it is" >&2
fi

[[ ${REPACK} == 1 ]] || REGIE5_LOG_HOST=none REGIE5_DRY_REQUESTS=0 REGIE5_SHOW_VERSION= "${ROOT}/scripts/build-ps5.sh" "${NAME}" app

# Not next to the folder: the build tools leave a <TITLE_ID>.zip of their own there.
mkdir -p "${OUT}/upload"
ZIP="${OUT}/upload/${TITLE_ID}.zip"
OGL="${ROOT}/toolchain/ps5-opengl-sdk-1.0.1"
python3 - "${OUT}/${TITLE_ID}" "${TITLE_ID}" "${ROOT}" "${ZIP}" "${VERSION}" "${OGL}" <<'PY'
import os, sys, zipfile
folder, title_id, root, zip_path, version, ogl = sys.argv[1:]
entries = []   # (path in the zip, file on disk)
for base, dirs, files in os.walk(folder):
    dirs.sort()
    for f in sorted(files):
        disk = os.path.join(base, f)
        entries.append((os.path.join(title_id, os.path.relpath(disk, folder)), disk))
t = title_id + "/"
entries.append((t + "config.example.json", os.path.join(root, "config.example.json")))
entries.append((t + "INSTALL.txt", os.path.join(root, "ps5", "app", "INSTALL.txt")))
entries.append((t + "LICENSE", os.path.join(root, "LICENSE")))
entries.append((t + "THIRD_PARTY_NOTICES.md", os.path.join(root, "THIRD_PARTY_NOTICES.md")))
entries.append((t + "licenses/ps5-opengl/THIRD_PARTY_NOTICES.md", os.path.join(ogl, "THIRD_PARTY_NOTICES.md")))
entries.append((t + "licenses/ps5-opengl/LICENSE", os.path.join(ogl, "LICENSE")))
mbed = os.path.join(root, "toolchain", "mbedtls-3.6.7")
entries.append((t + "licenses/mbedtls/LICENSE", os.path.join(mbed, "LICENSE")))
for base, dirs, files in os.walk(os.path.join(ogl, "LICENSES")):
    dirs.sort()
    for f in sorted(files):
        disk = os.path.join(base, f)
        entries.append((t + "licenses/ps5-opengl/LICENSES/" + os.path.relpath(disk, os.path.join(ogl, "LICENSES")), disk))
bad = [z for z, _ in entries if os.path.basename(z).lower() in ("config.json", "settings.json")]
if bad:
    sys.exit("refused: a release never holds " + ", ".join(bad))
with open(zip_path, "xb") as out, zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for name, disk in entries:
        info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))   # same bytes for the same files
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        with open(disk, "rb") as f:
            z.writestr(info, f.read())
with zipfile.ZipFile(zip_path) as z:   # checked again, as written
    names = z.namelist()
    assert not any(os.path.basename(n).lower() == "config.json" for n in names), "config.json in the zip"
    assert f"{title_id}/eboot.bin" in names and f"{title_id}/sce_sys/param.json" in names
    assert all(n.startswith(t) for n in names), "one folder at the top of the zip"
print(f"==> {zip_path} ({len(names)} files, version {version})")
PY
sha256sum "${ZIP}" | sed "s#${OUT}/upload/##" > "${ZIP}.sha256"
cat "${ZIP}.sha256"
