/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A frame's budgets: new texts past the text budget wait for the next frame
 * (their width is right all the same), pictures past the upload budget too;
 * without a budget, everything in the frame it is asked, as before.
 */
#include "check.h"
#include "gfx_stub.h"
#include "png_maker.h"

#include "api/services.h"
#include "ui/art.h"
#include "ui/font.h"
#include "ui/gfx.h"

#include <chrono>
#include <cmath>
#include <thread>

TEST(texts_past_the_budget_wait_a_frame)
{
    CHECK(font::init());
    gfx::forget_texts();
    const gfx::TextStyle st{gfx::Medium, 40};
    std::vector<std::string> texts;
    for (int i = 0; i < 12; i++)
        texts.push_back("Budget " + std::to_string(i) + " éàçÑØ");
    /* No budget: all in one frame. */
    gfx::set_text_budget(0);
    gfx::begin_frame();
    for (const auto &t : texts)
        gfx::text(0, 0, t, st, 0xffffffffu);
    CHECK_EQ(gfx::text_cache_size(), texts.size());
    CHECK(!gfx::text_waiting());
    /* A tiny budget: one new text a frame, the rest wait; widths right meanwhile. */
    gfx::forget_texts();
    gfx::set_text_budget(0.0001);
    gfx::begin_frame();
    float waiting_w = 0;
    for (const auto &t : texts) {
        const float w = gfx::text(0, 0, t, st, 0xffffffffu);
        if (t == texts.back())
            waiting_w = w;
    }
    CHECK_EQ(gfx::text_cache_size(), (size_t)1);
    CHECK(gfx::text_waiting());
    CHECK(std::fabs(waiting_w - gfx::text_width(texts.back(), st)) < 0.5f);
    int frames = 1;
    while (gfx::text_waiting() && frames < 50) {
        gfx::begin_frame();
        for (const auto &t : texts)
            gfx::text(0, 0, t, st, 0xffffffffu);
        frames++;
    }
    CHECK(gfx::text_cache_size() >= texts.size());
    CHECK(frames >= (int)texts.size() - 1 && frames <= (int)texts.size() + 2);
    gfx::begin_frame();
    CHECK(!gfx::text_waiting());   /* a new frame starts clear */
    gfx::set_text_budget(0);
    gfx::forget_texts();   /* the atlas's pages given back */
}

namespace {

/* Every picture asked: the same small PNG. */
class Flat : public http::Transport {
public:
    http::Response perform(const http::Request &, const std::atomic<bool> *) override
    {
        http::Response r;
        r.status = 200;
        r.body = png;
        return r;
    }
    std::string png;
};

} // namespace

TEST(pictures_past_the_upload_budget_wait_a_frame)
{
    Flat net;
    net.png = png_maker::png(20, 30);
    http::Pool pool(net, 3, nullptr);
    api::Services services(config::mock(), pool, "fr");
    art::init(&services, &pool);
    art::forget(~0u);
    auto refs = [] {
        std::vector<art::Ref> v;
        for (int i = 0; i < 3; i++)
            v.push_back({config::Radarr, "/u/" + std::to_string(i) + ".png", 0});
        return v;
    }();
    auto ask = [&] {
        for (const auto &r : refs)
            art::get(r, 10, 10);
    };
    /* Asked, fetched and decoded (nothing uploaded yet). */
    art::frame();
    ask();
    for (int i = 0; i < 4; i++) {
        art::frame();
        ask();
    }
    for (int i = 0; i < 400 && pool.busy() > 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    pool.pump();
    /* A budget too small for even one: one a frame all the same. */
    art::set_upload_budget(0.000001);
    const int made = gfx_stub::made;
    art::frame();
    CHECK_EQ(gfx_stub::made - made, 1);
    art::frame();
    CHECK_EQ(gfx_stub::made - made, 2);
    art::frame();
    CHECK_EQ(gfx_stub::made - made, 3);
    art::set_upload_budget(0);
    art::forget(~0u);
    art::init(nullptr, nullptr);
}
