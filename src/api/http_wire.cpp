/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/http_wire.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace http::wire {

namespace {

constexpr size_t kMaxHead = 64u << 10;    /* status line and headers */
constexpr size_t kMaxBody = 64u << 20;    /* far past anything these APIs send */

std::string lower(std::string s)
{
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t'))
        a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
        b--;
    return s.substr(a, b - a);
}

int64_t days_from_civil(int64_t y, int64_t m, int64_t d)   /* Howard Hinnant's, public domain */
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

} // namespace

bool parse_http_date(const std::string &s, int64_t *utc)
{
    static const char *kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char wday[4] = {}, mon[4] = {}, zone[4] = {};
    int d = 0, y = 0, h = 0, mi = 0, se = 0;
    if (std::sscanf(s.c_str(), "%3s, %d %3s %d %d:%d:%d %3s", wday, &d, mon, &y, &h, &mi, &se, zone) != 8 ||
        std::string(zone) != "GMT")
        return false;
    int m = 0;
    while (m < 12 && std::string(kMonths[m]) != mon)
        m++;
    if (m == 12 || d < 1 || d > 31 || y < 1970 || h > 23 || mi > 59 || se > 60)
        return false;
    *utc = days_from_civil(y, m + 1, d) * 86400 + h * 3600 + mi * 60 + se;
    return true;
}

bool parse_url(const std::string &url, Url *out)
{
    Url u;
    const size_t colon = url.find("://");
    if (colon == std::string::npos)
        return false;
    u.scheme = lower(url.substr(0, colon));
    if (u.scheme != "http" && u.scheme != "https")
        return false;
    const size_t start = colon + 3;
    size_t end = url.find_first_of("/?#", start);
    if (end == std::string::npos)
        end = url.size();
    std::string authority = url.substr(start, end - start);
    if (authority.find('@') != std::string::npos)
        return false;   /* credentials in the address: not something these services use */
    std::string port;
    if (!authority.empty() && authority[0] == '[') {   /* [IPv6]:port */
        const size_t close = authority.find(']');
        if (close == std::string::npos)
            return false;
        u.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':')
                return false;
            port = authority.substr(close + 2);
        }
    } else {
        const size_t c = authority.find(':');
        u.host = authority.substr(0, c);
        if (c != std::string::npos)
            port = authority.substr(c + 1);
    }
    if (u.host.empty())
        return false;
    if (!port.empty()) {
        for (char ch : port)
            if (!std::isdigit((unsigned char)ch))
                return false;
        u.port = std::atoi(port.c_str());
        if (u.port <= 0 || u.port > 65535)
            return false;
    } else {
        u.port = u.scheme == "https" ? 443 : 80;
    }
    u.target = url.substr(end);
    const size_t hash = u.target.find('#');
    if (hash != std::string::npos)
        u.target.erase(hash);
    if (u.target.empty() || u.target[0] != '/')
        u.target.insert(0, "/");
    *out = u;
    return true;
}

std::string request_text(const Request &req, const Url &u)
{
    std::string host = u.host.find(':') != std::string::npos ? "[" + u.host + "]" : u.host;
    if (u.port != (u.scheme == "https" ? 443 : 80))
        host += ":" + std::to_string(u.port);
    std::string t = req.method + " " + u.target + " HTTP/1.1\r\n";
    t += "Host: " + host + "\r\n";
    t += "User-Agent: Regie5\r\n";
    t += "Accept: application/json, image/*, */*\r\n";
    t += "Accept-Encoding: identity\r\n";
    t += "Connection: close\r\n";
    for (const std::string &h : req.headers)
        t += h + "\r\n";
    if (!req.body.empty())
        t += "Content-Type: application/json\r\n";
    if (!req.body.empty() || req.method != "GET")
        t += "Content-Length: " + std::to_string(req.body.size()) + "\r\n";
    t += "\r\n";
    t += req.body;
    return t;
}

Reader::Reader(bool head, size_t max_body) : m_head(head), m_max(max_body ? max_body : kMaxBody) {}

Reader::State Reader::too_big()
{
    too_large = true;
    return fail("answer too large");
}

Reader::State Reader::fail(const char *why)
{
    m_part = Broken;
    error = why;
    body.clear();
    return Bad;
}

Reader::State Reader::feed(const char *data, size_t n)
{
    switch (m_part) {
    case Finished: return Done;
    case Broken: return Bad;
    case Head:
        m_buf.append(data, n);
        return parse_head();
    case Sized: {
        const size_t take = n < m_left ? n : m_left;
        body.append(data, take);
        m_left -= take;
        if (m_left == 0)
            m_part = Finished;
        return m_part == Finished ? Done : More;
    }
    case ToClose:
        if (body.size() + n > m_max)
            return too_big();
        body.append(data, n);
        return More;
    case Chunked:
        m_buf.append(data, n);
        return parse_chunks();
    }
    return Bad;
}

Reader::State Reader::finish()
{
    switch (m_part) {
    case Finished: return Done;
    case ToClose: m_part = Finished; return Done;
    case Broken: return Bad;
    case Head: return fail(m_buf.empty() ? "connection closed without an answer" : "answer cut short (headers)");
    case Sized:
    case Chunked: return fail("answer cut short (body)");
    }
    return Bad;
}

Reader::State Reader::parse_head()
{
    for (;;) {
        const size_t end = m_buf.find("\r\n\r\n");
        if (end == std::string::npos)
            return m_buf.size() > kMaxHead ? fail("headers too large") : More;
        const std::string head = m_buf.substr(0, end);
        m_buf.erase(0, end + 4);

        size_t line_end = head.find("\r\n");
        const std::string status_line = head.substr(0, line_end);
        if (status_line.compare(0, 7, "HTTP/1.") != 0 || status_line.size() < 12 || status_line[8] != ' ')
            return fail("not an HTTP answer");
        status = std::atoi(status_line.c_str() + 9);
        if (status < 100 || status > 999)
            return fail("not an HTTP answer");
        if (status >= 100 && status < 200)
            continue;   /* 100 Continue and the like: the real answer follows */

        long long length = -1;
        bool chunked = false;
        while (line_end != std::string::npos) {
            const size_t next = head.find("\r\n", line_end + 2);
            const std::string line = head.substr(line_end + 2, next == std::string::npos ? std::string::npos
                                                                                          : next - line_end - 2);
            line_end = next;
            const size_t c = line.find(':');
            if (c == std::string::npos)
                continue;
            const std::string name = lower(trim(line.substr(0, c)));
            const std::string value = trim(line.substr(c + 1));
            if (name == "content-length") {
                char *stop = nullptr;
                length = std::strtoll(value.c_str(), &stop, 10);
                if (!stop || *stop || length < 0)
                    return fail("bad Content-Length");
            } else if (name == "transfer-encoding") {
                chunked = lower(value).find("chunked") != std::string::npos;
            } else if (name == "date") {
                parse_http_date(value, &server_date);
            } else if (name == "content-type") {
                content_type = value;
            } else if (name == "content-encoding") {
                const std::string v = lower(value);
                if (!v.empty() && v != "identity")
                    return fail("compressed answer");
            }
        }

        std::string rest;
        rest.swap(m_buf);
        if (m_head || status == 204 || status == 304) {
            m_part = Finished;
            return Done;
        }
        if (chunked) {
            m_part = Chunked;
            m_buf = rest;
            return parse_chunks();
        }
        if (length >= 0) {
            if ((unsigned long long)length > m_max)
                return too_big();
            m_part = Sized;
            m_left = (size_t)length;
            if (m_left == 0) {
                m_part = Finished;
                return Done;
            }
            body.reserve(m_left);
            return rest.empty() ? More : feed(rest.data(), rest.size());
        }
        m_part = ToClose;
        return rest.empty() ? More : feed(rest.data(), rest.size());
    }
}

Reader::State Reader::parse_chunks()
{
    for (;;) {
        if (m_trailers) {
            const size_t nl = m_buf.find("\r\n");
            if (nl == std::string::npos)
                return m_buf.size() > kMaxHead ? fail("trailers too large") : More;
            const bool last = nl == 0;
            m_buf.erase(0, nl + 2);
            if (last) {
                m_part = Finished;
                return Done;
            }
            continue;
        }
        if (m_in_chunk) {
            if (m_left > 0) {
                const size_t take = m_buf.size() < m_left ? m_buf.size() : m_left;
                if (body.size() + take > m_max)
                    return too_big();
                body.append(m_buf, 0, take);
                m_buf.erase(0, take);
                m_left -= take;
                if (m_left > 0)
                    return More;
            }
            if (m_buf.size() < 2)
                return More;
            if (m_buf.compare(0, 2, "\r\n") != 0)
                return fail("bad chunk");
            m_buf.erase(0, 2);
            m_in_chunk = false;
            continue;
        }
        const size_t nl = m_buf.find("\r\n");
        if (nl == std::string::npos)
            return m_buf.size() > 1024 ? fail("bad chunk size") : More;
        const std::string size_text = m_buf.substr(0, nl);
        m_buf.erase(0, nl + 2);
        char *stop = nullptr;
        const unsigned long long size = std::strtoull(size_text.c_str(), &stop, 16);
        if (stop == size_text.c_str() || (*stop && *stop != ';' && *stop != ' ' && *stop != '\t'))
            return fail("bad chunk size");
        if (size > m_max)
            return too_big();
        if (size == 0) {
            m_trailers = true;
            continue;
        }
        m_left = (size_t)size;
        m_in_chunk = true;
    }
}

} // namespace http::wire
