/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/scrutiny.h"

#include "api/json.h"

#include <algorithm>
#include <cctype>

namespace scrutiny {

std::string summary_path() { return "/api/summary"; }
std::string settings_path() { return "/api/settings"; }

std::string details_path(const std::string &key)
{
    /* A UUID or a WWN ("0x5000c500..."): nothing else may reach the path. */
    if (key.empty() || key.size() > 64 ||
        !std::all_of(key.begin(), key.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '-'; }))
        return {};
    return "/api/device/" + key + "/details?duration_key=week";
}

bool parse_summary(const std::string &body, std::vector<model::Disk> *out)
{
    json::Doc d(body);
    const cJSON *data = json::get(d.root(), "data");
    const cJSON *summary = json::get(data, "summary");
    if (!cJSON_IsObject(d.root()) || !cJSON_IsObject(summary))
        return false;
    out->clear();
    const cJSON *it;
    cJSON_ArrayForEach(it, summary)
    {
        const cJSON *dev = json::get(it, "device");
        if (!it->string || !cJSON_IsObject(dev) || json::boolean(dev, "archived"))
            continue;
        model::Disk k;
        /* The summary's key is what details asks for: the WWN (0.8) or the scrutiny_uuid (0.9). */
        k.key = it->string;
        k.label = json::str(dev, "label");
        k.device = json::str(dev, "device_name");
        k.capacity = json::i64(dev, "capacity");
        const std::string protocol = json::str(dev, "device_protocol");
        k.kind = protocol == "NVMe" ? model::Disk::Nvme
                 : json::integer(dev, "rotational_speed") == 0 && protocol == "ATA" ? model::Disk::Ssd
                                                                                    : model::Disk::Hdd;
        k.status = json::integer(dev, "device_status");
        const cJSON *smart = json::get(it, "smart");
        if (cJSON_IsObject(smart)) {
            k.has_smart = true;
            k.collected = json::parse_time(json::str(smart, "collector_date"));
            k.temp = json::integer(smart, "temp");
            k.hours = json::i64(smart, "power_on_hours");
        }
        out->push_back(std::move(k));
    }
    std::sort(out->begin(), out->end(), [](const model::Disk &a, const model::Disk &b) {
        return a.device.size() != b.device.size() ? a.device.size() < b.device.size() : a.device < b.device;
    });
    return true;
}

bool parse_test(const std::string &body, std::string *what)
{
    std::vector<model::Disk> disks;
    if (!parse_summary(body, &disks))
        return false;
    *what = std::to_string(disks.size());
    return true;
}

bool parse_settings(const std::string &body, int *threshold)
{
    json::Doc d(body);
    const cJSON *settings = json::get(d.root(), "settings");
    if (!cJSON_IsObject(settings))
        return false;
    const int t = json::integer(json::get(settings, "metrics"), "status_threshold", 3);
    *threshold = t >= 1 && t <= 3 ? t : 3;
    return true;
}

bool parse_details(const std::string &body, model::DiskDetail *out)
{
    json::Doc d(body);
    const cJSON *data = json::get(d.root(), "data");
    const cJSON *results = json::get(data, "smart_results");
    if (!cJSON_IsObject(data) || !cJSON_IsArray(results))
        return false;
    *out = model::DiskDetail{};
    const cJSON *latest = nullptr;
    time_t latest_at = 0;
    const cJSON *r;
    cJSON_ArrayForEach(r, results)
    {
        const time_t at = json::parse_time(json::str(r, "date"));
        if (at == 0)
            continue;
        const int temp = json::integer(r, "temp");
        if (temp > 0)
            out->temps.push_back({at, temp});
        if (!latest || at > latest_at)
            latest = r, latest_at = at;
    }
    /* Scrutiny groups by 24 hours from the moment asked, not by date: two
     * points may fall on one day (the later kept). */
    std::sort(out->temps.begin(), out->temps.end());
    std::vector<std::pair<time_t, int>> daily;
    for (const auto &t : out->temps) {
        if (!daily.empty() && daily.back().first / 86400 == t.first / 86400)
            daily.back() = t;
        else
            daily.push_back(t);
    }
    out->temps.swap(daily);
    if (!latest)
        return true;
    out->power_cycles = json::i64(latest, "power_cycle_count");
    const cJSON *a;
    cJSON_ArrayForEach(a, json::get(latest, "attrs"))
    {
        const int status = json::integer(a, "status");
        if (status == 0 || !a->string)
            continue;
        model::DiskAttribute attr;
        attr.id = a->string;
        attr.status = status;
        /* ATA: the raw count; NVMe and SCSI: their value is it. */
        attr.raw = json::get(a, "raw_value") ? json::i64(a, "raw_value") : json::i64(a, "value");
        out->attributes.push_back(std::move(attr));
    }
    /* Failures first, then by attribute (ATA's by number). */
    std::sort(out->attributes.begin(), out->attributes.end(), [](const model::DiskAttribute &x, const model::DiskAttribute &y) {
        const bool fx = (x.status & 5) != 0, fy = (y.status & 5) != 0;
        if (fx != fy)
            return fx;
        const int nx = json::int_of(x.id, -1), ny = json::int_of(y.id, -1);
        return nx != ny ? nx < ny : x.id < y.id;
    });
    return true;
}

} // namespace scrutiny
