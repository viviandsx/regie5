/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PNG pictures made by the tests (stored, not compressed): a picture of any
 * size without a file.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace png_maker {

inline uint32_t crc32(const std::string &s)
{
    uint32_t c = 0xffffffffu;
    for (unsigned char b : s) {
        c ^= b;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

inline void be32(std::string *o, uint32_t v)
{
    for (int s = 24; s >= 0; s -= 8)
        o->push_back((char)((v >> s) & 0xff));
}

inline void chunk(std::string *png, const char *type, const std::string &data)
{
    be32(png, (uint32_t)data.size());
    const std::string body = std::string(type, 4) + data;
    png->append(body);
    be32(png, crc32(body));
}

/* An RGBA PNG of w x h, its pixels stored uncompressed; header_only: the
 * header alone (enough for a size check, never decodable). */
inline std::string png(int w, int h, bool header_only = false)
{
    std::string out = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    be32(&ihdr, (uint32_t)w);
    be32(&ihdr, (uint32_t)h);
    ihdr += std::string("\x08\x06\x00\x00\x00", 5);
    chunk(&out, "IHDR", ihdr);
    if (header_only)
        return out;
    std::string raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);   /* no filter */
        for (int x = 0; x < w; x++)
            raw += std::string{(char)x, (char)y, (char)(x + y), (char)0xff};
    }
    std::string z = "\x78\x01";
    uint32_t a = 1, b = 0;
    for (unsigned char ch : raw)
        a = (a + ch) % 65521, b = (b + a) % 65521;
    for (size_t at = 0; at < raw.size() || at == 0; at += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        const bool last = at + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((char)(n & 0xff));
        z.push_back((char)(n >> 8));
        z.push_back((char)(~n & 0xff));
        z.push_back((char)((~n >> 8) & 0xff));
        z.append(raw, at, n);
        if (last)
            break;
    }
    be32(&z, (b << 16) | a);
    chunk(&out, "IDAT", z);
    chunk(&out, "IEND", "");
    return out;
}

} // namespace png_maker
