#!/usr/bin/env bash
# Regie5 — shows the console's log live: a development build sends it over UDP
# to this machine (port 5555). Each run writes a new file under build/ps5/logs/.
#   scripts/ps5-log.sh          (Ctrl-C to stop)
#   REGIE5_DRIVER_LOG=show scripts/ps5-log.sh   the driver's lines on screen too
#
# The OpenGL driver (ps5-opengl, built with its profiling on) prints many
# "[ps5-...]" lines it cannot be told to keep quiet: they go to a second file,
# console-<date>.driver.log, and stay off the screen and out of the main log,
# except those that warn of a slow path (software-blit, fallback, error, fail).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${REGIE5_LOG_PORT:-5555}"
mkdir -p "${ROOT}/build/ps5/logs"
FILE="${ROOT}/build/ps5/logs/console-$(date +%Y%m%d-%H%M%S).log"
DRIVER="${FILE%.log}.driver.log"
echo "listening for the Régie5 log on udp/${PORT}, copy in ${FILE#"${ROOT}/"}"
echo "(the driver's [ps5-...] lines: ${DRIVER#"${ROOT}/"}; Ctrl-C to stop)"
exec python3 -u - "${PORT}" "${FILE}" "${DRIVER}" "${REGIE5_DRIVER_LOG:-}" <<'PY'
import codecs, re, socket, sys
port, path, driver_path, show = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4] == "show"
worth_seeing = re.compile(r"software-blit|fallback|error|fail", re.I)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", port))
decode = codecs.getincrementaldecoder("utf-8")("replace")   # an "é" may straddle two datagrams
pending = ""   # a line not finished yet: a line may straddle two datagrams too
with open(path, "x", buffering=1) as f, open(driver_path, "x", buffering=1) as d:   # "x": never over a file
    while True:
        data, _ = s.recvfrom(65536)
        pending += decode.decode(data)
        *lines, pending = pending.split("\n")
        for line in lines:
            line += "\n"
            if line.startswith("[ps5-"):
                d.write(line)
                if not (show or worth_seeing.search(line)):
                    continue
            sys.stdout.write(line)
            f.write(line)
PY
