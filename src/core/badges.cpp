/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/badges.h"

namespace badges {

Badge queue(const std::vector<model::QueueItem> &items)
{
    Badge b{(int)items.size(), Neutral};
    for (const model::QueueItem &q : items)
        if (q.status == model::QueueItem::Error)
            b.tone = Problem;
        else if (q.status == model::QueueItem::Warning && b.tone != Problem)
            b.tone = Warning;
    return b;
}

Badge requests(const model::RequestPage &pending)
{
    /* total is the filter's count over every page; a page alone when it is missing. */
    const int n = pending.total > 0 ? pending.total : (int)pending.requests.size();
    return {n, Attention};
}

Badge sessions(const std::vector<model::Session> &playing) { return {(int)playing.size(), Live}; }

std::string label(int count) { return count > 99 ? "99+" : std::to_string(count); }

} // namespace badges
