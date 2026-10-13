/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The counters on the tabs: how many downloads in the queue, requests waiting
 * for an answer, people watching. Worked out from what the services answer;
 * no badge when the count is unknown (a service off or not answering) or 0.
 */
#pragma once

#include "core/model.h"

#include <string>
#include <vector>

namespace badges {

/* Neutral: something is going on. Attention: someone waits for the user
 * (a request). Live: someone is watching. Warning / Problem: in the queue. */
enum Tone { Neutral, Attention, Live, Warning, Problem };

struct Badge {
    int count = -1;   /* < 0: unknown, nothing is shown */
    Tone tone = Neutral;

    bool shown() const { return count > 0; }
};

/* The queue of every instance: the count, the worst status for the tone. */
Badge queue(const std::vector<model::QueueItem> &items);
/* Seerr's pending requests: the total of the pending filter (every page). */
Badge requests(const model::RequestPage &pending);
/* Jellyfin's sessions playing something (paused ones included). */
Badge sessions(const std::vector<model::Session> &playing);

/* "7", "99+". */
std::string label(int count);

} // namespace badges
