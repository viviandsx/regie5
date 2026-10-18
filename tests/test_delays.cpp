/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The PS5's HTTP transport (sockets, Mbed TLS) against servers on the
 * loopback that misbehave: silent, trickling, cut short, closed. Every
 * request ends within its delay (plus a little), whatever the server does.
 */
#include "check.h"

#include "platform/platform.h"
#include "platform/posix/socket_transport.h"

#include <arpa/inet.h>
#include <atomic>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

/* A one-connection server on 127.0.0.1: behave(fd) runs on the accepted
 * socket in a thread of its own; the socket closes when it returns or the
 * server goes. */
class Server {
public:
    explicit Server(std::function<void(int fd, const std::atomic<bool> &stop)> behave)
    {
        m_listen = socket(AF_INET, SOCK_STREAM, 0);
        const int one = 1;
        setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(m_listen, reinterpret_cast<sockaddr *>(&a), sizeof a);
        socklen_t len = sizeof a;
        getsockname(m_listen, reinterpret_cast<sockaddr *>(&a), &len);
        m_port = ntohs(a.sin_port);
        listen(m_listen, 4);
        m_thread = std::thread([this, behave] {
            const int fd = accept(m_listen, nullptr, nullptr);
            if (fd < 0)
                return;
            behave(fd, m_stop);
            close(fd);
        });
    }
    ~Server()
    {
        m_stop = true;
        shutdown(m_listen, SHUT_RDWR);
        close(m_listen);
        m_thread.join();
    }
    std::string url(const char *scheme = "http") const
    {
        return std::string(scheme) + "://127.0.0.1:" + std::to_string(m_port) + "/x";
    }

private:
    int m_listen = -1, m_port = 0;
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

void wait_while(const std::atomic<bool> &stop, double seconds)
{
    for (double t0 = platform::now(); !stop && platform::now() - t0 < seconds;)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

/* Reads what the client sends, then says nothing at all. */
void silent(int fd, const std::atomic<bool> &stop) { wait_while(stop, 30); }

void send_text(int fd, const std::string &s) { (void)!send(fd, s.data(), s.size(), MSG_NOSIGNAL); }

struct Timed {
    http::Response r;
    double seconds = 0;
};

Timed get(const std::string &url, int timeout_s, const std::atomic<bool> *cancel = nullptr)
{
    platform_posix::SocketTransport t;
    http::Request req;
    req.url = url;
    req.timeout_s = timeout_s;
    req.insecure = true;
    const double t0 = platform::now();
    Timed out;
    out.r = t.perform(req, cancel);
    out.seconds = platform::now() - t0;
    return out;
}

} // namespace

TEST(http_delays_hold_on_the_plain_socket)
{
    /* A closed port: refused at once. */
    int port = 0;
    {
        Server s(silent);
        port = std::stoi(s.url().substr(s.url().rfind(':') + 1));
    }
    Timed refused = get("http://127.0.0.1:" + std::to_string(port) + "/x", 2);
    CHECK(refused.r.fail == http::Response::Connect);
    CHECK(refused.seconds < 1.0);

    /* Connected, never an answer. */
    {
        Server s(silent);
        Timed t = get(s.url(), 1);
        CHECK(t.r.fail == http::Response::Timeout);
        CHECK(t.seconds >= 0.9 && t.seconds < 1.6);
    }
    /* The headers, then a body a byte at a time, forever: the delay counts from the start. */
    {
        Server s([](int fd, const std::atomic<bool> &stop) {
            send_text(fd, "HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\n");
            while (!stop) {
                send_text(fd, "x");
                wait_while(stop, 0.1);
            }
        });
        Timed t = get(s.url(), 1);
        CHECK(t.r.fail == http::Response::Timeout);
        CHECK(t.seconds >= 0.9 && t.seconds < 1.6);
    }
    /* Cut short: refused at once, not taken for a whole answer. */
    {
        Server s([](int fd, const std::atomic<bool> &) {
            send_text(fd, "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n[1,2,3]");
        });
        Timed t = get(s.url(), 5);
        CHECK(t.r.fail == http::Response::Other);
        CHECK(t.r.body.empty());
        CHECK(t.seconds < 1.0);
    }
    /* Closing the app: a long wait gives up within a slice. */
    {
        Server s(silent);
        std::atomic<bool> cancel{false};
        std::thread later([&cancel] {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            cancel = true;
        });
        Timed t = get(s.url(), 10, &cancel);
        later.join();
        CHECK(t.r.fail != http::Response::None);
        CHECK(t.seconds < 1.0);
    }
}

TEST(http_delays_hold_during_the_tls_handshake)
{
    if (!platform_posix::SocketTransport::has_tls())
        return;
    /* A server that takes the connection and never speaks TLS. */
    {
        Server s(silent);
        Timed t = get(s.url("https"), 1);
        CHECK(t.r.fail == http::Response::Timeout);
        CHECK(t.seconds >= 0.9 && t.seconds < 1.6);
    }
    /* One that answers the hello a byte at a time, slowly: never past the delay. */
    {
        Server s([](int fd, const std::atomic<bool> &stop) {
            const unsigned char hello[] = {0x16, 0x03, 0x03, 0x40, 0x00, 0x02, 0x00, 0x3f, 0xfc, 0x03, 0x03};
            for (unsigned char b : hello) {
                if (stop)
                    return;
                (void)!send(fd, &b, 1, MSG_NOSIGNAL);
                wait_while(stop, 0.3);
            }
            wait_while(stop, 30);
        });
        Timed t = get(s.url("https"), 1);
        CHECK(t.r.fail != http::Response::None);
        CHECK(t.seconds < 1.6);
    }
    /* Plain HTTP where TLS was expected: refused at once. */
    {
        Server s([](int fd, const std::atomic<bool> &) { send_text(fd, "HTTP/1.1 400 Bad Request\r\n\r\n"); });
        Timed t = get(s.url("https"), 5);
        CHECK(t.r.fail == http::Response::Other);
        CHECK(t.seconds < 1.0);
    }
}
