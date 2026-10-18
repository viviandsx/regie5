/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The PS5's HTTP transport (sockets, Mbed TLS) against servers on the
 * loopback that misbehave: silent, trickling, cut short, closed. Every
 * request ends within its delay (plus a little), whatever the server does.
 */
#include "check.h"

#include "api/services.h"
#include "platform/platform.h"
#include "platform/posix/socket_transport.h"

#include <arpa/inet.h>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
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

namespace {

/* Requests held until released, counted by server. */
class Gate : public http::Transport {
public:
    http::Response perform(const http::Request &req, const std::atomic<bool> *cancel) override
    {
        const std::string server = http::Pool::server_of(req);
        {
            std::lock_guard<std::mutex> l(m);
            calls[server]++;
            const int now_running = ++running[server];
            most[server] = std::max(most[server], now_running);
        }
        http::Response r;
        if (fail_connect.count(server)) {
            r.fail = http::Response::Connect;
        } else {
            std::unique_lock<std::mutex> l(m);
            cv.wait(l, [&] { return open || (cancel && *cancel); });
            r.status = 200;
        }
        std::lock_guard<std::mutex> l(m);
        running[server]--;
        return r;
    }
    void release()
    {
        {
            std::lock_guard<std::mutex> l(m);
            open = true;
        }
        cv.notify_all();
    }
    int get(std::map<std::string, int> &v, const std::string &k)
    {
        std::lock_guard<std::mutex> l(m);
        return v[k];
    }
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    std::map<std::string, int> calls, running, most;
    std::set<std::string> fail_connect;
};

void drain(http::Pool &pool)
{
    for (int i = 0; i < 400 && (pool.busy() > 0 || pool.pump() > 0); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    pool.pump();
}

http::Request to(const std::string &server, const char *path = "/x")
{
    http::Request r;
    r.url = server + path;
    r.server = server;
    return r;
}

} // namespace

TEST(each_server_waits_its_turn)
{
    Gate gate;
    http::Pool pool(gate, 4, nullptr);
    int done_b = 0;
    for (int i = 0; i < 5; i++)
        pool.submit(to("http://slow:1"), nullptr);
    pool.submit(to("http://quick:2"), [&done_b](http::Response &) -> std::function<void()> {
        return [&done_b] { done_b++; };
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    /* The slow server holds two workers, never four; the other server was reached meanwhile. */
    CHECK_EQ(gate.get(gate.running, "http://slow:1"), 2);
    CHECK_EQ(gate.get(gate.calls, "http://quick:2"), 1);
    const std::map<std::string, double> waiting = pool.waiting();
    CHECK(waiting.count("http://slow:1") && waiting.at("http://slow:1") >= 0.15);
    gate.release();
    drain(pool);
    CHECK_EQ(gate.get(gate.most, "http://slow:1"), 2);
    CHECK_EQ(gate.get(gate.calls, "http://slow:1"), 5);
    CHECK_EQ(done_b, 1);
    CHECK(pool.waiting().empty());
    /* The server's own address when Request::server is empty. */
    http::Request plain;
    plain.url = "https://media.example:8920/jellyfin/Items?x=1";
    CHECK_EQ(http::Pool::server_of(plain), std::string("https://media.example:8920"));
}

TEST(an_unreachable_server_is_set_aside)
{
    Gate gate;
    gate.open = true;
    gate.fail_connect.insert("http://down:1");
    http::Pool pool(gate, 2, nullptr);
    std::vector<http::Response::Fail> got;
    auto keep = [&got](http::Response &r) -> std::function<void()> {
        const http::Response::Fail f = r.fail;
        return [&got, f] { got.push_back(f); };
    };
    pool.submit(to("http://down:1"), keep);
    drain(pool);
    /* Asked again at once: answered "unreachable" without trying; the others go on. */
    for (int i = 0; i < 3; i++)
        pool.submit(to("http://down:1"), keep);
    pool.submit(to("http://up:2"), keep);
    drain(pool);
    CHECK_EQ(gate.get(gate.calls, "http://down:1"), 1);
    CHECK_EQ(gate.get(gate.calls, "http://up:2"), 1);
    CHECK_EQ(got.size(), (size_t)5);
    CHECK(got[0] == http::Response::Connect && got[1] == http::Response::Connect);
    /* A connection test still goes. */
    http::Request test = to("http://down:1");
    test.fresh = true;
    pool.submit(test, keep);
    drain(pool);
    CHECK_EQ(gate.get(gate.calls, "http://down:1"), 2);
}

TEST(services_name_the_slow_one)
{
    Gate gate;
    http::Pool pool(gate, 4, nullptr);
    config::Config cfg = config::mock();
    api::Services s(cfg, pool, "fr");
    s.queue(config::Sonarr, nullptr);
    CHECK(!s.idle());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    double longest = 0;
    const std::vector<std::string> slow = s.slow(0.2, &longest);
    CHECK(slow.size() == 1 && slow[0] == cfg.at(config::Sonarr).name);
    CHECK(longest >= 0.2);
    CHECK(s.slow(5).empty());
    gate.release();
    drain(pool);
    CHECK(s.idle());
}

TEST(names_are_looked_up_once_in_a_while)
{
    platform_posix::SocketTransport t;
    http::Request req;
    req.timeout_s = 1;
    /* A port nothing listens on: refused, but the name was looked up. */
    int port = 0;
    {
        Server s(silent);
        port = std::stoi(s.url().substr(s.url().rfind(':') + 1));
    }
    req.url = "http://localhost:" + std::to_string(port) + "/x";
    t.perform(req, nullptr);
    t.perform(req, nullptr);
    CHECK_EQ(t.name_lookups(), 1);
    /* A name that is not found: not asked again at once either. */
    req.url = "http://regie5-no-such-host.invalid/x";
    http::Response a = t.perform(req, nullptr);
    http::Response b = t.perform(req, nullptr);
    CHECK(a.fail == http::Response::Connect && b.fail == http::Response::Connect);
    CHECK_EQ(t.name_lookups(), 2);
    /* Numeric addresses are never looked up. */
    req.url = "http://127.0.0.1:" + std::to_string(port) + "/x";
    t.perform(req, nullptr);
    CHECK_EQ(t.name_lookups(), 2);
}
