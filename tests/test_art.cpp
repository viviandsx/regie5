/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The picture cache (ui/art) on a counting transport that draws its own PNG
 * pictures: decoded to the size asked, never past the decoding limit (the
 * Emby lesson), within its memory budget, failures kept until a retry.
 */
#include "check.h"
#include "gfx_stub.h"

#include "api/services.h"
#include "ui/art.h"

#include <chrono>
#include <map>
#include <mutex>
#include <thread>

namespace {

uint32_t crc32(const std::string &s)
{
    uint32_t c = 0xffffffffu;
    for (unsigned char b : s) {
        c ^= b;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

void be32(std::string *o, uint32_t v)
{
    for (int s = 24; s >= 0; s -= 8)
        o->push_back((char)((v >> s) & 0xff));
}

void chunk(std::string *png, const char *type, const std::string &data)
{
    be32(png, (uint32_t)data.size());
    const std::string body = std::string(type, 4) + data;
    png->append(body);
    be32(png, crc32(body));
}

/* An RGBA PNG of w x h, its pixels stored uncompressed; header_only: the
 * header alone (enough for a size check, never decodable). */
std::string png(int w, int h, bool header_only = false)
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

/* ".../img/<w>x<h>/<name>": a picture of that size; ".../header/<w>x<h>/...":
 * only its header; anything else: 404. Counts what it is asked. */
class Pictures : public http::Transport {
public:
    http::Response perform(const http::Request &req, const std::atomic<bool> *) override
    {
        const std::string path = req.url.substr(req.url.find('/', req.url.find("://") + 3));
        {
            std::lock_guard<std::mutex> l(m);
            asked[path]++;
        }
        http::Response r;
        int w = 0, h = 0;
        const bool header = path.rfind("/header/", 0) == 0;
        if ((path.rfind("/img/", 0) == 0 || header) &&
            std::sscanf(path.c_str() + (header ? 8 : 5), "%dx%d", &w, &h) == 2) {
            r.status = 200;
            r.content_type = "image/png";
            r.body = png(w, h, header);
        } else {
            r.status = 404;
        }
        return r;
    }
    int count(const std::string &path)
    {
        std::lock_guard<std::mutex> l(m);
        return asked[path];
    }
    int total()
    {
        std::lock_guard<std::mutex> l(m);
        int n = 0;
        for (const auto &kv : asked)
            n += kv.second;
        return n;
    }
    std::mutex m;
    std::map<std::string, int> asked;
};

/* A cache on its own transport, emptied before and after. */
struct Rig {
    Pictures net;
    http::Pool pool{net, 3, nullptr};
    api::Services services{config::mock(), pool, "fr"};
    Rig()
    {
        art::init(&services, &pool);
        art::forget(~0u);
        art::set_budget(192u << 20);
    }
    ~Rig()
    {
        settle();
        art::forget(~0u);
        art::init(nullptr, nullptr);
    }
    /* Frames until nothing is on its way (each frame asks again for refs). */
    void frames(int n, const std::function<void()> &draw = {})
    {
        for (int i = 0; i < n; i++) {
            art::frame();
            if (draw)
                draw();
            pool.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    void settle(const std::function<void()> &draw = {})
    {
        frames(2, draw);   /* asked, then sent */
        for (int i = 0; i < 500 && (pool.busy() > 0 || art::busy()); i++)
            frames(1, draw);
        frames(2, draw);
    }
};

art::Ref pic(const std::string &path) { return {config::Radarr, path, 0}; }

} // namespace

TEST(art_decodes_to_the_size_asked)
{
    Rig rig;
    const int made = gfx_stub::made;
    const art::Ref r = pic("/img/500x750/a.png");
    auto draw = [&] { art::get(r, 360, 540); };
    rig.settle(draw);
    const gfx::Texture *t = art::get(r, 360, 540);
    CHECK(t != nullptr);
    CHECK_EQ(gfx::texture_width(t), 360);
    CHECK_EQ(gfx::texture_height(t), 540);
    CHECK_EQ(gfx_stub::made - made, 1);
    CHECK_EQ(rig.net.count("/img/500x750/a.png"), 1);
    /* Smaller than asked: kept at its own size, never enlarged. */
    const art::Ref small = pic("/img/100x150/s.png");
    rig.settle([&] { art::get(small, 360, 540); });
    CHECK_EQ(gfx::texture_width(art::get(small, 360, 540)), 100);
}

TEST(art_never_decodes_past_the_limit)
{
    Rig rig;
    const int made = gfx_stub::made;
    /* 5000 x 3000 (past 4096 x 2304): read as a header, refused, a drawn card instead. */
    const art::Ref huge = pic("/header/5000x3000/huge.png");
    rig.settle([&] { art::get(huge, 360, 540); });
    CHECK(art::failed(huge, 360, 540));
    CHECK(art::get(huge, 360, 540) == nullptr);
    CHECK_EQ(gfx_stub::made, made);
    /* Not a picture at all, or missing: failed too, asked once. */
    const art::Ref junk = pic("/missing.jpg");
    rig.settle([&] { art::get(junk, 360, 540); });
    CHECK(art::failed(junk, 360, 540));
    rig.frames(5, [&] { art::get(junk, 360, 540); });
    CHECK_EQ(rig.net.count("/missing.jpg"), 1);
    /* A refresh forgets the failures: asked again. */
    art::retry_failed();
    rig.settle([&] { art::get(junk, 360, 540); });
    CHECK_EQ(rig.net.count("/missing.jpg"), 2);
}

TEST(art_keeps_to_its_budget)
{
    Rig rig;
    const size_t one = 100 * 150 * 4;
    art::set_budget(3 * one);
    const size_t before = gfx_stub::live_bytes;
    /* Six pictures, each drawn a while then left: the oldest go. */
    for (int i = 0; i < 6; i++) {
        const art::Ref r = pic("/img/100x150/p" + std::to_string(i) + ".png");
        rig.settle([&] { art::get(r, 100, 150); });
        CHECK(art::get(r, 100, 150) != nullptr);
        rig.frames(4, [&] { art::get(r, 100, 150); });
        CHECK(gfx_stub::live_bytes - before <= 3 * one);
    }
    /* The last one stays, the first is gone (asked again if drawn again). */
    CHECK(art::get(pic("/img/100x150/p5.png"), 100, 150) != nullptr);
    CHECK(art::get(pic("/img/100x150/p0.png"), 100, 150) == nullptr);
    /* Forgetting a service releases its textures. */
    art::forget(1u << config::Radarr);
    CHECK_EQ(gfx_stub::live_bytes, before);
}

TEST(art_asks_each_file_once)
{
    Rig rig;
    const art::Ref r = pic("/img/200x300/same.png");
    /* Two sizes asked in the same frame: one request, two textures (the
     * test's PNG files are stored uncompressed: 240 KiB, under the 1 MiB kept at most). */
    rig.settle([&] {
        art::get(r, 160, 240);
        art::get(r, 100, 150);
    });
    CHECK_EQ(rig.net.count("/img/200x300/same.png"), 1);
    CHECK_EQ(gfx::texture_width(art::get(r, 160, 240)), 160);
    CHECK_EQ(gfx::texture_width(art::get(r, 100, 150)), 100);
    /* A third size later, and a blurred copy: decoded from the file kept. */
    rig.settle([&] {
        art::get(r, 80, 120);
        art::get(r, 64, 96, 6);
    });
    CHECK(art::get(r, 80, 120) != nullptr && art::get(r, 64, 96, 6) != nullptr);
    CHECK_EQ(rig.net.count("/img/200x300/same.png"), 1);
    /* Textures let go (budget), drawn again: still not asked again. */
    art::set_budget(1);
    rig.frames(6, [&] { art::get(pic("/img/10x10/other.png"), 10, 10); });
    CHECK(art::get(r, 160, 240) == nullptr);
    art::set_budget(192u << 20);
    rig.settle([&] { art::get(r, 160, 240); });
    CHECK(art::get(r, 160, 240) != nullptr);
    CHECK_EQ(rig.net.count("/img/200x300/same.png"), 1);
    /* The service forgotten (config.json changed): asked again. */
    art::forget(1u << config::Radarr);
    rig.settle([&] { art::get(r, 160, 240); });
    CHECK_EQ(rig.net.count("/img/200x300/same.png"), 2);
    /* The files' budget: the least recent go. */
    art::set_file_budget(3 * 1000);
    for (int i = 0; i < 4; i++) {
        const art::Ref k = pic("/img/20x20/k" + std::to_string(i) + ".png");
        rig.settle([&] { art::get(k, 20, 20); });
    }
    art::forget(0);   /* nothing forgotten: only to be sure files stay unless asked */
    art::set_budget(1);
    rig.frames(6, [&] { art::get(pic("/img/10x10/other.png"), 10, 10); });
    art::set_budget(192u << 20);
    rig.settle([&] { art::get(pic("/img/20x20/k3.png"), 20, 20); });
    CHECK_EQ(rig.net.count("/img/20x20/k3.png"), 1);   /* the latest kept */
    rig.settle([&] { art::get(pic("/img/20x20/k0.png"), 20, 20); });
    CHECK_EQ(rig.net.count("/img/20x20/k0.png"), 2);   /* the oldest gone */
    art::set_file_budget(32u << 20);
}
