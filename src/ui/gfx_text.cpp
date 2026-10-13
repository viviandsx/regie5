/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * gfx::text on top of the backend's textures, with a glyph atlas: each glyph
 * (weight, size, character) is rasterised by ui/font once and packed into a
 * few large textures; each (text, style, scale) is laid out once and kept as
 * quads into them. A new caption costs a layout, not a rasterisation and a
 * texture of its own; text and solid fills share the atlas, so they draw in
 * the same batches. Portable: every backend gets text from here.
 */
#include "ui/gfx.h"

#include "core/perf.h"
#include "ui/font.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace gfx {

namespace {

/* ---- the atlas ----------------------------------------------------------- */

size_t s_atlas_budget = 48u << 20;   /* bytes of pages before it starts afresh (set_atlas_budget) */
constexpr int kWhite = 4;                    /* the opaque block solid fills read, page 0 */

struct Page {
    Texture *tex = nullptr;
    int shelf_x = 0, shelf_y = 0, shelf_h = 0;   /* packing: rows of glyphs */
    /* Glyphs are written here first, and the region they changed goes to the
     * GPU in one upload, before the first draw that reads it. */
    std::vector<uint8_t> alpha;
    int dirty_x0 = 0, dirty_y0 = 0, dirty_x1 = 0, dirty_y1 = 0;   /* empty when x1 <= x0 */
};
std::vector<Page> s_pages;
int s_page_size = 0;

struct Slot {
    int page = -1;
    int x = 0, y = 0, w = 0, h = 0;
};
std::unordered_map<uint64_t, Slot> s_slots;

int page_size_for(float scale) { return scale > 1.5f ? 2048 : 1024; }

uint64_t slot_key(int weight, int size_key, uint32_t cp)
{
    return ((uint64_t)(weight & 3) << 62) | ((uint64_t)(size_key & 0x3fffffff) << 32) | cp;
}

void mark_dirty(Page &p, int x, int y, int w, int h)
{
    if (p.dirty_x1 <= p.dirty_x0) {
        p.dirty_x0 = x, p.dirty_y0 = y, p.dirty_x1 = x + w, p.dirty_y1 = y + h;
        return;
    }
    p.dirty_x0 = std::min(p.dirty_x0, x), p.dirty_y0 = std::min(p.dirty_y0, y);
    p.dirty_x1 = std::max(p.dirty_x1, x + w), p.dirty_y1 = std::max(p.dirty_y1, y + h);
}

void add_page()
{
    if (s_pages.empty())
        s_page_size = page_size_for(scale());
    const int n = s_page_size;
    Page p;
    /* Its pixels are undefined until uploaded: only uploaded regions (glyphs,
     * the gaps between them, all from the cleared copy) are ever read. */
    p.tex = texture_from_pixels(nullptr, n, n, n);
    p.alpha.assign((size_t)n * n, 0);
    if (s_pages.empty() && p.tex) {
        for (int y = 0; y < kWhite; y++)
            std::fill_n(&p.alpha[(size_t)y * n], kWhite, 255);   /* white, premultiplied */
        mark_dirty(p, 0, 0, kWhite, kWhite);
        set_solid_source(p.tex, (kWhite / 2.f) / n, (kWhite / 2.f) / n);
        p.shelf_x = kWhite + 1;
        p.shelf_h = kWhite;
    }
    s_pages.push_back(std::move(p));
}

/* Room for a w x h glyph (1 px apart, so filtering never bleeds). */
bool place(int w, int h, Slot *out)
{
    if (s_pages.empty())
        add_page();
    if (w + 1 > s_page_size || h + 1 > s_page_size)
        return false;
    Page *p = &s_pages.back();
    if (p->shelf_x + w + 1 > s_page_size) {   /* the row is full: the next one */
        p->shelf_y += p->shelf_h + 1;
        p->shelf_x = 0;
        p->shelf_h = 0;
    }
    if (p->shelf_y + h + 1 > s_page_size) {   /* the page is full: another */
        add_page();
        p = &s_pages.back();
    }
    if (!p->tex)
        return false;
    out->page = (int)s_pages.size() - 1;
    out->x = p->shelf_x, out->y = p->shelf_y, out->w = w, out->h = h;
    p->shelf_x += w + 1;
    p->shelf_h = std::max(p->shelf_h, h);
    return true;
}

/* Sends what changed on a page since its last upload. */
void upload(Page &p)
{
    if (p.dirty_x1 <= p.dirty_x0 || !p.tex)
        return;
    const int w = p.dirty_x1 - p.dirty_x0, h = p.dirty_y1 - p.dirty_y0;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        const uint8_t *src = &p.alpha[(size_t)(p.dirty_y0 + y) * s_page_size + p.dirty_x0];
        uint8_t *dst = &rgba[(size_t)y * w * 4];
        for (int x = 0; x < w; x++)   /* white, premultiplied */
            dst[x * 4] = dst[x * 4 + 1] = dst[x * 4 + 2] = dst[x * 4 + 3] = src[x];
    }
    texture_update(p.tex, p.dirty_x0, p.dirty_y0, w, h, rgba.data(), w);
    p.dirty_x0 = p.dirty_x1 = 0;
}

const Slot *glyph_slot(int weight, int size_key, uint32_t cp)
{
    const uint64_t key = slot_key(weight, size_key, cp);
    auto it = s_slots.find(key);
    if (it != s_slots.end())
        return it->second.page >= 0 ? &it->second : nullptr;
    Slot slot;
    const font::GlyphImage g = font::glyph_image(weight, size_key, cp);
    if (g.alpha && place(g.w, g.h, &slot)) {
        Page &p = s_pages[slot.page];
        for (int y = 0; y < g.h; y++)
            std::copy_n(g.alpha + (size_t)y * g.w, g.w, &p.alpha[(size_t)(slot.y + y) * s_page_size + slot.x]);
        mark_dirty(p, slot.x, slot.y, g.w, g.h);
        perf::cur().glyphs++;
    }
    it = s_slots.emplace(key, slot).first;
    return slot.page >= 0 ? &it->second : nullptr;
}

/* ---- laid-out texts ------------------------------------------------------- */

struct Key {
    std::string s;
    int weight, size10, maxw, lines, line_h10, scale100;
    bool operator==(const Key &o) const
    {
        return weight == o.weight && size10 == o.size10 && maxw == o.maxw && lines == o.lines &&
               line_h10 == o.line_h10 && scale100 == o.scale100 && s == o.s;
    }
};
struct KeyHash {
    size_t operator()(const Key &k) const
    {
        size_t h = std::hash<std::string>()(k.s);
        for (int v : {k.weight, k.size10, k.maxw, k.lines, k.line_h10, k.scale100})
            h = h * 1000003u ^ (size_t)v;
        return h;
    }
};
struct Quad {
    int page;
    int x, y, w, h;   /* output px from the pen's start on the first baseline */
    float u0, v0, u1, v1;
};
struct Entry {
    std::vector<Quad> quads;
    float advance = 0;   /* logical width of the text */
    uint64_t used = 0;   /* the frame it was drawn last */
};
std::unordered_map<Key, Entry, KeyHash> s_text;

Key key_of(const std::string &s, const TextStyle &st)
{
    return {s,
            (int)st.weight,
            (int)std::lround(st.size * 10),
            (int)std::lround(st.max_w),
            st.max_lines,
            (int)std::lround(st.line_h * 10),
            (int)std::lround(scale() * 100)};
}

Entry *entry(const std::string &s, const TextStyle &st)
{
    if (s.empty())
        return nullptr;
    const Key k = key_of(s, st);
    auto it = s_text.find(k);
    if (it == s_text.end()) {
        perf::Frame &pf = perf::cur();
        perf::Timer timer(&pf.text_ms);
        pf.texts++;
        const float sc = scale();
        const float line_h = st.line_h > 0 ? st.line_h : st.size * 1.4f;
        const font::Run run =
            font::layout((int)st.weight, st.size * sc, s, st.max_w * sc, st.max_lines, line_h * sc);
        Entry e;
        e.advance = run.advance / sc;
        e.quads.reserve(run.glyphs.size());
        for (const font::Placed &g : run.glyphs) {
            const Slot *slot = glyph_slot((int)st.weight, run.size_key, g.cp);
            if (!slot)
                continue;
            const float n = (float)s_page_size;
            e.quads.push_back({slot->page, g.x, g.y, slot->w, slot->h, slot->x / n, slot->y / n,
                               (slot->x + slot->w) / n, (slot->y + slot->h) / n});
        }
        for (Page &p : s_pages)
            upload(p);
        it = s_text.emplace(k, std::move(e)).first;
    }
    it->second.used = frame_index();
    return &it->second;
}

uint64_t s_resets = 0;

void reset_atlas()
{
    s_resets++;
    s_text.clear();
    s_slots.clear();
    for (Page &p : s_pages)
        texture_release(p.tex);   /* the solid source goes with page 0 */
    s_pages.clear();
}

} // namespace

float text(float x, float baseline, const std::string &s, const TextStyle &st, uint32_t color, int align)
{
    Entry *e = entry(s, st);
    if (!e)
        return 0;
    float left = x;
    if (align == 1)
        left -= e->advance / 2;
    else if (align == 2)
        left -= e->advance;
    /* On whole output pixels, so glyphs stay as sharp as FreeType drew them. */
    const float sc = scale();
    const float ox = std::round(left * sc), oy = std::round(baseline * sc);
    for (const Quad &q : e->quads)
        image_tint_uv({(ox + q.x) / sc, (oy + q.y) / sc, q.w / sc, q.h / sc}, s_pages[q.page].tex, q.u0, q.v0,
                      q.u1, q.v1, color);
    return e->advance;
}

float text_width(const std::string &s, const TextStyle &st)
{
    if (s.empty())
        return 0;
    if (st.max_lines <= 1 && st.max_w <= 0)
        return font::width((int)st.weight, st.size * scale(), s) / scale();
    Entry *e = entry(s, st);
    return e ? e->advance : 0;
}

size_t text_cache_size() { return s_text.size(); }

void set_atlas_budget(size_t bytes) { s_atlas_budget = bytes; }

uint64_t atlas_resets() { return s_resets; }

void prewarm_glyphs(const std::string &chars, const TextStyle &st) { entry(chars, st); }

void collect()
{
    /* Too many pages (or the output's scale changed): start afresh. The next
     * frame lays out and rasterises what it draws again. */
    const size_t page_bytes = (size_t)s_page_size * s_page_size * 4;
    if (!s_pages.empty() && (s_pages.size() * page_bytes > s_atlas_budget || s_page_size != page_size_for(scale()))) {
        reset_atlas();
        return;
    }
    const uint64_t now = frame_index();
    for (auto it = s_text.begin(); it != s_text.end();)
        it = now - it->second.used > 300 ? s_text.erase(it) : std::next(it);
}

} // namespace gfx
