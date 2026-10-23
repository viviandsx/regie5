/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What a disk's reading means (Scrutiny, optional): its state as Scrutiny's
 * own page says it, a temperature too high by Régie5's thresholds (Scrutiny
 * has none), a reading that stopped coming. docs/DISKS-STATS.md § 1.6.
 */
#pragma once

#include "core/i18n.h"
#include "core/model.h"

#include <ctime>
#include <string>
#include <vector>

namespace disks {

/* Temperatures: a hard disk or a SATA SSD warm from 50 °C, hot from 60;
 * an NVMe SSD from 70 and 80. */
enum class Heat { Unknown, Ok, Warm, Hot };
Heat heat(const model::Disk &d);

/* A reading older than this: the collector stopped. */
constexpr time_t kStale = 48 * 3600;

/* Scrutiny's state for the disk, by the threshold its settings choose (as
 * its web page: device_status & threshold). */
enum class State { Unknown, Passed, FailedSmart, FailedScrutiny, FailedBoth };
State state(const model::Disk &d, int threshold);

/* What is wrong with a disk, most serious first. */
struct Alert {
    enum Kind { FailedSmart, FailedScrutiny, Hot, Warm, Stale } kind;
    bool error;   /* red; else orange */
};
std::vector<Alert> alerts(const model::Disk &d, int threshold, time_t now);

/* "Disque sda", or the name its owner gave it. */
std::string name(const model::Disk &d);

/* An attribute's name in the interface's words; S Count when Régie5 does
 * not name it (then "Attribut n° 9"). */
i18n::S attribute_name(const std::string &id);

} // namespace disks
