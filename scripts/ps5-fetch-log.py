#!/usr/bin/env python3
# Regie5 — copies the app's log from the console over FTP, read only.
# SPDX-License-Identifier: GPL-3.0-or-later
"""
    scripts/ps5-fetch-log.py <console address> [port] [title id]

The title's own storage, where it writes app.log (this launch) and
app.prev.log (the launch before), can be read only while the title runs:
start it first (after a freeze, the probe starts in a safe mode that draws
nothing and stays up five minutes for exactly this). Default port 2121
(ftpsrv; etaHEN's is 1337), title PPSA99521 (the probe).

Nothing on the console is written or deleted. The copies land in
build/ps5/logs/, under new names.
"""
import ftplib
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return 2
    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2121
    title = sys.argv[3] if len(sys.argv) > 3 else "PPSA99521"
    folder = f"/mnt/sandbox/{title}_000/download0/regie5/dev"
    out_dir = os.path.join(ROOT, "build", "ps5", "logs")
    os.makedirs(out_dir, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    ftp = ftplib.FTP()
    ftp.connect(host, port, timeout=15)
    ftp.login()
    ftp.set_pasv(True)
    try:
        ftp.cwd(folder)
    except ftplib.error_perm:
        print(f"{folder} is not there: is the title running?")
        return 1
    got = 0
    for name in ("app.log", "app.prev.log"):
        local = os.path.join(out_dir, f"{stamp}-{name}")
        chunks = []
        try:
            ftp.retrbinary(f"RETR {name}", chunks.append)
        except ftplib.error_perm:
            print(f"{name}: not on the console")
            continue
        with open(local, "xb") as f:   # a new file, never over an existing one
            f.write(b"".join(chunks))
        print(f"{name} -> {os.path.relpath(local, ROOT)} ({os.path.getsize(local)} bytes)")
        got += 1
    ftp.quit()
    return 0 if got else 1


if __name__ == "__main__":
    sys.exit(main())
