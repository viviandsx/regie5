#!/usr/bin/env bash
# Regie5 — fetches Mbed TLS (HTTPS on the PS5's socket transport, and on
# Linux with REGIE5_HTTP=socket) into toolchain/ (not versioned).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/fetch-mbedtls.sh       prints the source folder
#
# Also fetches the certificate authorities HTTPS is checked against on the
# PS5 (Mozilla's, as curl.se extracts them, MPL-2.0): toolchain/dl/cacert-<date>.pem,
# shipped in the title's assets by scripts/build-ps5.sh.
#
# Mbed TLS 3.6 (long-term support), Apache-2.0, the release's own checksum
# pinned here. Its library sources are compiled into Regie5 with Regie5's own
# configuration (src/platform/posix/mbedtls_config.h): a TLS client, nothing
# else. Nothing already fetched is replaced.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VER=3.6.7
SHA=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
DL="${ROOT}/toolchain/dl"
SRC="${ROOT}/toolchain/mbedtls-${VER}"

if [[ ! -f "${SRC}/library/ssl_tls.c" ]]; then
    mkdir -p "${DL}"
    arch="${DL}/mbedtls-${VER}.tar.bz2"
    if [[ ! -f "${arch}" ]]; then
        curl -fsSL -o "${arch}.part" \
            "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${VER}/mbedtls-${VER}.tar.bz2"
        mv "${arch}.part" "${arch}"
    fi
    echo "${SHA}  ${arch}" | sha256sum -c - >/dev/null
    tar -xjf "${arch}" -C "${ROOT}/toolchain"
fi
[[ -f "${SRC}/library/ssl_tls.c" ]] || { echo "Mbed TLS not found in ${SRC}" >&2; exit 1; }

CA_DATE=2026-09-25
CA_SHA=a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505
CA="${DL}/cacert-${CA_DATE}.pem"
if [[ ! -f "${CA}" ]]; then
    mkdir -p "${DL}"
    curl -fsSL -o "${CA}.part" "https://curl.se/ca/cacert-${CA_DATE}.pem"
    mv "${CA}.part" "${CA}"
fi
echo "${CA_SHA}  ${CA}" | sha256sum -c - >/dev/null
echo "${SRC}"
