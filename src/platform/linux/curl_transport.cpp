/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/linux/curl_transport.h"

#include "platform/platform.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>

namespace platform_linux {

namespace {

size_t on_body(char *p, size_t size, size_t n, void *ud)
{
    auto *body = static_cast<std::string *>(ud);
    if (body->size() > (64u << 20))
        return 0;   /* far past anything these APIs send: give up */
    body->append(p, size * n);
    return size * n;
}

int on_progress(void *ud, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    const auto *cancel = static_cast<const std::atomic<bool> *>(ud);
    return cancel && cancel->load() ? 1 : 0;
}

struct Handle {
    CURL *curl = curl_easy_init();
    ~Handle() { curl_easy_cleanup(curl); }
};

} // namespace

CurlTransport::CurlTransport() : m_log(std::getenv("REGIE5_HTTP_LOG") != nullptr)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CurlTransport::~CurlTransport() { curl_global_cleanup(); }

http::Response CurlTransport::perform(const http::Request &req, const std::atomic<bool> *cancel)
{
    thread_local Handle h;
    http::Response r;
    CURL *c = h.curl;
    if (!c) {
        r.fail = http::Response::Other;
        r.error = "curl_easy_init failed";
        return r;
    }
    curl_easy_reset(c);
    struct curl_slist *headers = nullptr;
    for (const std::string &hd : req.headers)
        headers = curl_slist_append(headers, hd.c_str());
    if (!req.body.empty())
        headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, (long)req.timeout_s);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, req.insecure ? 0L : 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, req.insecure ? 0L : 2L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_progress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, (void *)cancel);
    if (req.method == "GET") {
        curl_easy_setopt(c, CURLOPT_HTTPGET, 1L);
    } else if (req.method == "POST") {
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
    } else {
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, req.method.c_str());
        if (!req.body.empty())
            curl_easy_setopt(c, CURLOPT_POSTFIELDS, req.body.c_str());
    }

    const double t0 = platform::now();
    const CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(headers);
    if (rc == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        r.status = (int)status;
        const char *ct = nullptr;
        curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct);
        r.content_type = ct ? ct : "";
    } else {
        r.body.clear();
        r.error = curl_easy_strerror(rc);
        r.fail = rc == CURLE_OPERATION_TIMEDOUT ? http::Response::Timeout
                 : (rc == CURLE_COULDNT_CONNECT || rc == CURLE_COULDNT_RESOLVE_HOST ||
                    rc == CURLE_COULDNT_RESOLVE_PROXY)
                     ? http::Response::Connect
                     : http::Response::Other;
    }
    if (m_log) {
        /* The address only: the key travels in a header and is never printed. */
        const std::string shown = req.url.substr(0, req.url.find('?'));
        std::fprintf(stderr, "http: %s %s -> %d%s%s (%.0f ms)\n", req.method.c_str(), shown.c_str(), r.status,
                     r.error.empty() ? "" : " ", r.error.c_str(), (platform::now() - t0) * 1000.0);
    }
    return r;
}

} // namespace platform_linux
