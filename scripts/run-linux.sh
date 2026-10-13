#!/usr/bin/env bash
# Regie5 — builds the Linux desktop version and runs it.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/run-linux.sh                 build, then run
#   scripts/run-linux.sh --lang en       arguments go to regie5 (see regie5 --help)
#   REGIE5_BUILD_TYPE=Debug scripts/run-linux.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/linux"
TYPE="${REGIE5_BUILD_TYPE:-RelWithDebInfo}"

if [[ ! -f "${BUILD}/build.ninja" ]]; then
    CC_ARGS=()
    if command -v clang >/dev/null && command -v clang++ >/dev/null; then
        CC_ARGS=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
    fi
    cmake -S "${ROOT}" -B "${BUILD}" -G Ninja -DCMAKE_BUILD_TYPE="${TYPE}" "${CC_ARGS[@]}"
fi
cmake --build "${BUILD}"
exec "${BUILD}/regie5" "$@"
