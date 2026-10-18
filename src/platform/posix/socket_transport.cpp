/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/posix/socket_transport.h"

#include "api/http_wire.h"
#include "platform/platform.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#ifdef REGIE5_TLS
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0   /* FreeBSD / PS5: SO_NOSIGPIPE on the socket instead */
#endif

namespace platform_posix {

namespace {

constexpr int kConnectMs = 5000;
constexpr int kSliceMs = 200;   /* how often a wait looks at the cancel flag */

struct Fd {
    int fd = -1;
    ~Fd()
    {
        if (fd >= 0)
            close(fd);
    }
};

/* The addresses to try: a literal needs no lookup (and no resolver). */
bool resolve(const std::string &host, int port, bool names, std::vector<sockaddr_storage> *out, std::string *error)
{
    sockaddr_storage ss{};
    auto *v4 = reinterpret_cast<sockaddr_in *>(&ss);
    if (inet_pton(AF_INET, host.c_str(), &v4->sin_addr) == 1) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons((uint16_t)port);
        out->push_back(ss);
        return true;
    }
    ss = {};
    auto *v6 = reinterpret_cast<sockaddr_in6 *>(&ss);
    if (inet_pton(AF_INET6, host.c_str(), &v6->sin6_addr) == 1) {
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons((uint16_t)port);
        out->push_back(ss);
        return true;
    }
    if (!names) {
        *error = "host names cannot be looked up here: use the server's numeric address";
        return false;
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *res = nullptr;
    const std::string service = std::to_string(port);
    const int rc = getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        *error = "name not found: " + host;
        return false;
    }
    for (addrinfo *a = res; a; a = a->ai_next)
        if ((a->ai_family == AF_INET || a->ai_family == AF_INET6) && a->ai_addrlen <= sizeof(sockaddr_storage)) {
            sockaddr_storage s{};
            std::memcpy(&s, a->ai_addr, a->ai_addrlen);
            out->push_back(s);
        }
    freeaddrinfo(res);
    if (out->empty())
        *error = "no address for " + host;
    return !out->empty();
}

/* Waits for fd to be ready for ev, in slices; 1 ready, 0 time is up, -1 cancelled or failed. */
int wait_for(int fd, short ev, double deadline, const std::atomic<bool> *cancel)
{
    for (;;) {
        if (cancel && cancel->load())
            return -1;
        const double left = deadline - platform::now();
        if (left <= 0)
            return 0;
        pollfd p{fd, ev, 0};
        const int ms = (int)(left * 1000) < kSliceMs ? (int)(left * 1000) + 1 : kSliceMs;
        const int rc = poll(&p, 1, ms);
        if (rc > 0)
            return 1;
        if (rc < 0 && errno != EINTR)
            return -1;
    }
}

/* A connected socket to one of the addresses, or -1 (and why). */
int open_connection(const std::vector<sockaddr_storage> &addrs, const std::atomic<bool> *cancel,
                    http::Response::Fail *fail, std::string *error)
{
    *fail = http::Response::Connect;
    for (const sockaddr_storage &a : addrs) {
        Fd s;
        s.fd = socket(a.ss_family, SOCK_STREAM, 0);
        if (s.fd < 0) {
            *error = std::string("socket: ") + std::strerror(errno);
            continue;
        }
#ifdef SO_NOSIGPIPE
        const int one = 1;
        setsockopt(s.fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        const int flags = fcntl(s.fd, F_GETFL, 0);
        fcntl(s.fd, F_SETFL, flags | O_NONBLOCK);
        const socklen_t len = a.ss_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
        if (connect(s.fd, reinterpret_cast<const sockaddr *>(&a), len) != 0) {
            if (errno != EINPROGRESS && errno != EINTR) {
                *error = std::string("connect: ") + std::strerror(errno);
                continue;
            }
            const int w = wait_for(s.fd, POLLOUT, platform::now() + kConnectMs / 1000.0, cancel);
            if (w <= 0) {
                *fail = w == 0 ? http::Response::Timeout : http::Response::Other;
                *error = w == 0 ? "connect: timed out" : "cancelled";
                if (w < 0)
                    return -1;
                continue;
            }
            int err = 0;
            socklen_t el = sizeof err;
            getsockopt(s.fd, SOL_SOCKET, SO_ERROR, &err, &el);
            if (err != 0) {
                *fail = http::Response::Connect;
                *error = std::string("connect: ") + std::strerror(err);
                continue;
            }
        }
        /* A TLS record or a request goes out at once: without this, a small
         * write waits for the peer's delayed acknowledgement (40 ms and more). */
        const int nodelay = 1;
        setsockopt(s.fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof nodelay);
        const int fd = s.fd;
        s.fd = -1;
        return fd;
    }
    return -1;
}

/* ---- the bytes on the wire: plain, or through TLS ---------------------------- */

/* What a read or a write came to. */
struct Io {
    long n = 0;   /* bytes, when > 0; 0 with fail None: the peer closed */
    http::Response::Fail fail = http::Response::None;
    std::string error;
};

Io bytes(long n)
{
    Io r;
    r.n = n;
    return r;
}

Io failed(http::Response::Fail fail, std::string error)
{
    Io r;
    r.fail = fail;
    r.error = std::move(error);
    return r;
}

Io io_wait_failed(int w, const char *what)
{
    Io r;
    r.fail = w == 0 ? http::Response::Timeout : http::Response::Other;
    r.error = w == 0 ? std::string(what) + ": timed out" : "cancelled";
    return r;
}

class Channel {
public:
    Channel(int fd, double deadline, const std::atomic<bool> *cancel) : m_fd(fd), m_deadline(deadline), m_cancel(cancel) {}
    virtual ~Channel() = default;
    /* Waits as needed until the deadline. */
    virtual Io write(const char *p, size_t n) = 0;
    virtual Io read(char *p, size_t n) = 0;

protected:
    int m_fd;
    double m_deadline;
    const std::atomic<bool> *m_cancel;
};

class Plain : public Channel {
public:
    using Channel::Channel;
    Io write(const char *p, size_t n) override
    {
        for (;;) {
            const ssize_t k = send(m_fd, p, n, MSG_NOSIGNAL);
            if (k >= 0)
                return bytes((long)k);
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                return failed(http::Response::Other, std::string("send: ") + std::strerror(errno));
            const int w = wait_for(m_fd, POLLOUT, m_deadline, m_cancel);
            if (w <= 0)
                return io_wait_failed(w, "send");
        }
    }
    Io read(char *p, size_t n) override
    {
        for (;;) {
            const ssize_t k = recv(m_fd, p, n, 0);
            if (k >= 0)
                return bytes((long)k);
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                return failed(http::Response::Other, std::string("recv: ") + std::strerror(errno));
            const int w = wait_for(m_fd, POLLIN, m_deadline, m_cancel);
            if (w <= 0)
                return io_wait_failed(w, "answer");
        }
    }
};

#ifdef REGIE5_TLS

/* The random generator: one for the process, behind a lock (Mbed TLS is
 * built without its own), made once and never freed (no thread-exit or
 * process-exit hooks needed: the PS5 runtime has none for threads). */
struct Rng {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    bool ok = false;
    Rng()
    {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        static const unsigned char kWho[] = "regie5";
        ok = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, kWho, sizeof kWho - 1) == 0;
    }
};

std::mutex s_rng_lock;

Rng &rng()
{
    static Rng *r = new Rng;
    return *r;
}

int locked_random(void *, unsigned char *out, size_t n)
{
    std::lock_guard<std::mutex> lock(s_rng_lock);
    return mbedtls_ctr_drbg_random(&rng().drbg, out, n);
}

std::string tls_text(int rc)
{
    char t[160];
    mbedtls_strerror(rc, t, sizeof t);
    return t;
}

int bio_send(void *ctx, const unsigned char *p, size_t n)
{
    const int fd = *static_cast<int *>(ctx);
    const ssize_t k = send(fd, p, n, MSG_NOSIGNAL);
    if (k >= 0)
        return (int)k;
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? MBEDTLS_ERR_SSL_WANT_WRITE
                                                                     : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int bio_recv(void *ctx, unsigned char *p, size_t n)
{
    const int fd = *static_cast<int *>(ctx);
    const ssize_t k = recv(fd, p, n, 0);
    if (k >= 0)
        return (int)k;
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? MBEDTLS_ERR_SSL_WANT_READ
                                                                     : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

class Tls : public Channel {
public:
    Tls(int fd, double deadline, const std::atomic<bool> *cancel) : Channel(fd, deadline, cancel)
    {
        mbedtls_ssl_init(&m_ssl);
        mbedtls_ssl_config_init(&m_conf);
    }
    ~Tls() override
    {
        if (m_open)
            mbedtls_ssl_close_notify(&m_ssl);
        mbedtls_ssl_free(&m_ssl);
        mbedtls_ssl_config_free(&m_conf);
    }

    /* The handshake, the certificate checked against ca (none: refused)
     * unless insecure. */
    Io open(const std::string &host, mbedtls_x509_crt *ca, bool insecure)
    {
        {
            std::lock_guard<std::mutex> lock(s_rng_lock);
            if (!rng().ok)
                return failed(http::Response::Other, "TLS: no random source");
        }
        mbedtls_ssl_config_defaults(&m_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(&m_conf, locked_random, nullptr);
        if (insecure) {
            mbedtls_ssl_conf_authmode(&m_conf, MBEDTLS_SSL_VERIFY_NONE);
        } else {
            mbedtls_ssl_conf_authmode(&m_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
            mbedtls_ssl_conf_ca_chain(&m_conf, ca, nullptr);
        }
        int rc = mbedtls_ssl_setup(&m_ssl, &m_conf);
        if (rc == 0)
            rc = mbedtls_ssl_set_hostname(&m_ssl, host.c_str());
        if (rc != 0)
            return failed(http::Response::Other, "TLS: " + tls_text(rc));
        mbedtls_ssl_set_bio(&m_ssl, &m_fd, bio_send, bio_recv, nullptr);
        for (;;) {
            rc = mbedtls_ssl_handshake(&m_ssl);
            if (rc == 0)
                break;
            if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
                const int w = wait_for(m_fd, rc == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, m_deadline, m_cancel);
                if (w <= 0)
                    return io_wait_failed(w, "TLS handshake");
                continue;
            }
            const uint32_t flags = mbedtls_ssl_get_verify_result(&m_ssl);
            if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED || (flags != 0 && flags != (uint32_t)-1)) {
                char why[256];
                mbedtls_x509_crt_verify_info(why, sizeof why, "", flags);
                for (char &c : why)
                    if (c == '\n')
                        c = ' ';
                return failed(http::Response::Certificate, std::string("certificate refused: ") + why);
            }
            return failed(http::Response::Other, "TLS: " + tls_text(rc));
        }
        m_open = true;
        return bytes(1);
    }

    Io write(const char *p, size_t n) override
    {
        for (;;) {
            const int rc = mbedtls_ssl_write(&m_ssl, reinterpret_cast<const unsigned char *>(p), n);
            if (rc >= 0)
                return bytes(rc);
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
                return failed(http::Response::Other, "TLS: " + tls_text(rc));
            const int w = wait_for(m_fd, rc == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, m_deadline, m_cancel);
            if (w <= 0)
                return io_wait_failed(w, "send");
        }
    }

    Io read(char *p, size_t n) override
    {
        for (;;) {
            const int rc = mbedtls_ssl_read(&m_ssl, reinterpret_cast<unsigned char *>(p), n);
            if (rc >= 0)
                return bytes(rc);
            if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                return bytes(0);
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
                return failed(http::Response::Other, "TLS: " + tls_text(rc));
            const int w = wait_for(m_fd, rc == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, m_deadline, m_cancel);
            if (w <= 0)
                return io_wait_failed(w, "answer");
        }
    }

private:
    mbedtls_ssl_context m_ssl;
    mbedtls_ssl_config m_conf;
    bool m_open = false;
};

#endif

} // namespace

#ifdef REGIE5_TLS
struct SocketTransport::Authorities {
    mbedtls_x509_crt chain;
    int count = 0;
    Authorities() { mbedtls_x509_crt_init(&chain); }
    ~Authorities() { mbedtls_x509_crt_free(&chain); }
};
#else
struct SocketTransport::Authorities {
    int count = 0;
};
#endif

SocketTransport::SocketTransport(std::function<void(const std::string &)> log, bool resolve_names)
    : m_log(std::move(log)), m_resolve_names(resolve_names), m_ca(std::make_unique<Authorities>())
{
}

SocketTransport::~SocketTransport() = default;

int SocketTransport::set_authorities(const std::string &pem)
{
#ifdef REGIE5_TLS
    if (pem.empty())
        return 0;
    /* PEM parsing wants the closing NUL counted. */
    const int bad = mbedtls_x509_crt_parse(&m_ca->chain, reinterpret_cast<const unsigned char *>(pem.c_str()),
                                           pem.size() + 1);
    int n = 0;
    for (const mbedtls_x509_crt *c = &m_ca->chain; c && c->raw.len; c = c->next)
        n++;
    m_ca->count = n;
    (void)bad;   /* > 0: some certificates were skipped; the others stand */
    return n;
#else
    (void)pem;
    return 0;
#endif
}

bool SocketTransport::has_tls()
{
#ifdef REGIE5_TLS
    return true;
#else
    return false;
#endif
}

http::Response SocketTransport::perform(const http::Request &req, const std::atomic<bool> *cancel)
{
    const double t0 = platform::now();
    double t_connected = 0, t_secured = 0;   /* https: where the time went, for the log */
    http::Response r;
    auto done = [&](http::Response::Fail fail, const std::string &error) {
        r.fail = fail;
        r.error = error;
        if (fail != http::Response::None) {
            r.status = 0;
            r.body.clear();
        }
        if (m_log) {
            /* The address only: the key travels in a header and is never printed. */
            const std::string shown = req.url.substr(0, req.url.find('?'));
            char tail[260];
            const int n = std::snprintf(tail, sizeof tail, " -> %d%s%s (%.0f ms%s%s", r.status, r.error.empty() ? "" : " ",
                                        r.error.c_str(), (platform::now() - t0) * 1000.0, r.body.empty() ? "" : ", ",
                                        r.body.empty() ? "" : http::size_text(r.body.size()).c_str());
            if (n > 0 && n < (int)sizeof tail && t_connected > 0)
                std::snprintf(tail + n, sizeof tail - n, ": connected %.0f, TLS %.0f", (t_connected - t0) * 1000.0,
                              t_secured > 0 ? (t_secured - t_connected) * 1000.0 : -1.0);
            const size_t used = std::strlen(tail);
            std::snprintf(tail + used, sizeof tail - used, ")");
            m_log("http: " + req.method + " " + shown + tail);
        }
        return r;
    };

    http::wire::Url u;
    if (!http::wire::parse_url(req.url, &u))
        return done(http::Response::Connect, "bad address");
    const bool tls = u.scheme == "https";
    if (tls && !has_tls())
        return done(http::Response::Unsupported, "https is not supported here");

    std::vector<sockaddr_storage> addrs;
    std::string error;
    if (!resolve(u.host, u.port, m_resolve_names, &addrs, &error))
        return done(http::Response::Connect, error);
    http::Response::Fail fail = http::Response::Connect;
    Fd s;
    s.fd = open_connection(addrs, cancel, &fail, &error);
    if (s.fd < 0)
        return done(fail, error);

    if (tls)
        t_connected = platform::now();
    const double deadline = t0 + (req.timeout_s > 0 ? req.timeout_s : 10);
    std::unique_ptr<Channel> ch;
#ifdef REGIE5_TLS
    if (tls) {
        if (!req.insecure && m_ca->count == 0)
            return done(http::Response::Certificate, "no certificate authorities to check against");
        auto t = std::make_unique<Tls>(s.fd, deadline, cancel);
        const Io opened = t->open(u.host, &m_ca->chain, req.insecure);
        if (opened.fail != http::Response::None)
            return done(opened.fail, opened.error);
        t_secured = platform::now();
        ch = std::move(t);
    }
#endif
    if (!ch)
        ch = std::make_unique<Plain>(s.fd, deadline, cancel);

    const std::string text = http::wire::request_text(req, u);
    size_t sent = 0;
    while (sent < text.size()) {
        const Io w = ch->write(text.data() + sent, text.size() - sent);
        if (w.fail != http::Response::None)
            return done(w.fail, w.error);
        sent += (size_t)w.n;
    }

    http::wire::Reader reader(req.method == "HEAD", req.max_body);
    std::vector<char> buf(16384);   /* on the heap: worker stacks are small on the PS5 */
    for (;;) {
        const Io got = ch->read(buf.data(), buf.size());
        if (got.fail != http::Response::None)
            return done(got.fail, got.error);
        const http::wire::Reader::State st = got.n > 0 ? reader.feed(buf.data(), (size_t)got.n) : reader.finish();
        if (st == http::wire::Reader::Bad)
            return done(reader.too_large ? http::Response::TooLarge : http::Response::Other, reader.error);
        if (st == http::wire::Reader::Done)
            break;
    }
    ch.reset();   /* TLS says goodbye before the socket closes */
    if (m_on_date && reader.server_date > 0)
        m_on_date(reader.server_date);
    r.status = reader.status;
    r.content_type = reader.content_type;
    r.body = std::move(reader.body);
    return done(http::Response::None, "");
}

} // namespace platform_posix
