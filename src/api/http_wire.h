/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HTTP/1.1 on the wire, without the network: an address taken apart, the
 * request's text, and a reader that takes the answer as it arrives (status,
 * headers, a body sized by Content-Length, chunked or ended by the close).
 * The socket transport (platform/posix) sends and receives; this part is
 * tested on its own (tests/test_api.cpp).
 */
#pragma once

#include "api/http.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace http::wire {

struct Url {
    std::string scheme;   /* "http" or "https", lower case */
    std::string host;     /* a name, an IPv4 address, or an IPv6 one without its brackets */
    int port = 0;         /* given, or the scheme's */
    std::string target;   /* path and query, "/" at least */
};

/* An HTTP date ("Mon, 12 Oct 2026 12:34:32 GMT", the only form servers
 * send today) as seconds since 1970 UTC; false for anything else. */
bool parse_http_date(const std::string &s, int64_t *utc);

/* http(s)://host[:port][/path][?query]; false for anything else. */
bool parse_url(const std::string &url, Url *out);

/* The request as sent: one per connection (Connection: close), no
 * compression asked, the body as JSON. */
std::string request_text(const Request &req, const Url &u);

/* The answer, fed as it comes. */
class Reader {
public:
    enum State { More, Done, Bad };
    /* max_body: bytes at most (0: 64 MiB); past it, Bad with too_large. */
    explicit Reader(bool head = false, size_t max_body = 0);

    State feed(const char *data, size_t n);
    /* The server closed the connection: the end of a body without a length,
     * or an answer cut short. */
    State finish();

    int status = 0;
    std::string content_type;
    std::string body;
    int64_t server_date = 0;   /* the Date header, UTC seconds; 0 if none */
    std::string error;   /* why it is Bad */
    bool too_large = false;   /* Bad because the body passed max_body */

private:
    enum Part { Head, Sized, Chunked, ToClose, Finished, Broken };
    State fail(const char *why);
    State too_big();
    State parse_head();
    State parse_chunks();

    bool m_head;
    size_t m_max;
    Part m_part = Head;
    std::string m_buf;     /* bytes not consumed yet (head, chunk sizes) */
    size_t m_left = 0;     /* body bytes still to come (Sized), or of the chunk (Chunked) */
    bool m_in_chunk = false, m_trailers = false;
};

} // namespace http::wire
