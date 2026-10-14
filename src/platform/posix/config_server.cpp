/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The page a phone uses to fill config.json (platform.h, share_*), on plain
 * sockets, for Linux and the PS5 alike. It runs only while Réglages shows
 * its QR code, answers only the local network, and only with the six-digit
 * code shown on the TV (ten wrong codes stop it). One connection at a time,
 * on its own thread; the page and the JSON are made by core/config_web.
 */
#include "core/config_web.h"
#include "platform/platform.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <string>

/* Weak: a console that does not give titles these functions only loses the
 * button (can_share_config). app-6 found bind and listen there. */
#pragma weak bind
#pragma weak listen
#pragma weak accept
#pragma weak getsockname

namespace platform {

namespace {

constexpr int kFirstPort = 8085, kPorts = 6;
constexpr size_t kMaxHead = 16u << 10, kMaxBody = 64u << 10;
constexpr int kMaxWrongCodes = 10;

std::mutex s_lock;
ShareInfo s_info;                /* under s_lock */
std::string s_path;              /* config.json, under s_lock */
std::atomic<bool> s_stop{false};
int s_listen = -1;
pthread_t s_thread;
bool s_thread_on = false;
int s_wrong = 0;                 /* wrong codes since the start, under s_lock */

std::string ip_text(const sockaddr_in &a)
{
    const auto *b = reinterpret_cast<const unsigned char *>(&a.sin_addr);
    char t[20];
    std::snprintf(t, sizeof t, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return t;
}

/* The console's address on the network that reaches host (a UDP "connect"
 * sends nothing; it only picks the route). */
std::string local_ip(const std::string &host)
{
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return {};
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    if (inet_pton(AF_INET, host.c_str(), &to.sin_addr) != 1)
        inet_pton(AF_INET, "192.168.1.1", &to.sin_addr);   /* a name: any private address finds the LAN's route */
    std::string out;
    sockaddr_in me{};
    socklen_t len = sizeof me;
    if (connect(fd, (const sockaddr *)&to, sizeof to) == 0 && getsockname(fd, (sockaddr *)&me, &len) == 0)
        out = ip_text(me);
    close(fd);
    return out == "0.0.0.0" ? std::string() : out;
}

bool read_text(const std::string &path, std::string *out)
{
    out->clear();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out->append(buf, n);
    std::fclose(f);
    return true;
}

/* Beside, then renamed over: a reader never sees half a file. */
bool write_text(const std::string &path, const std::string &text)
{
    const std::string tmp = path + ".new";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok)
        return false;
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

void send_all(int fd, const std::string &s)
{
    size_t done = 0;
    while (done < s.size()) {
        const ssize_t n = send(fd, s.data() + done, s.size() - done, 0);
        if (n <= 0)
            return;
        done += (size_t)n;
    }
}

void reply(int fd, int status, const char *type, const std::string &body)
{
    const char *word = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 403 ? "Forbidden"
                                          : status == 404 ? "Not Found" : status == 413 ? "Payload Too Large"
                                                                                       : "Error";
    char head[320];
    std::snprintf(head, sizeof head,
                  "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                  "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n\r\n",
                  status, word, type, body.size());
    send_all(fd, head + body);
}

std::string json_error(const std::string &e)
{
    std::string out = "{\"ok\":false,\"error\":\"";
    for (char c : e)
        if (c == '"' || c == '\\')
            out += ' ';
        else if ((unsigned char)c >= 0x20)
            out += c;
    return out + "\"}";
}

/* The value of ?c= in a target, or empty. */
std::string code_of(const std::string &target)
{
    const size_t q = target.find('?');
    if (q == std::string::npos)
        return {};
    std::string query = "&" + target.substr(q + 1);
    const size_t at = query.find("&c=");
    if (at == std::string::npos)
        return {};
    const size_t end = query.find('&', at + 3);
    return query.substr(at + 3, end == std::string::npos ? std::string::npos : end - at - 3);
}

bool same(const std::string &a, const std::string &b)   /* in constant time */
{
    if (a.size() != b.size())
        return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); i++)
        d |= (unsigned char)(a[i] ^ b[i]);
    return d == 0;
}

/* Reads one request (head, then a body of Content-Length), 5 s at most. */
bool read_request(int fd, std::string *method, std::string *target, std::string *body)
{
    std::string in;
    char buf[2048];
    const double deadline = now() + 5;
    size_t head_end = std::string::npos, length = 0;
    for (;;) {
        if (head_end == std::string::npos) {
            head_end = in.find("\r\n\r\n");
            if (head_end != std::string::npos) {
                const size_t sp1 = in.find(' '), sp2 = in.find(' ', sp1 + 1);
                if (sp1 == std::string::npos || sp2 == std::string::npos || sp2 > head_end)
                    return false;
                *method = in.substr(0, sp1);
                *target = in.substr(sp1 + 1, sp2 - sp1 - 1);
                std::string lower = in.substr(0, head_end);
                for (char &c : lower)
                    c = (char)std::tolower((unsigned char)c);
                const size_t cl = lower.find("\r\ncontent-length:");
                if (cl != std::string::npos)
                    length = (size_t)std::strtoul(lower.c_str() + cl + 17, nullptr, 10);
                if (length > kMaxBody)
                    return false;
            } else if (in.size() > kMaxHead) {
                return false;
            }
        }
        if (head_end != std::string::npos && in.size() >= head_end + 4 + length) {
            *body = in.substr(head_end + 4, length);
            return true;
        }
        const double left = deadline - now();
        if (left <= 0 || s_stop.load())
            return false;
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, (int)(left * 1000) + 1) <= 0)
            return false;
        const ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0)
            return false;
        in.append(buf, (size_t)n);
    }
}

void serve(int fd, const std::string &peer)
{
    std::string method, target, body;
    if (!read_request(fd, &method, &target, &body))
        return reply(fd, 400, "text/plain", "bad request");
    std::string code, path;
    {
        std::lock_guard<std::mutex> lock(s_lock);
        code = s_info.code;
        path = s_path;
        s_info.phone_seen = true;
    }
    const std::string where = target.substr(0, target.find('?'));
    if (method == "GET" && (where == "/" || where == "/index.html"))
        return reply(fd, 200, "text/html; charset=utf-8", config_web::page_html());
    if (where != "/api/config")
        return reply(fd, 404, "text/plain", "not found");
    if (!same(code_of(target), code)) {
        std::lock_guard<std::mutex> lock(s_lock);
        if (++s_wrong >= kMaxWrongCodes) {
            s_info.error = "too many wrong codes from " + peer;
            s_stop = true;
        }
        return reply(fd, 403, "application/json", json_error("wrong code"));
    }
    std::string current;
    read_text(path, &current);
    if (method == "GET")
        return reply(fd, 200, "application/json", config_web::public_view(current));
    if (method != "POST")
        return reply(fd, 400, "application/json", json_error("GET or POST"));
    std::string next, error;
    if (!config_web::apply(current, body, &next, &error))
        return reply(fd, 400, "application/json", json_error(error));
    if (!write_text(path, next))
        return reply(fd, 500, "application/json", json_error("the console could not write config.json"));
    {
        std::lock_guard<std::mutex> lock(s_lock);
        s_info.saves++;
    }
    reply(fd, 200, "application/json", "{\"ok\":true}");
}

void *run(void *)
{
    while (!s_stop.load()) {
        pollfd p{s_listen, POLLIN, 0};
        if (poll(&p, 1, 200) <= 0)
            continue;
        sockaddr_in peer{};
        socklen_t len = sizeof peer;
        const int fd = accept(s_listen, (sockaddr *)&peer, &len);
        if (fd < 0)
            continue;
        const std::string ip = ip_text(peer);
        if (config_web::is_local_address(ip))
            serve(fd, ip);
        else
            reply(fd, 403, "text/plain", "local network only");
        close(fd);
    }
    {
        std::lock_guard<std::mutex> lock(s_lock);
        s_info.running = false;
    }
    return nullptr;
}

} // namespace

bool can_share_config() { return &bind && &listen && &accept && &getsockname; }

bool share_start(const std::string &config_path, const std::string &hint_host, std::string *error)
{
    share_stop();
    if (!can_share_config()) {
        *error = "this system does not let the app listen";
        return false;
    }
    const std::string ip = local_ip(hint_host);
    if (ip.empty()) {
        *error = "no local network address";
        return false;
    }
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        *error = std::string("socket: ") + std::strerror(errno);
        return false;
    }
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    int port = 0;
    for (int p = kFirstPort; p < kFirstPort + kPorts && !port; p++) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons((uint16_t)p);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, (const sockaddr *)&a, sizeof a) == 0)
            port = p;
    }
    /* Which call refused tells a port in use from a title the console does
     * not let listen (0.3.0's param.json: "permission denied"). */
    const bool bound = port != 0;
    if (!bound || listen(fd, 4) != 0) {
        *error = std::string(bound ? "listen: " : "bind: ") + std::strerror(errno);
        close(fd);
        return false;
    }
    /* Six digits from the system's random source. */
    uint32_t r = 0;
    if (!random_bytes(&r, sizeof r))
        r = (uint32_t)(now() * 1e6) ^ (uint32_t)(uintptr_t)&r;
    char code[8];
    std::snprintf(code, sizeof code, "%06u", (unsigned)(r % 1000000u));
    {
        std::lock_guard<std::mutex> lock(s_lock);
        s_info = ShareInfo{};
        s_info.running = true;
        s_info.code = code;
        s_info.shown = "http://" + ip + ":" + std::to_string(port);
        s_info.address = s_info.shown + "/?c=" + code;
        s_path = config_path;
        s_wrong = 0;
    }
    s_listen = fd;
    s_stop = false;
    pthread_attr_t attr;   /* a roomy stack: the PS5 gives threads 64 KiB */
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 256u << 10);
    const int started = pthread_create(&s_thread, &attr, &run, nullptr);
    pthread_attr_destroy(&attr);
    if (started != 0) {
        close(fd);
        s_listen = -1;
        std::lock_guard<std::mutex> lock(s_lock);
        s_info.running = false;
        *error = "no thread";
        return false;
    }
    s_thread_on = true;
    return true;
}

void share_stop()
{
    if (s_thread_on) {
        s_stop = true;
        pthread_join(s_thread, nullptr);
        s_thread_on = false;
    }
    if (s_listen >= 0) {
        close(s_listen);
        s_listen = -1;
    }
    std::lock_guard<std::mutex> lock(s_lock);
    s_info.running = false;
}

ShareInfo share_info()
{
    std::lock_guard<std::mutex> lock(s_lock);
    return s_info;
}

} // namespace platform
