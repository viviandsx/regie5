/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/disks.h"

#include <cstdio>

namespace disks {

Heat heat(const model::Disk &d)
{
    if (d.temp <= 0)
        return Heat::Unknown;
    const bool nvme = d.kind == model::Disk::Nvme;
    if (d.temp >= (nvme ? 80 : 60))
        return Heat::Hot;
    if (d.temp >= (nvme ? 70 : 50))
        return Heat::Warm;
    return Heat::Ok;
}

State state(const model::Disk &d, int threshold)
{
    if (!d.has_smart)
        return State::Unknown;
    switch (d.status & threshold & 3) {
    case 1: return State::FailedSmart;
    case 2: return State::FailedScrutiny;
    case 3: return State::FailedBoth;
    default: return State::Passed;
    }
}

std::vector<Alert> alerts(const model::Disk &d, int threshold, time_t now)
{
    std::vector<Alert> out;
    const State s = state(d, threshold);
    if (s == State::FailedSmart || s == State::FailedBoth)
        out.push_back({Alert::FailedSmart, true});
    if (s == State::FailedScrutiny || s == State::FailedBoth)
        out.push_back({Alert::FailedScrutiny, true});
    const Heat h = heat(d);
    if (h == Heat::Hot)
        out.push_back({Alert::Hot, true});
    else if (h == Heat::Warm)
        out.push_back({Alert::Warm, false});
    if (d.has_smart && d.collected > 0 && now - d.collected > kStale)
        out.push_back({Alert::Stale, false});
    return out;
}

std::string name(const model::Disk &d)
{
    if (!d.label.empty())
        return d.label;
    char buf[160];
    std::snprintf(buf, sizeof buf, i18n::tr(i18n::DiskName), d.device.empty() ? "?" : d.device.c_str());
    return buf;
}

i18n::S attribute_name(const std::string &id)
{
    static const struct {
        const char *id;
        i18n::S name;
    } kAta[] = {{"1", i18n::DkAttrReadErrors},      {"5", i18n::DkAttrReallocated},   {"10", i18n::DkAttrSpinRetry},
                {"184", i18n::DkAttrEndToEnd},      {"187", i18n::DkAttrUncorrectable}, {"188", i18n::DkAttrTimeout},
                {"196", i18n::DkAttrReallocEvents}, {"197", i18n::DkAttrPending},     {"198", i18n::DkAttrOffline},
                {"199", i18n::DkAttrCrc}},
      kOther[] = {{"critical_warning", i18n::DkAttrCritical}, {"media_errors", i18n::DkAttrMediaErrors},
                  {"percentage_used", i18n::DkAttrWear},     {"available_spare", i18n::DkAttrSpare},
                  {"scsi_grown_defect_list", i18n::DkAttrDefects},
                  {"read_total_uncorrected_errors", i18n::DkAttrUncorrectable},
                  {"write_total_uncorrected_errors", i18n::DkAttrUncorrectable}};
    for (const auto &a : kAta)
        if (id == a.id)
            return a.name;
    for (const auto &a : kOther)
        if (id == a.id)
            return a.name;
    return i18n::Count;
}

} // namespace disks
