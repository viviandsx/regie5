/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Configuring from a phone: the page's view and edits (core/config_web), and
 * the real server on the loopback (platform/posix/config_server), asked with
 * the PS5's own HTTP client.
 */
#include "check.h"

#include "core/config.h"
#include "core/config_web.h"
#include "platform/platform.h"
#include "platform/posix/socket_transport.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

const char *kConfig = R"({
  "_comment": ["kept"],
  "sonarr": {"url": "http://10.0.0.2:8989", "apiKey": "SONARRSECRET", "is4k": false},
  "radarr": [
    {"name": "Radarr", "url": "http://10.0.0.2:7878", "apiKey": "RADARRSECRET"},
    {"name": "Radarr 4K", "url": "http://10.0.0.3:7878", "apiKey": "RADARR4KSECRET", "is4k": true}
  ],
  "glances": {"url": "http://10.0.0.2:61208", "username": "u", "password": "GLANCESSECRET"},
  "cleanup": {"ignoreUsers": ["guest"]}
})";

std::string slurp(const std::string &path)
{
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

} // namespace

TEST(share_view_never_holds_secrets)
{
    const std::string v = config_web::public_view(kConfig);
    for (const char *secret : {"SONARRSECRET", "RADARRSECRET", "RADARR4KSECRET", "GLANCESSECRET"})
        CHECK(v.find(secret) == std::string::npos);
    CHECK(v.find("\"hasKey\":true") != std::string::npos);
    CHECK(v.find("Radarr 4K") != std::string::npos);
    CHECK(v.find("\"hasPassword\":true") != std::string::npos);
    CHECK(config_web::public_view("not json").find("\"sonarr\":[]") != std::string::npos);
}

TEST(share_apply_keeps_what_it_is_not_told)
{
    /* Radarr 4K's address changes, its key is kept; Sonarr gets a new key;
     * Radarr (the first) is dropped; Jellyfin is added. */
    const std::string posted = R"({"services":{
      "sonarr":[{"from":0,"url":"http://10.0.0.2:8989","apiKey":"NEWSONARR","enabled":true}],
      "radarr":[{"from":1,"name":"Radarr 4K","url":"http://10.0.0.4:7878","apiKey":"","is4k":true,"enabled":true}],
      "jellyfin":[{"from":-1,"url":"http://10.0.0.2:8096","apiKey":"JELLY","enabled":true}],
      "glances":[{"from":0,"url":"http://10.0.0.2:61208","username":"u","password":"","enabled":false}]
    }})";
    std::string out, error;
    CHECK(config_web::apply(kConfig, posted, &out, &error));
    config::Config c;
    CHECK(config::parse(out, &c));
    CHECK_EQ(c.services[config::Sonarr].api_key, std::string("NEWSONARR"));
    CHECK_EQ(c.count(config::Radarr), 1);
    CHECK_EQ(c.services[config::Radarr].url, std::string("http://10.0.0.4:7878"));
    CHECK_EQ(c.services[config::Radarr].api_key, std::string("RADARR4KSECRET"));
    CHECK(c.services[config::Radarr].is4k);
    CHECK_EQ(c.services[config::Jellyfin].api_key, std::string("JELLY"));
    CHECK_EQ(c.services[config::Glances].password, std::string("GLANCESSECRET"));
    CHECK(!c.services[config::Glances].enabled);
    CHECK_EQ(c.ignore_users.size(), (size_t)1);
    CHECK(out.find("_comment") != std::string::npos);

    CHECK(!config_web::apply(kConfig, "[]", &out, &error));
    CHECK(!config_web::apply(kConfig, R"({"services":{"seerr":[{"url":"ftp://x"}]}})", &out, &error));
    CHECK(config_web::apply("", R"({"services":{"seerr":[{"url":"http://1.2.3.4:5055","apiKey":"K"}]}})", &out,
                            &error));
    /* https with a self-signed certificate: "insecure" written, and read back. */
    CHECK(config_web::apply("", R"({"services":{"jellyfin":[{"url":"https://media.lan","apiKey":"K","insecure":true}]}})",
                            &out, &error));
    CHECK(config::parse(out, &c));
    CHECK(c.services[config::Jellyfin].insecure);
    CHECK(config_web::public_view(out).find("\"insecure\":true") != std::string::npos);
}

TEST(share_local_addresses_only)
{
    for (const char *ip : {"127.0.0.1", "10.1.2.3", "172.16.0.1", "172.31.255.255", "192.168.1.20", "169.254.3.4",
                           "::1", "fd00::5", "::ffff:192.168.1.2"})
        CHECK(config_web::is_local_address(ip));
    for (const char *ip : {"8.8.8.8", "172.32.0.1", "192.169.1.1", "2001:db8::1", "::ffff:8.8.8.8", ""})
        CHECK(!config_web::is_local_address(ip));
}

TEST(share_server_on_the_loopback)
{
    const auto dir = std::filesystem::temp_directory_path() /
                     ("regie5-share-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "config.json").string();
    std::ofstream(path) << kConfig;

    std::string error;
    CHECK(platform::share_start(path, "127.0.0.1", &error));
    const platform::ShareInfo info = platform::share_info();
    CHECK(info.running);
    CHECK_EQ(info.code.size(), (size_t)6);
    CHECK(info.address.rfind("http://127.0.0.1:", 0) == 0);

    platform_posix::SocketTransport http;
    http::Request req;
    req.url = info.shown + "/";
    http::Response r = http.perform(req, nullptr);
    CHECK_EQ(r.status, 200);
    CHECK(r.body.find("<html lang=\"fr\"") != std::string::npos);
    CHECK(r.body.find("const T = {lead:\"Adresses et clés") != std::string::npos);   /* the texts, from fr.cpp */
    CHECK(r.body.find("__") == std::string::npos);                                  /* nothing left to fill */

    req.url = info.shown + "/api/config?c=000000" + (info.code == "000000" ? "1" : "");
    CHECK_EQ(http.perform(req, nullptr).status, 403);

    req.url = info.shown + "/api/config?c=" + info.code;
    r = http.perform(req, nullptr);
    CHECK_EQ(r.status, 200);
    CHECK(r.body.find("SONARRSECRET") == std::string::npos);

    req.method = "POST";
    req.body = R"({"services":{"sonarr":[{"from":0,"url":"http://10.0.0.9:8989","apiKey":"","enabled":true}]}})";
    r = http.perform(req, nullptr);
    CHECK_EQ(r.status, 200);
    CHECK_EQ(platform::share_info().saves, 1);
    config::Config c;
    CHECK(config::parse(slurp(path), &c));
    CHECK_EQ(c.services[config::Sonarr].url, std::string("http://10.0.0.9:8989"));
    CHECK_EQ(c.services[config::Sonarr].api_key, std::string("SONARRSECRET"));
    CHECK_EQ(c.count(config::Radarr), 2);   /* not sent: untouched */

    platform::share_stop();
    CHECK(!platform::share_info().running);
    std::filesystem::remove_all(dir);
}
