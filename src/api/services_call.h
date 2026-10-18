/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Services::call, for api/services.cpp and api/media_server.cpp alone (not
 * a public header): a request built, run on the pool, parsed on the worker,
 * its result handed to the main thread.
 */
#pragma once

#include "api/media_server.h"
#include "api/services.h"

#include <memory>

namespace api {

/* An action's answer: any 2xx is enough. */
inline bool accept_any(const std::string &, bool *out)
{
    *out = true;
    return true;
}

template <typename T>
void Services::call(config::Service s, const std::string &method, const std::string &path,
                    std::function<bool(const std::string &, T *)> parse, std::function<void(Result<T>)> done,
                    int instance, const std::string &body, bool need_on, size_t max_body, bool fresh)
{
    if (need_on ? !enabled(s, instance) : m_cfg.at(s, instance).url.empty()) {
        Result<T> res;
        res.error = Error::Disabled;
        if (done)
            done(std::move(res));
        return;
    }
    http::Request req = make(s, method, path, instance);
    req.body = body;
    req.max_body = max_body;
    req.fresh = fresh;
    /* The media server's own reading of a refusal (an ended session, not
     * the owner...: api/media_server). */
    std::function<Error(int)> refusals = s == config::Jellyfin ? media().refusals(m_cfg.at(s, instance)) : nullptr;
    m_pool.submit(std::move(req), [parse, done, refusals](http::Response &resp) -> std::function<void()> {
        auto res = std::make_shared<Result<T>>();
        res->http_status = resp.status;
        const bool parsed = resp.ok() && parse(resp.body, &res->value);
        res->error = classify(resp, parsed);
        if (refusals) {
            const Error e = refusals(resp.status);
            if (e != Error::None)
                res->error = e;
        }
        return [done, res] {
            if (done)
                done(std::move(*res));
        };
    });
}

} // namespace api
