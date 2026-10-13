/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A very small test harness: TEST(name) { CHECK(cond); CHECK_EQ(a, b); }.
 */
#pragma once

#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Test {
    const char *name;
    std::function<void()> fn;
};

inline std::vector<Test> &registry()
{
    static std::vector<Test> r;
    return r;
}

inline int &failures()
{
    static int f = 0;
    return f;
}

struct Register {
    Register(const char *name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char *file, int line, const std::string &what)
{
    failures()++;
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
}

template <typename T> std::string show(const T &v)
{
    std::ostringstream s;
    s << v;
    return s.str();
}

/* A fixture's text (REGIE5_FIXTURES is the fixtures directory, or root). */
inline std::string fixture(const std::string &rel, const std::string &root = REGIE5_FIXTURES)
{
    std::ifstream in(root + "/" + rel, std::ios::binary);
    if (!in) {
        fail(__FILE__, __LINE__, "missing fixture " + rel);
        return {};
    }
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

} // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                                                                     \
    static void name();                                                                                                \
    static check::Register CHECK_CAT(reg_, name)(#name, name);                                                        \
    static void name()
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond))                                                                                                   \
            check::fail(__FILE__, __LINE__, #cond);                                                                    \
    } while (0)
#define CHECK_EQ(a, b)                                                                                                 \
    do {                                                                                                               \
        const auto &check_a = (a);                                                                                     \
        const auto &check_b = (b);                                                                                     \
        if (!(check_a == check_b))                                                                                     \
            check::fail(__FILE__, __LINE__,                                                                            \
                        std::string(#a " == " #b ": ") + check::show(check_a) + " vs " + check::show(check_b));       \
    } while (0)
