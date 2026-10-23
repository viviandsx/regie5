/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Scrutiny (optional): the disks' health from SMART, read-only. Its web API
 * has no authentication; Régie5 only ever GETs. Disks are keyed by their
 * WWN up to 0.8 and by a scrutiny_uuid from 0.9: both are read, as an opaque
 * key (docs/DISKS-STATS.md § 1).
 * https://github.com/AnalogJ/scrutiny (webapp/backend/pkg/web/server.go)
 */
#pragma once

#include "core/model.h"

#include <string>
#include <vector>

namespace scrutiny {

std::string summary_path();    /* every disk, its state and last reading */
std::string settings_path();   /* the web interface's settings: which failures count */
/* A disk's readings of the last week (a day each: temps keeps one point per
 * date), with each attribute's state;
 * empty when the key is not one Scrutiny gives (it goes into the path). */
std::string details_path(const std::string &key);

/* The disks, archived ones left out, by device name. */
bool parse_summary(const std::string &body, std::vector<model::Disk> *out);
/* The connection test: how many disks (as text). */
bool parse_test(const std::string &body, std::string *what);
/* metrics.status_threshold: 1 SMART, 2 Scrutiny, 3 both (the default). */
bool parse_settings(const std::string &body, int *threshold);
bool parse_details(const std::string &body, model::DiskDetail *out);

} // namespace scrutiny
