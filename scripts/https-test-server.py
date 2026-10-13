#!/usr/bin/env python3
# Regie5 — an HTTPS server on this machine, to try the console's HTTPS with a
# self-signed certificate (phase 6, step 6.2 bis). It answers like Glances
# (/api/4/quicklook, from fixtures/glances/quicklook.json), so Réglages can
# show it as a service: "Glances", address https://<this machine>:18443.
#   scripts/https-test-server.py          Ctrl-C to stop
# The certificate is made at start (openssl) into a new folder under
# build/https-test/ and is self-signed: on the console, "insecure" must be
# ticked for Glances, or Réglages says "Certificate refused" (which is right).
# SPDX-License-Identifier: GPL-3.0-or-later
import http.server
import os
import socket
import ssl
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = 18443


def local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.168.1.1", 9))   # picks the LAN's route, sends nothing
        return s.getsockname()[0]
    finally:
        s.close()


class Handler(http.server.BaseHTTPRequestHandler):
    body = b""

    def do_GET(self):
        ok = self.path.split("?")[0] == "/api/4/quicklook"
        data = Handler.body if ok else b'{"error":"not found"}'
        self.send_response(200 if ok else 404)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        print(f"{time.strftime('%H:%M:%S')} {self.client_address[0]} GET {self.path} -> {200 if ok else 404}", flush=True)

    def log_message(self, *args):
        pass


def main():
    ip = local_ip()
    folder = os.path.join(ROOT, "build", "https-test", time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(folder, exist_ok=False)
    cert, key = os.path.join(folder, "cert.pem"), os.path.join(folder, "key.pem")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                    "-keyout", key, "-out", cert, "-days", "7", "-subj", "/CN=regie5-test",
                    "-addext", f"subjectAltName=IP:{ip}"], check=True, capture_output=True)
    with open(os.path.join(ROOT, "fixtures", "glances", "quicklook.json"), "rb") as f:
        Handler.body = f.read()
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    print(f"HTTPS test server: https://{ip}:{PORT}  (self-signed; Ctrl-C to stop)", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
