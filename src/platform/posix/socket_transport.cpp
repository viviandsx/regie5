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

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>
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

/* Looked-up names, kept: a poster no longer pays a lookup, and a name that
 * could not be found is not asked again at once (on a console without a
 * DNS server, each attempt may take several seconds). */
constexpr double kNameKept = 300, kNameFailureKept = 30;

} // namespace

struct SocketTransport::Names {
    struct Entry {
        std::vector<sockaddr_storage> addrs;
        std::string error;   /* empty: found */
        double until = 0;
    };
    std::mutex lock;
    std::map<std::string, Entry> by_name;   /* "host:port" */
};

namespace {

bool numeric(const std::string &host)
{
    unsigned char buf[sizeof(in6_addr)];
    return inet_pton(AF_INET, host.c_str(), buf) == 1 || inet_pton(AF_INET6, host.c_str(), buf) == 1;
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

/* A connected socket to one of the addresses, or -1 (and why). Each address
 * gets kConnectMs, never past the request's deadline. */
int open_connection(const std::vector<sockaddr_storage> &addrs, double deadline, const std::atomic<bool> *cancel,
                    http::Response::Fail *fail, std::string *error)
{
    *fail = http::Response::Connect;
    for (const sockaddr_storage &a : addrs) {
        if (platform::now() >= deadline) {
            *error = "connect: timed out";
            return -1;
        }
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
            const int w = wait_for(s.fd, POLLOUT, std::min(platform::now() + kConnectMs / 1000.0, deadline), cancel);
            if (w <= 0) {
                /* No connection in time: unreachable (nothing answered), not a slow server. */
                *fail = w == 0 ? http::Response::Connect : http::Response::Other;
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
    /* Another request on the same connection: its own deadline. */
    void set_deadline(double deadline, const std::atomic<bool> *cancel)
    {
        m_deadline = deadline;
        m_cancel = cancel;
    }
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
            /* At every step, not only while waiting for the server: the
             * handshake's own work (keys, certificates) counts too. */
            if (m_cancel && m_cancel->load())
                return failed(http::Response::Other, "cancelled");
            if (platform::now() >= m_deadline)
                return failed(http::Response::Timeout, "TLS handshake: timed out");
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

/* A connection, and how it is spoken (plain or TLS). The channel goes
 * first: TLS says goodbye before the socket closes. */
struct Conn {
    Fd s;
    std::unique_ptr<Channel> ch;
    double idle_since = 0;
};

/* https connections kept for the next request to the same server: no new
 * TCP connection, no new TLS handshake (the costly part on the console). At
 * most kIdlePerServer each, kIdleKeep seconds; one the server closed
 * meanwhile is dropped before use. */
constexpr int kIdlePerServer = 2;
constexpr double kIdleKeep = 10.0;

} // namespace

struct SocketTransport::Idle {
    std::mutex lock;
    std::multimap<std::string, std::unique_ptr<Conn>> by_server;
};

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
    : m_log(std::move(log)), m_resolve_names(resolve_names), m_ca(std::make_unique<Authorities>()),
      m_names(std::make_unique<Names>()), m_idle(std::make_unique<Idle>())
{
}

bool SocketTransport::lookup(const std::string &host, int port, std::vector<sockaddr_storage> *out, std::string *error)
{
    if (numeric(host) || !m_resolve_names)
        return resolve(host, port, m_resolve_names, out, error);
    const std::string key = host + ":" + std::to_string(port);
    const double now = platform::now();
    {
        std::lock_guard<std::mutex> l(m_names->lock);
        auto it = m_names->by_name.find(key);
        if (it != m_names->by_name.end() && now < it->second.until) {
            *out = it->second.addrs;
            *error = it->second.error;
            return error->empty();
        }
    }
    m_lookups++;
    const bool ok = resolve(host, port, true, out, error);
    std::lock_guard<std::mutex> l(m_names->lock);
    m_names->by_name[key] = {ok ? *out : std::vector<sockaddr_storage>{}, ok ? std::string() : *error,
                             platform::now() + (ok ? kNameKept : kNameFailureKept)};
    return ok;
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
    bool reused = false;
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
            if (n > 0 && n < (int)sizeof tail && reused)
                std::snprintf(tail + n, sizeof tail - n, ": kept connection");
            else if (n > 0 && n < (int)sizeof tail && t_connected > 0)
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
    const double deadline = t0 + (req.timeout_s > 0 ? req.timeout_s : 10);
    /* Kept connections: https (a handshake saved), plain only when asked
     * (tests); reads only, an action (POST, DELETE) always goes on a new
     * connection. A kept one the server dropped meanwhile fails before the
     * first byte of the answer: the read is then sent once more on a new one. */
    const bool keep = tls || m_keep_plain;
    const bool again_ok = req.method == "GET" || req.method == "HEAD";
    const std::string server = u.scheme + "://" + u.host + ":" + std::to_string(u.port) + (req.insecure ? " insecure" : "");

    for (int attempt = 0;; attempt++) {
        std::unique_ptr<Conn> c;
        reused = false;
        if (keep && again_ok && attempt == 0) {   /* an action always on a new connection */
            std::lock_guard<std::mutex> l(m_idle->lock);
            const double now = platform::now();
            for (auto it = m_idle->by_server.find(server); it != m_idle->by_server.end() && it->first == server;) {
                std::unique_ptr<Conn> k = std::move(it->second);
                it = m_idle->by_server.erase(it);
                pollfd p{k->s.fd, POLLIN, 0};
                if (now - k->idle_since > kIdleKeep || poll(&p, 1, 0) != 0)
                    continue;   /* too old, or the server closed it (or said something unasked) */
                c = std::move(k);
                break;
            }
        }
        reused = c != nullptr;
        if (!c) {
            std::vector<sockaddr_storage> addrs;
            std::string error;
            if (!lookup(u.host, u.port, &addrs, &error))
                return done(http::Response::Connect, error);
            http::Response::Fail fail = http::Response::Connect;
            c = std::make_unique<Conn>();
            c->s.fd = open_connection(addrs, deadline, cancel, &fail, &error);
            if (c->s.fd < 0)
                return done(fail, error);
            m_opened++;
            if (tls)
                t_connected = platform::now();
#ifdef REGIE5_TLS
            if (tls) {
                if (!req.insecure && m_ca->count == 0)
                    return done(http::Response::Certificate, "no certificate authorities to check against");
                auto t = std::make_unique<Tls>(c->s.fd, deadline, cancel);
                const Io opened = t->open(u.host, &m_ca->chain, req.insecure);
                if (opened.fail != http::Response::None)
                    return done(opened.fail, opened.error);
                t_secured = platform::now();
                c->ch = std::move(t);
            }
#endif
            if (!c->ch)
                c->ch = std::make_unique<Plain>(c->s.fd, deadline, cancel);
        }
        c->ch->set_deadline(deadline, cancel);
        Channel &ch = *c->ch;

        const std::string text = http::wire::request_text(req, u, keep);
        size_t sent = 0;
        Io failure;
        while (sent < text.size() && failure.fail == http::Response::None) {
            const Io w = ch.write(text.data() + sent, text.size() - sent);
            if (w.fail != http::Response::None)
                failure = w;
            else
                sent += (size_t)w.n;
        }
        http::wire::Reader reader(req.method == "HEAD", req.max_body);
        size_t got_bytes = 0;
        bool closed_early = false;
        if (failure.fail == http::Response::None) {
            std::vector<char> buf(16384);   /* on the heap: worker stacks are small on the PS5 */
            for (;;) {
                const Io got = ch.read(buf.data(), buf.size());
                if (got.fail != http::Response::None) {
                    failure = got;
                    break;
                }
                closed_early = got.n == 0 && got_bytes == 0;
                got_bytes += (size_t)got.n;
                const http::wire::Reader::State st =
                    got.n > 0 ? reader.feed(buf.data(), (size_t)got.n) : reader.finish();
                if (st == http::wire::Reader::Bad) {
                    failure.fail = reader.too_large ? http::Response::TooLarge : http::Response::Other;
                    failure.error = reader.error;
                    break;
                }
                if (st == http::wire::Reader::Done)
                    break;
            }
        }
        if (failure.fail != http::Response::None || closed_early) {
            /* A kept connection the server dropped: nothing of the answer came. */
            const bool dropped = reused && got_bytes == 0 && failure.fail != http::Response::Timeout;
            if (dropped && again_ok && attempt == 0 && platform::now() < deadline)
                continue;
            if (failure.fail == http::Response::None)
                failure = failed(http::Response::Other, "connection closed without an answer");
            return done(failure.fail, failure.error);
        }
        if (keep && reader.reusable()) {   /* kept for the next request to this server */
            c->idle_since = platform::now();
            std::lock_guard<std::mutex> l(m_idle->lock);
            if ((int)m_idle->by_server.count(server) < kIdlePerServer)
                m_idle->by_server.emplace(server, std::move(c));
        }
        c.reset();   /* or closed: TLS says goodbye before the socket closes */
        if (m_on_date && reader.server_date > 0)
            m_on_date(reader.server_date);
        r.status = reader.status;
        r.content_type = reader.content_type;
        r.body = std::move(reader.body);
        return done(http::Response::None, "");
    }
}

} // namespace platform_posix
