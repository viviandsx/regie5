/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace json {

namespace {

/* Walks a document without building it: only the list's elements go to cJSON. */
class Walker {
public:
    Walker(const std::string &t, const std::function<void(const cJSON *)> &fn)
        : m_p(t.data()), m_end(t.data() + t.size()), m_fn(fn)
    {
    }

    Each run(std::initializer_list<const char *> path)
    {
        m_path = path.begin();
        m_levels = path.size();
        Each r;
        if (m_levels == 0) {
            space();
            if (m_p >= m_end || *m_p != '[')   /* an error object is not a list */
                return Each::Bad;
            r = list() ? Each::Done : Each::Bad;
        } else {
            r = object(0);
        }
        space();
        return r != Each::Bad && m_p == m_end ? r : Each::Bad;
    }

private:
    void space()
    {
        while (m_p < m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r'))
            m_p++;
    }

    /* At a '"': past the string; its raw text in *out. */
    bool string(std::string *out)
    {
        if (m_p >= m_end || *m_p != '"')
            return false;
        const char *start = ++m_p;
        while (m_p < m_end && *m_p != '"') {
            if (*m_p == '\\' && ++m_p >= m_end)
                return false;
            m_p++;
        }
        if (m_p >= m_end)
            return false;
        if (out)
            out->assign(start, m_p);
        m_p++;
        return true;
    }

    /* Past any value (its brackets balanced; cJSON checks the elements). */
    bool skip()
    {
        space();
        if (m_p >= m_end)
            return false;
        if (*m_p == '"')
            return string(nullptr);
        if (*m_p == '{' || *m_p == '[') {
            int depth = 0;
            while (m_p < m_end) {
                const char c = *m_p;
                if (c == '"') {
                    if (!string(nullptr))
                        return false;
                    continue;
                }
                if (c == '{' || c == '[') {
                    depth++;
                } else if (c == '}' || c == ']') {
                    if (--depth == 0) {
                        m_p++;
                        return true;
                    }
                }
                m_p++;
            }
            return false;
        }
        const char *start = m_p;
        while (m_p < m_end && *m_p != ',' && *m_p != '}' && *m_p != ']' && *m_p != ' ' && *m_p != '\t' && *m_p != '\n' &&
               *m_p != '\r')
            m_p++;
        return m_p > start;
    }

    /* One element: its text parsed alone, then handed over. */
    bool element()
    {
        space();
        const char *start = m_p;
        if (!skip())
            return false;
        cJSON *e = cJSON_ParseWithLength(start, (size_t)(m_p - start));
        if (!e)
            return false;
        m_fn(e);
        cJSON_Delete(e);
        return true;
    }

    /* At the list's value: an array, or a lone object. */
    bool list()
    {
        space();
        if (m_p < m_end && *m_p == '{')
            return element();
        if (m_p >= m_end || *m_p != '[')
            return false;
        m_p++;
        space();
        if (m_p < m_end && *m_p == ']') {
            m_p++;
            return true;
        }
        for (;;) {
            if (!element())
                return false;
            space();
            if (m_p < m_end && *m_p == ',') {
                m_p++;
                continue;
            }
            if (m_p < m_end && *m_p == ']') {
                m_p++;
                return true;
            }
            return false;
        }
    }

    /* At an object on the path, level deep: its members, the one on the path followed. */
    Each object(size_t level)
    {
        space();
        if (m_p >= m_end || *m_p != '{')
            return Each::Bad;
        m_p++;
        Each found = level + 1 == m_levels ? Each::Missing : Each::Bad;   /* a member on the way must be there */
        bool seen = false;
        space();
        if (m_p < m_end && *m_p == '}') {
            m_p++;
            return found;
        }
        for (;;) {
            space();
            std::string key;
            if (!string(&key))
                return Each::Bad;
            space();
            if (m_p >= m_end || *m_p != ':')
                return Each::Bad;
            m_p++;
            if (!seen && key == m_path[level]) {
                seen = true;
                if (level + 1 == m_levels) {
                    if (!list())
                        return Each::Bad;
                    found = Each::Done;
                } else {
                    found = object(level + 1);
                    if (found == Each::Bad)
                        return Each::Bad;
                }
            } else if (!skip()) {
                return Each::Bad;
            }
            space();
            if (m_p < m_end && *m_p == ',') {
                m_p++;
                continue;
            }
            if (m_p < m_end && *m_p == '}') {
                m_p++;
                return found;
            }
            return Each::Bad;
        }
    }

    const char *m_p, *m_end;
    const std::function<void(const cJSON *)> &m_fn;
    const char *const *m_path = nullptr;
    size_t m_levels = 0;
};

} // namespace

Each each(const std::string &text, std::initializer_list<const char *> path, const std::function<void(const cJSON *)> &fn)
{
    return Walker(text, fn).run(path);
}

const cJSON *get(const cJSON *o, const char *key)
{
    return cJSON_IsObject(o) ? cJSON_GetObjectItemCaseSensitive(o, key) : nullptr;
}

std::string str(const cJSON *o, const char *key, const std::string &def)
{
    const cJSON *v = get(o, key);
    return cJSON_IsString(v) && v->valuestring ? std::string(v->valuestring) : def;
}

int64_t i64(const cJSON *o, const char *key, int64_t def)
{
    const cJSON *v = get(o, key);
    return cJSON_IsNumber(v) ? (int64_t)std::llround(v->valuedouble) : def;
}

int integer(const cJSON *o, const char *key, int def) { return (int)i64(o, key, def); }

double num(const cJSON *o, const char *key, double def)
{
    const cJSON *v = get(o, key);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

bool boolean(const cJSON *o, const char *key, bool def)
{
    const cJSON *v = get(o, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : def;
}

int64_t days_from_civil(int y, int m, int d)
{
    /* Howard Hinnant's algorithm. */
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

time_t parse_time(const std::string &s)
{
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0, n = 0;
    if (std::sscanf(s.c_str(), "%4d-%2d-%2d%n", &y, &mo, &d, &n) != 3 || y < 1900 || mo < 1 || mo > 12 || d < 1 ||
        d > 31)
        return 0;
    size_t i = (size_t)n;
    if (i < s.size() && (s[i] == 'T' || s[i] == ' ')) {
        int m2 = 0;
        if (std::sscanf(s.c_str() + i + 1, "%2d:%2d:%2d%n", &h, &mi, &sec, &m2) != 3)
            return 0;
        i += 1 + m2;
        if (i < s.size() && s[i] == '.')   /* fractions: dropped */
            while (++i < s.size() && s[i] >= '0' && s[i] <= '9') {
            }
    }
    int64_t offset = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        int oh = 0, om = 0;
        if (std::sscanf(s.c_str() + i + 1, "%2d:%2d", &oh, &om) >= 1)
            offset = (s[i] == '+' ? 1 : -1) * (oh * 3600 + om * 60);
    }
    return (time_t)(days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec - offset);
}

int64_t parse_timespan(const std::string &s)
{
    if (s.empty())
        return -1;
    int64_t days = 0;
    int h = 0, m = 0, sec = 0;
    const size_t colon = s.find(':');
    const size_t dot = s.find('.');
    const char *p = s.c_str();
    if (dot != std::string::npos && colon != std::string::npos && dot < colon) {   /* "d.hh:mm:ss" */
        days = std::strtoll(p, nullptr, 10);
        p += dot + 1;
    }
    if (std::sscanf(p, "%d:%d:%d", &h, &m, &sec) != 3 || h < 0 || m < 0 || sec < 0)
        return -1;
    return days * 86400 + h * 3600 + m * 60 + sec;
}

} // namespace json
