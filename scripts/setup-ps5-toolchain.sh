#!/usr/bin/env bash
# Regie5 — sets up the PS5 cross toolchain on Linux, with the distribution's
# LLVM (clang, lld, llvm). After Jelly5's scripts/setup-toolchain.sh.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/setup-ps5-toolchain.sh          download (checksums pinned), extract
#   eval "$(scripts/setup-ps5-toolchain.sh --env)"
#
# Result: toolchain/ps5-payload-sdk (not versioned), with the pacbrew homebrew
# sysroot (SDL2 with OpenGL ES through Mesa, FreeType, curl...) merged into
# target/user/homebrew. Nothing already there is deleted: a version change goes
# into a new folder name.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="${ROOT}/toolchain"
DL="${TC}/dl"
SDK_VER=v0.44
PB_VER=v0.41.1
SDK="${TC}/ps5-payload-sdk-${SDK_VER}"

SDK_URL="https://github.com/ps5-payload-dev/sdk/releases/download/${SDK_VER}/ps5-payload-sdk.zip"
SDK_SHA="${REGIE5_SDK_SHA:-4d06eab777bb135705b5a6816688c9132efb246093fe606ac488ab6cf65ca5b0}"
PB_URL="https://github.com/ps5-payload-dev/pacbrew-repo/releases/download/${PB_VER}/ps5-payload-dev.tar.gz"
PB_SHA="${REGIE5_PB_SHA:-e811bc2e4676c5bb7f80a8fe928d2985dce599996a5ca6b54da3e72699287887}"

llvm_config() {
    if [[ -n "${LLVM_CONFIG:-}" && -x "$(command -v "${LLVM_CONFIG}")" ]]; then
        command -v "${LLVM_CONFIG}"
        return
    fi
    local v
    for v in 23 22 21 20 19 18; do
        command -v "llvm-config-${v}" 2>/dev/null && return
    done
    command -v llvm-config 2>/dev/null || true
}

if [[ "${1:-}" == "--env" ]]; then
    config="$(llvm_config)"
    [[ -n "${config}" ]] || { echo "echo 'llvm-config not found' >&2"; exit 0; }
    bindir="$("${config}" --bindir)"
    echo "export PS5_PAYLOAD_SDK='${SDK}'"
    echo "export LLVM_CONFIG='${config}'"
    echo "export PS5_LLD='${bindir}/ld.lld'"
    echo "export PATH='${bindir}:${SDK}/bin:'\"\$PATH\""
    exit 0
fi

config="$(llvm_config)"
[[ -n "${config}" ]] || { echo "missing: llvm-config (sudo apt install clang lld llvm)" >&2; exit 2; }
bindir="$("${config}" --bindir)"
for t in clang clang++ ld.lld llvm-nm llvm-ar; do
    [[ -x "${bindir}/${t}" ]] || { echo "missing: ${bindir}/${t}" >&2; exit 2; }
done
for t in curl unzip tar python3 sha256sum; do
    command -v "${t}" >/dev/null || { echo "missing: ${t}" >&2; exit 2; }
done
echo "==> LLVM $("${config}" --version) at ${bindir}"

# Downloads once; a file whose checksum does not match is never used.
fetch() { # url sha name
    local out="${DL}/$3"
    mkdir -p "${DL}"
    if [[ ! -f "${out}" ]]; then
        echo "==> downloading $3"
        curl -fSL -o "${out}.part" "$1"
        mv "${out}.part" "${out}"
    fi
    local got
    got="$(sha256sum "${out}" | cut -d' ' -f1)"
    if [[ "${got}" != "$2" ]]; then
        echo "checksum of $3: ${got} (expected $2)" >&2
        echo "(the pinned value is the release's own digest, from GitHub)" >&2
        exit 1
    fi
}

fetch "${SDK_URL}" "${SDK_SHA}" "ps5-payload-sdk-${SDK_VER}.zip"
fetch "${PB_URL}" "${PB_SHA}" "pacbrew-${PB_VER}.tar.gz"

if [[ ! -x "${SDK}/bin/prospero-clang" ]]; then
    echo "==> extracting the SDK into ${SDK}"
    [[ ! -e "${SDK}" ]] || { echo "${SDK} exists but is incomplete: move it away first" >&2; exit 1; }
    tmp="$(mktemp -d)"
    unzip -q "${DL}/ps5-payload-sdk-${SDK_VER}.zip" -d "${tmp:?}"
    mv "${tmp:?}/ps5-payload-sdk" "${SDK}"
    rmdir "${tmp:?}"
fi

if [[ ! -f "${SDK}/target/user/homebrew/.pacbrew-${PB_VER}" ]]; then
    echo "==> merging the pacbrew sysroot ${PB_VER}"
    tmp="$(mktemp -d)"
    tar -xzf "${DL}/pacbrew-${PB_VER}.tar.gz" -C "${tmp:?}" "opt/ps5-payload-sdk/target/user/homebrew"
    mkdir -p "${SDK}/target/user/homebrew"
    cp -R "${tmp:?}/opt/ps5-payload-sdk/target/user/homebrew/." "${SDK}/target/user/homebrew/"
    touch "${SDK}/target/user/homebrew/.pacbrew-${PB_VER}"
    echo "    (temporary copy left in ${tmp}: remove it when convenient)"
fi

echo "==> toolchain ready at ${SDK}"
echo "    eval \"\$(scripts/setup-ps5-toolchain.sh --env)\""
