/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HTTP/1.1 over plain sockets (POSIX): the PS5's transport, and on Linux
 * with REGIE5_HTTP=socket to try it against real servers. One connection per
 * request, no compression. https:// goes through Mbed TLS when the build has
 * it (REGIE5_TLS), the certificate checked against the authorities given
 * (set_authorities) unless the service says "insecure"; without it, an
 * https:// address is answered "unsupported", which Réglages says plainly.
 * Few system functions are needed (socket, connect, poll, send, recv,
 * getaddrinfo for a name), where libcurl and OpenSSL would bring some eighty
 * more to the console.
 */
#pragma once

#include "api/http.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct sockaddr_storage;

namespace platform_posix {

class SocketTransport : public http::Transport {
public:
    /* log: one line per request (method, address without its query, status,
     * time), never a header: the keys travel there. Empty: no log. */
    /* resolve_names false: the system cannot look names up (a console
     * without getaddrinfo), only numeric addresses are tried. */
    explicit SocketTransport(std::function<void(const std::string &)> log = nullptr, bool resolve_names = true);
    ~SocketTransport() override;

    /* Whether this build speaks https:// (Mbed TLS compiled in, REGIE5_TLS). */
    static bool has_tls();
    /* The certificate authorities https:// is checked against (PEM, any
     * number). Returns how many were taken. Set before the first request;
     * with none, only an "insecure" service is reached over https. */
    int set_authorities(const std::string &pem);

    http::Response perform(const http::Request &req, const std::atomic<bool> *cancel) override;

    /* Told the servers' clock (their Date header, UTC seconds) with each
     * answer that has one: the PS5 checks its own clock against it. Set
     * before the first request. */
    void on_server_date(std::function<void(int64_t)> f) { m_on_date = std::move(f); }
    /* Times the system was asked to look a name up (the rest came from the kept ones). */
    int name_lookups() const { return m_lookups.load(); }

private:
    std::function<void(const std::string &)> m_log;
    bool m_resolve_names;
    std::function<void(int64_t)> m_on_date;
    struct Authorities;
    std::unique_ptr<Authorities> m_ca;
    /* Names looked up, kept a while (found or not). */
    struct Names;
    std::unique_ptr<Names> m_names;
    std::atomic<int> m_lookups{0};
    bool lookup(const std::string &host, int port, std::vector<sockaddr_storage> *out, std::string *error);
};

} // namespace platform_posix
