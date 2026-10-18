/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Big libraries: lists read one element at a time, the size limit and its
 * message, the lists shared between screens.
 */
#include "check.h"

#include "api/arr.h"
#include "api/http_wire.h"
#include "api/jellyfin.h"
#include "api/json.h"
#include "api/mock_transport.h"
#include "api/plex.h"
#include "api/services.h"
#include "ui/arr_lists.h"

#include <chrono>
#include <thread>

using check::fixture;

namespace {

std::vector<int> ids(const std::string &text, std::initializer_list<const char *> path, json::Each *how)
{
    std::vector<int> out;
    *how = json::each(text, path, [&out](const cJSON *e) { out.push_back(json::integer(e, "id", -1)); });
    return out;
}

void drain(http::Pool &pool)
{
    for (int i = 0; i < 400 && (pool.busy() > 0 || pool.pump() > 0); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    pool.pump();
}

} // namespace

TEST(json_each_reads_a_list_one_element_at_a_time)
{
    json::Each how;
    CHECK(ids(R"( [ {"id":1}, {"id":2,"x":[1,{"y":"]}"}]} ,{"id":3} ] )", {}, &how) == std::vector<int>({1, 2, 3}));
    CHECK(how == json::Each::Done);
    CHECK(ids("[]", {}, &how).empty() && how == json::Each::Done);
    /* Strings with brackets, quotes and escapes do not end an element. */
    CHECK(ids(R"([{"id":4,"t":"a \"[\" {"},{"id":5,"t":"\\"}])", {}, &how) == std::vector<int>({4, 5}));
    CHECK(how == json::Each::Done);
    /* A member path, other members skipped (before and after). */
    const std::string jf = R"({"Before":{"a":[1,2]},"Items":[{"id":7},{"id":8}],"TotalRecordCount":2})";
    CHECK(ids(jf, {"Items"}, &how) == std::vector<int>({7, 8}) && how == json::Each::Done);
    CHECK(ids(R"({"MediaContainer":{"size":1,"Metadata":[{"id":9}]}})", {"MediaContainer", "Metadata"}, &how) ==
          std::vector<int>({9}));
    CHECK(how == json::Each::Done);
    /* A lone object for the list (some Plex versions): one element. */
    CHECK(ids(R"({"MediaContainer":{"Metadata":{"id":10}}})", {"MediaContainer", "Metadata"}, &how) ==
          std::vector<int>({10}));
    /* The last member absent: Missing; one on the way absent: Bad. */
    CHECK(ids(R"({"MediaContainer":{"size":0}})", {"MediaContainer", "Metadata"}, &how).empty());
    CHECK(how == json::Each::Missing);
    ids(R"({"Other":{}})", {"MediaContainer", "Metadata"}, &how);
    CHECK(how == json::Each::Bad);
    /* Not a list: an error object, cut short, trailing text, a broken element. */
    for (const char *bad : {R"({"message":"Unauthorized"})", R"([{"id":1},{"id":2)", R"([{"id":1}] x)",
                            R"([{"id":1},{"id":})", "", "<html>"}) {
        ids(bad, {}, &how);
        CHECK(how == json::Each::Bad);
    }
    ids(R"({"Items":[{"id":1}],)", {"Items"}, &how);   /* cut short after the list */
    CHECK(how == json::Each::Bad);
}

TEST(big_lists_parse_without_a_whole_tree)
{
    /* Same answers as before json::each, on the fixtures. */
    std::vector<model::Movie> movies;
    CHECK(arr::parse_movies(fixture("radarr/movie.json"), &movies));
    CHECK(movies.size() > 100);
    std::vector<model::Series> series;
    CHECK(arr::parse_series(fixture("sonarr/series.json"), &series));
    CHECK(!series.empty() && !series[0].seasons.empty());
    CHECK(!arr::parse_movies(R"({"message":"Unauthorized"})", &movies));
    CHECK(!arr::parse_series("[{\"id\":1},", &series));
    /* A synthetic library of 3 000 films, each element alone. */
    std::string big = "[";
    for (int i = 1; i <= 3000; i++)
        big += (i > 1 ? "," : "") + std::string(R"({"id":)") + std::to_string(i) +
               R"(,"title":"T","sizeOnDisk":10,"alternateTitles":[{"title":"x"}],"movieFile":{"quality":{"quality":{"name":"Q"}}}})";
    big += "]";
    CHECK(arr::parse_movies(big, &movies));
    CHECK_EQ(movies.size(), (size_t)3000);
    CHECK(movies.back().id == 3000 && movies.back().file_quality == "Q");
    /* Jellyfin's and Plex's whole libraries. */
    std::vector<model::UserItem> items;
    CHECK(jellyfin::parse_user_items(R"({"Items":[{"Id":"a","Type":"Series"},{"Id":"b"}],"TotalRecordCount":2})", &items));
    CHECK(items.size() == 2 && items[0].series && items[1].id == "b");
    CHECK(!jellyfin::parse_user_items(R"({"TotalRecordCount":0})", &items));
    CHECK(plex::parse_section_items(R"({"MediaContainer":{"size":0}})", &items) && items.empty());
    CHECK(!plex::parse_section_items(R"({"error":"x"})", &items));
}

TEST(lists_past_the_limit_are_too_large)
{
    /* The reader gives up past max_body, sized or chunked. */
    http::wire::Reader sized(false, 4);
    const std::string a = "HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\n[1,2,3,4]";
    CHECK(sized.feed(a.data(), a.size()) == http::wire::Reader::Bad && sized.too_large);
    http::wire::Reader chunked(false, 4);
    const std::string b = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\n[1,\r\n3\r\n2,3\r\n0\r\n\r\n";
    CHECK(chunked.feed(b.data(), b.size()) == http::wire::Reader::Bad && chunked.too_large);
    http::wire::Reader fits(false, 64);
    CHECK(fits.feed(a.data(), a.size()) == http::wire::Reader::Done && !fits.too_large);

    http::Response r;
    r.fail = http::Response::TooLarge;
    CHECK(api::classify(r, false) == api::Error::TooLarge);

    /* Through the services: the films past the limit, everything else as usual. */
    const size_t before = api::list_limit();
    api::set_list_limit(200u << 10);   /* the fixture's films weigh more, its series less */
    api::MockTransport mock(REGIE5_FIXTURES, 0, "");
    http::Pool pool(mock, 2, nullptr);
    api::Services s(config::mock(), pool, "fr");
    api::Result<std::vector<model::Movie>> films;
    api::Result<std::vector<model::Series>> shows;
    api::Result<std::vector<model::QueueItem>> queue;
    s.movies([&](api::Result<std::vector<model::Movie>> x) { films = x; });
    s.series([&](api::Result<std::vector<model::Series>> x) { shows = x; });
    s.queue(config::Radarr, [&](api::Result<std::vector<model::QueueItem>> x) { queue = x; });
    drain(pool);
    CHECK(films.error == api::Error::TooLarge && films.value.empty());
    CHECK(shows.ok());
    CHECK(queue.ok());
    api::set_list_limit(before);

    /* REGIE5_MOCK_FAIL=radarr:toolarge: only the lists. */
    api::MockTransport failing(REGIE5_FIXTURES, 0, "radarr:toolarge");
    http::Pool pool2(failing, 2, nullptr);
    api::Services s2(config::mock(), pool2, "fr");
    queue = {};
    s2.movies([&](api::Result<std::vector<model::Movie>> x) { films = x; });
    s2.queue(config::Radarr, [&](api::Result<std::vector<model::QueueItem>> x) { queue = x; });
    drain(pool2);
    CHECK(films.error == api::Error::TooLarge);
    CHECK(queue.ok());
}

TEST(shared_lists_join_without_copies)
{
    using ui::Part;
    using ui::Shared;
    auto list = [](std::initializer_list<int> ids, int instance) {
        auto v = std::make_shared<std::vector<model::Movie>>();
        for (int id : ids) {
            model::Movie m;
            m.id = id, m.instance = instance;
            v->push_back(m);
        }
        return Shared<model::Movie>(v);
    };
    std::vector<Part<Shared<model::Movie>>> parts(2);
    parts[0].instance = 0, parts[1].instance = 1;
    parts[0].load.state = ui::load::Ready, parts[0].load.value = list({1, 2}, 0);
    parts[1].load.state = ui::load::Loading;
    ui::Load<Shared<model::Movie>> joined;
    ui::join_shared(parts, &joined);
    CHECK(joined.ready());
    CHECK(joined.value == parts[0].load.value);   /* the same list, not a copy */
    parts[1].load.state = ui::load::Ready, parts[1].load.value = list({3}, 1);
    ui::join_shared(parts, &joined);
    CHECK_EQ(ui::list_of(joined).size(), (size_t)3);
    CHECK(ui::list_of(joined)[2].id == 3 && ui::list_of(joined)[2].instance == 1);
    parts[0].load.state = ui::load::Failed, parts[1].load.state = ui::load::Failed;
    ui::join_shared(parts, &joined);
    CHECK(joined.state == ui::load::Failed && ui::list_of(joined).empty());
}
