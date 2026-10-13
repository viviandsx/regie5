/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * http::Transport on libcurl. One handle per worker thread (connections are
 * kept alive). REGIE5_HTTP_LOG=1 logs each request's method, address, status
 * and time; never a header, so never a key.
 */
#pragma once

#include "api/http.h"

namespace platform_linux {

class CurlTransport : public http::Transport {
public:
    CurlTransport();
    ~CurlTransport() override;
    http::Response perform(const http::Request &req, const std::atomic<bool> *cancel) override;

private:
    bool m_log;
};

} // namespace platform_linux
