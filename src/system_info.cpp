#include "system_info.h"

#include <cstdio>

namespace sysinfo {

std::string FormatBytes(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        unit++;
    }
    char buf[64];
    if (unit == 0)
        std::snprintf(buf, sizeof(buf), "%llu %s", (unsigned long long)bytes, units[unit]);
    else
        std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
    return buf;
}

std::string FormatRate(double bytes_per_sec) {
    return FormatBytes(static_cast<uint64_t>(bytes_per_sec > 0 ? bytes_per_sec : 0.0)) + "/s";
}

std::string FormatUptime(double seconds) {
    if (seconds < 0) seconds = 0;
    uint64_t total = static_cast<uint64_t>(seconds);
    uint64_t days = total / 86400;
    uint64_t hours = (total % 86400) / 3600;
    uint64_t mins = (total % 3600) / 60;
    uint64_t secs = total % 60;

    char buf[128];
    if (days > 0)
        std::snprintf(buf, sizeof(buf), "%llud %lluh %llum %llus",
                      (unsigned long long)days, (unsigned long long)hours,
                      (unsigned long long)mins, (unsigned long long)secs);
    else if (hours > 0)
        std::snprintf(buf, sizeof(buf), "%lluh %llum %llus",
                      (unsigned long long)hours, (unsigned long long)mins,
                      (unsigned long long)secs);
    else
        std::snprintf(buf, sizeof(buf), "%llum %llus",
                      (unsigned long long)mins, (unsigned long long)secs);
    return buf;
}

const char* ProcessStatusToString(ProcessStatus status) {
    switch (status) {
        case ProcessStatus::Running:  return "Running";
        case ProcessStatus::Sleeping: return "Sleeping";
        case ProcessStatus::Stopped:  return "Stopped";
        case ProcessStatus::Zombie:   return "Zombie";
        default:                      return "Unknown";
    }
}

} // namespace sysinfo
