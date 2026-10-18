/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/art.h"

#include "api/jellyfin.h"
#include "core/perf.h"
#include "platform/platform.h"
#include "ui/poster.h"
#include "ui/style.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace art {

namespace {

size_t s_budget = 192u << 20;   /* bytes of textures kept (art::set_budget) */
constexpr int kUploadsPerFrame = 3;
/* Pictures on their way at once: the rest wait here, the ones on screen first.
 * A picture asked for and then scrolled past is dropped before it is fetched. */
constexpr int kInFlight = 8;
constexpr uint64_t kStaleFrames = 20;
/* Decoding holds about 7 bytes a pixel of the heap for a while (the picture
 * and the decoder's planes), and a PS5 heap is 128 to 256 MiB: a picture past
 * kMaxPixels is not decoded (a drawn cover stands in), and those past
 * kBigPixels go one at a time, never three at once on the image workers. */
constexpr long long kMaxPixels = 4096LL * 2304;   /* a little over 4K UHD */
constexpr long long kBigPixels = 1920LL * 1080;
std::mutex s_big;

struct Pixels {
    std::vector<uint8_t> rgba;   /* premultiplied */
    int w = 0, h = 0;
};

struct Entry {
    enum State { Wanted, Loading, Decoded, Ready, Failed } state = Wanted;
    Ref ref;
    int mw = 0, mh = 0, blur = 0;
    uint64_t asked = 0;   /* order of asking, within a frame: the screen's reading order */
    uint64_t sent = 0;    /* which request is on its way for it (its answer must match) */
    std::shared_ptr<Pixels> px;
    gfx::Texture *tex = nullptr;
    double ready_at = 0;
    uint64_t used = 0;
    size_t bytes = 0;
};

api::Services *s_services = nullptr;
http::Pool *s_pool = nullptr;
std::unordered_map<std::string, Entry> s_cache;
uint64_t s_frame = 0;
size_t s_bytes = 0;
int s_loading = 0;
uint64_t s_asked = 0;
uint64_t s_sent = 0;

std::string key_of(const Ref &r, int w, int h, int blur)
{
    return std::to_string((int)r.service) + "." + std::to_string(r.instance) + "|" + r.path + "|" + std::to_string(w) + "x" + std::to_string(h) + "|" +
           std::to_string(blur);
}

/* Fits (sw, sh) in (mw, mh), never larger than it is. */
void fit(int sw, int sh, int mw, int mh, int *w, int *h)
{
    const double k = std::min(1.0, std::min((double)mw / sw, (double)mh / sh));
    *w = std::max(1, (int)std::lround(sw * k));
    *h = std::max(1, (int)std::lround(sh * k));
}

/* Area-averaging downscale (good enough from a poster to a card). */
Pixels scale(const uint8_t *src, int sw, int sh, int w, int h)
{
    Pixels out;
    out.w = w, out.h = h;
    out.rgba.resize((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        const int y0 = y * sh / h, y1 = std::max(y0 + 1, (y + 1) * sh / h);
        for (int x = 0; x < w; x++) {
            const int x0 = x * sw / w, x1 = std::max(x0 + 1, (x + 1) * sw / w);
            uint32_t acc[4] = {0, 0, 0, 0};
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const uint8_t *p = src + ((size_t)yy * sw + xx) * 4;
                    for (int c = 0; c < 4; c++)
                        acc[c] += p[c];
                }
            const uint32_t n = (uint32_t)((y1 - y0) * (x1 - x0));
            uint8_t *d = &out.rgba[((size_t)y * w + x) * 4];
            const uint32_t a = acc[3] / n;
            for (int c = 0; c < 3; c++)   /* premultiply */
                d[c] = (uint8_t)((acc[c] / n) * a / 255);
            d[3] = (uint8_t)a;
        }
    }
    return out;
}

/* Three box blurs (close to a Gaussian), radius in pixels, edges clamped. */
void blur(Pixels &p, int radius)
{
    std::vector<uint8_t> tmp(p.rgba.size());
    for (int pass = 0; pass < 3; pass++)
        for (int dir = 0; dir < 2; dir++) {
            const int n = dir == 0 ? p.w : p.h, lines = dir == 0 ? p.h : p.w;
            for (int l = 0; l < lines; l++)
                for (int i = 0; i < n; i++) {
                    uint32_t acc[4] = {0, 0, 0, 0};
                    for (int k = -radius; k <= radius; k++) {
                        const int j = std::clamp(i + k, 0, n - 1);
                        const size_t at = dir == 0 ? ((size_t)l * p.w + j) * 4 : ((size_t)j * p.w + l) * 4;
                        for (int c = 0; c < 4; c++)
                            acc[c] += p.rgba[at + c];
                    }
                    const size_t to = dir == 0 ? ((size_t)l * p.w + i) * 4 : ((size_t)i * p.w + l) * 4;
                    for (int c = 0; c < 4; c++)
                        tmp[to + c] = (uint8_t)(acc[c] / (2 * radius + 1));
                }
            p.rgba.swap(tmp);
        }
}

std::shared_ptr<Pixels> decode(const std::string &body, int mw, int mh, int blur_r)
{
    int sw = 0, sh = 0, comp = 0;
    if (!stbi_info_from_memory((const stbi_uc *)body.data(), (int)body.size(), &sw, &sh, &comp) ||
        (long long)sw * sh > kMaxPixels)
        return nullptr;
    std::unique_lock<std::mutex> one_big(s_big, std::defer_lock);
    if ((long long)sw * sh > kBigPixels)
        one_big.lock();
    uint8_t *src = stbi_load_from_memory((const stbi_uc *)body.data(), (int)body.size(), &sw, &sh, &comp, 4);
    if (!src)
        return nullptr;
    int w = 0, h = 0;
    fit(sw, sh, mw, mh, &w, &h);
    auto px = std::make_shared<Pixels>(scale(src, sw, sh, w, h));
    stbi_image_free(src);
    if (blur_r > 0)
        blur(*px, blur_r);
    return px;
}

void submit(const std::string &key, Entry &e)
{
    e.state = Entry::Loading;
    s_loading++;
    http::Request req;
    const bool emby = e.ref.service == config::Jellyfin && s_services->config().emby();
    req.url = s_services->url(e.ref.service, emby ? jellyfin::emby_image_path(e.ref.path) : e.ref.path, e.ref.instance);
    const std::string auth = s_services->auth_header(e.ref.service, e.ref.instance);
    if (!auth.empty())
        req.headers.push_back(auth);
    req.timeout_s = 20;
    req.insecure = s_services->config().at(e.ref.service, e.ref.instance).insecure;
    req.server = s_services->config().at(e.ref.service, e.ref.instance).url;   /* its own turn (http::Pool) */
    const int mw = e.mw, mh = e.mh, blur_r = e.blur;
    const uint64_t sent = e.sent = ++s_sent;
    s_pool->submit(std::move(req), [key, mw, mh, blur_r, sent](http::Response &resp) -> std::function<void()> {
        std::shared_ptr<Pixels> px = resp.ok() ? decode(resp.body, mw, mh, blur_r) : nullptr;
        return [key, px, sent] {
            s_loading--;
            auto f = s_cache.find(key);
            if (f == s_cache.end() || f->second.sent != sent)
                return;
            f->second.state = px ? Entry::Decoded : Entry::Failed;
            f->second.px = px;
        };
    });
}

Entry *request(const Ref &ref, int mw, int mh, int blur_r)
{
    if (ref.empty() || !s_services || !s_pool || !s_services->enabled(ref.service, ref.instance))
        return nullptr;
    const std::string key = key_of(ref, mw, mh, blur_r);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) {
        if (it->second.used != s_frame)
            it->second.asked = ++s_asked;
        it->second.used = s_frame;
        return &it->second;
    }
    Entry &e = s_cache[key];
    e.ref = ref;
    e.mw = mw, e.mh = mh, e.blur = blur_r;
    e.used = s_frame;
    e.asked = ++s_asked;
    return &e;   /* waits for frame() to send it */
}

} // namespace

void set_budget(size_t bytes) { s_budget = bytes; }

void init(api::Services *services, http::Pool *images)
{
    s_services = services;
    s_pool = images;
}

void frame()
{
    s_frame++;
    /* Send what waits, most recently asked first; forget what was scrolled past. */
    std::vector<std::pair<std::pair<uint64_t, uint64_t>, std::string>> wanted;
    for (auto it = s_cache.begin(); it != s_cache.end();) {
        Entry &e = it->second;
        if (e.state == Entry::Wanted && e.used + kStaleFrames < s_frame) {
            it = s_cache.erase(it);
            continue;
        }
        if (e.state == Entry::Wanted)
            wanted.push_back({{~e.used, e.asked}, it->first});
        ++it;
    }
    std::sort(wanted.begin(), wanted.end());
    for (const auto &w : wanted) {
        if (s_loading >= kInFlight)
            break;
        submit(w.second, s_cache[w.second]);
    }
    int uploads = 0;
    for (auto &kv : s_cache) {
        Entry &e = kv.second;
        if (e.state != Entry::Decoded || uploads >= kUploadsPerFrame)
            continue;
        perf::Frame &pf = perf::cur();
        perf::Timer timer(&pf.upload_ms);
        pf.uploads++;
        e.tex = gfx::texture_from_pixels(e.px->rgba.data(), e.px->w, e.px->h, e.px->w);
        e.bytes = e.px->rgba.size();
        s_bytes += e.bytes;
        e.px.reset();
        e.state = e.tex ? Entry::Ready : Entry::Failed;
        e.ready_at = platform::now();
        uploads++;
    }
    if (s_bytes <= s_budget)
        return;
    /* Over budget: drop the least recently drawn (never one drawn in the last frames). */
    std::vector<std::pair<uint64_t, std::string>> old;
    for (auto &kv : s_cache)
        if (kv.second.state == Entry::Ready && kv.second.used + 2 < s_frame)
            old.push_back({kv.second.used, kv.first});
    std::sort(old.begin(), old.end());
    for (const auto &o : old) {
        if (s_bytes <= s_budget * 3 / 4)
            break;
        Entry &e = s_cache[o.second];
        gfx::texture_release(e.tex);
        s_bytes -= e.bytes;
        s_cache.erase(o.second);
    }
}

const gfx::Texture *get(const Ref &ref, int mw, int mh, int blur_r)
{
    Entry *e = request(ref, mw, mh, blur_r);
    return e && e->state == Entry::Ready ? e->tex : nullptr;
}

bool failed(const Ref &ref, int mw, int mh, int blur_r)
{
    if (ref.empty() || !s_services || !s_services->enabled(ref.service, ref.instance))
        return true;
    Entry *e = request(ref, mw, mh, blur_r);
    return e && e->state == Entry::Failed;
}

float fade(const Ref &ref, int mw, int mh, int blur_r, float seconds)
{
    Entry *e = request(ref, mw, mh, blur_r);
    if (!e || e->state != Entry::Ready)
        return 0.f;
    const float t = (float)((platform::now() - e->ready_at) / seconds);
    return t >= 1.f ? 1.f : t <= 0.f ? 0.f : t * t * (3.f - 2.f * t);
}

bool busy()
{
    if (s_loading > 0)
        return true;
    for (const auto &kv : s_cache)
        if (kv.second.state == Entry::Wanted)
            return true;
    const double now = platform::now();
    for (const auto &kv : s_cache)
        if (kv.second.state == Entry::Decoded ||
            (kv.second.state == Entry::Ready && now - kv.second.ready_at < 0.5 && kv.second.used + 1 >= s_frame))
            return true;
    return false;
}

void retry_failed()
{
    for (auto it = s_cache.begin(); it != s_cache.end();)
        it = it->second.state == Entry::Failed ? s_cache.erase(it) : std::next(it);
}

void forget(unsigned services)
{
    for (auto it = s_cache.begin(); it != s_cache.end();) {
        Entry &e = it->second;
        if (!(services & (1u << e.ref.service))) {
            ++it;
            continue;
        }
        if (e.tex) {
            gfx::texture_release(e.tex);
            s_bytes -= e.bytes;
        }
        it = s_cache.erase(it);
    }
}

void draw_poster(const gfx::Rect &r, const Ref &ref, const std::string &title, uint32_t seed, float radius,
                 float opacity, int px_w)
{
    const int pw = px_w, ph = px_w * 3 / 2;
    if (failed(ref, pw, ph)) {
        ui::draw_generated_card(r, title, seed, radius, opacity);
        return;
    }
    const gfx::Texture *t = get(ref, pw, ph);
    const float f = t ? fade(ref, pw, ph) : 0.f;
    if (f < 1.f)
        ui::glass_flat(r, radius, opacity * (1.f - f));   /* the placeholder while it comes */
    if (t)
        gfx::image(r, t, opacity * f, radius, true);
}

} // namespace art
