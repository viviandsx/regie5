/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * --mock: answers every request from fixtures/ instead of a server.
 * http://<service>.mock/<path> reads fixtures/<service>/<file>.json, the file
 * named from the path: the /api/vN/ prefix dropped, lower case, '/' as '_'
 * (/api/v3/system/status -> system_status.json, /Sessions -> sessions.json).
 * Seerr's request list adds its filter (request_pending.json). Actions (POST)
 * are only logged and answered 204: nothing anywhere changes.
 *
 * Pictures: none in fixtures/ (every card is a generated one). A root with an
 * images.json (demo/) maps picture_for()'s keys to files in its images/
 * folder: Sonarr/Radarr mediacover, Jellyfin item images, Seerr's proxy.
 *
 * For trying the interface's states:
 *   REGIE5_MOCK_DELAY_MS=800              every answer that late (default 250)
 *   REGIE5_MOCK_FAIL=radarr:unreachable,seerr:unauthorized
 *                                        (unreachable, timeout, unauthorized,
 *                                         notfound, servererror, badjson)
 *   REGIE5_MOCK_EMPTY=sonarr,seerr        their lists answer empty (same shape)
 *   REGIE5_MOCK_OFF=jellyfin              off, as if "enabled": false
 *   REGIE5_MOCK_MULTI=1                   a second Radarr, "Radarr 4K", under
 *                                        http://radarr.mock/radarr4k
 *                                        (both read by the shell, see config::mock)
 *   REGIE5_MOCK_CLEANUP=1                 the Ménage already turned on (read by
 *                                        the shell; in memory, as every --mock setting)
 */
#pragma once

#include "api/http.h"

#include <map>
#include <string>

namespace api {

class MockTransport : public http::Transport {
public:
    /* root: the fixtures directory. fail: REGIE5_MOCK_FAIL's syntax. empty:
     * REGIE5_MOCK_EMPTY's, services separated by commas. */
    MockTransport(std::string root, int delay_ms, const std::string &fail, const std::string &empty = {});
    http::Response perform(const http::Request &req, const std::atomic<bool> *cancel) override;

    /* The fixture file a URL maps to, relative to root ("" when none). Public for the tests. */
    static std::string file_for(const std::string &url);
    /* A fixture's body with its lists emptied (arrays [], counts 0); the
     * connection tests' answers and Seerr's media details are kept. */
    static std::string emptied(const std::string &rel, const std::string &body);
    /* The key of a picture's URL in images.json ("" when the URL is not a
     * picture): radarr/mediacover/12/poster (any size), jellyfin/items/<id>/primary,
     * seerr/<file> (any size). Public for the tests. */
    static std::string picture_for(const std::string &url);

private:
    std::string m_root;
    int m_delay_ms;
    std::map<std::string, std::string> m_fail;   /* service -> failure */
    std::map<std::string, bool> m_empty;         /* services whose lists are empty */
    std::map<std::string, std::string> m_pictures;   /* images.json: key -> file in images/ */
};

} // namespace api
