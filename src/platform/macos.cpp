// macOS backend: libproc for per-process data, Mach host APIs for CPU/RAM,
// IOKit for disk throughput, getifaddrs for network throughput.
#include "system_info.h"

#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sysinfo {

namespace {

using Clock = std::chrono::steady_clock;

int NumCores() {
    static int n = []() {
        int cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
        return cores > 0 ? cores : 1;
    }();
    return n;
}

std::string UsernameForUid(uid_t uid) {
    static std::unordered_map<uid_t, std::string> cache;
    auto it = cache.find(uid);
    if (it != cache.end()) return it->second;
    std::string name;
    struct passwd* pw = getpwuid(uid);
    name = pw ? pw->pw_name : std::to_string(uid);
    cache[uid] = name;
    return name;
}

ProcessStatus MapStatus(int pbi_status) {
    switch (pbi_status) {
        case SRUN:   return ProcessStatus::Running;
        case SSLEEP: return ProcessStatus::Sleeping;
        case SSTOP:  return ProcessStatus::Stopped;
        case SZOMB:  return ProcessStatus::Zombie;
        default:     return ProcessStatus::Unknown;
    }
}

// --- per-process CPU% state (delta of mach-absolute-time ticks) ---
std::unordered_map<pid_t, uint64_t> g_prev_proc_ticks;
uint64_t g_prev_wall_ticks = 0;

// --- host CPU state (delta of tick counters per core) ---
struct CoreTicks { uint32_t user = 0, system = 0, idle = 0, nice = 0; };
std::vector<CoreTicks> g_prev_core_ticks;

// --- disk / network cumulative-counter state ---
uint64_t g_prev_disk_read = 0, g_prev_disk_write = 0;
Clock::time_point g_prev_disk_time{};
uint64_t g_prev_net_recv = 0, g_prev_net_send = 0;
Clock::time_point g_prev_net_time{};

bool GetDiskCumulativeBytes(uint64_t& read_bytes, uint64_t& write_bytes) {
    read_bytes = write_bytes = 0;
    io_iterator_t iter = IO_OBJECT_NULL;
    CFMutableDictionaryRef matching = IOServiceMatching("IOBlockStorageDriver");
    if (!matching) return false;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS)
        return false;

    io_registry_entry_t entry;
    while ((entry = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        CFMutableDictionaryRef props = nullptr;
        if (IORegistryEntryCreateCFProperties(entry, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
            CFDictionaryRef stats = (CFDictionaryRef)CFDictionaryGetValue(
                props, CFSTR(kIOBlockStorageDriverStatisticsKey));
            if (stats) {
                CFNumberRef r = (CFNumberRef)CFDictionaryGetValue(
                    stats, CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey));
                CFNumberRef w = (CFNumberRef)CFDictionaryGetValue(
                    stats, CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey));
                int64_t rv = 0, wv = 0;
                if (r) CFNumberGetValue(r, kCFNumberSInt64Type, &rv);
                if (w) CFNumberGetValue(w, kCFNumberSInt64Type, &wv);
                read_bytes += (uint64_t)rv;
                write_bytes += (uint64_t)wv;
            }
            CFRelease(props);
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(iter);
    return true;
}

bool GetNetCumulativeBytes(uint64_t& recv_bytes, uint64_t& send_bytes) {
    recv_bytes = send_bytes = 0;
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return false;
    for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
        if (std::strncmp(ifa->ifa_name, "lo", 2) == 0) continue;
        if (!ifa->ifa_data) continue;
        auto* data = (struct if_data*)ifa->ifa_data;
        recv_bytes += data->ifi_ibytes;
        send_bytes += data->ifi_obytes;
    }
    freeifaddrs(ifap);
    return true;
}

} // namespace

std::vector<ProcessInfo> GetProcesses() {
    std::vector<ProcessInfo> out;

    int n = proc_listallpids(nullptr, 0);
    if (n <= 0) return out;
    std::vector<pid_t> pids(n + 64);
    n = proc_listallpids(pids.data(), (int)(pids.size() * sizeof(pid_t)));
    if (n <= 0) return out;
    pids.resize(n);

    uint64_t now_ticks = mach_absolute_time();
    uint64_t wall_delta = (g_prev_wall_ticks != 0) ? (now_ticks - g_prev_wall_ticks) : 0;
    std::unordered_map<pid_t, uint64_t> fresh_ticks;
    fresh_ticks.reserve(pids.size());

    out.reserve(pids.size());
    for (pid_t pid : pids) {
        if (pid <= 0) continue;
        struct proc_taskallinfo info;
        int rc = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
        if (rc <= 0) continue;

        ProcessInfo p;
        p.pid = (uint32_t)pid;
        p.ppid = (uint32_t)info.pbsd.pbi_ppid;
        p.name = info.pbsd.pbi_comm[0] ? info.pbsd.pbi_comm : "(unknown)";
        p.memory_bytes = info.ptinfo.pti_resident_size;
        p.status = MapStatus(info.pbsd.pbi_status);
        p.user = UsernameForUid(info.pbsd.pbi_uid);

        uint64_t proc_ticks = info.ptinfo.pti_total_user + info.ptinfo.pti_total_system;
        fresh_ticks[pid] = proc_ticks;
        auto prev_it = g_prev_proc_ticks.find(pid);
        if (prev_it != g_prev_proc_ticks.end() && wall_delta > 0) {
            uint64_t delta = proc_ticks > prev_it->second ? proc_ticks - prev_it->second : 0;
            double pct = 100.0 * (double)delta / (double)wall_delta / NumCores();
            p.cpu_percent = pct;
        }
        out.push_back(std::move(p));
    }

    g_prev_proc_ticks = std::move(fresh_ticks);
    g_prev_wall_ticks = now_ticks;
    return out;
}

CpuSample GetCpuSample() {
    CpuSample sample;
    processor_info_array_t cpu_info = nullptr;
    mach_msg_type_number_t cpu_msg_count = 0;
    natural_t cpu_count = 0;

    kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                            &cpu_count, &cpu_info, &cpu_msg_count);
    if (kr != KERN_SUCCESS) return sample;

    auto* loads = (processor_cpu_load_info_t)cpu_info;
    std::vector<CoreTicks> fresh(cpu_count);
    for (natural_t i = 0; i < cpu_count; ++i) {
        fresh[i].user = loads[i].cpu_ticks[CPU_STATE_USER];
        fresh[i].system = loads[i].cpu_ticks[CPU_STATE_SYSTEM];
        fresh[i].idle = loads[i].cpu_ticks[CPU_STATE_IDLE];
        fresh[i].nice = loads[i].cpu_ticks[CPU_STATE_NICE];
    }

    sample.per_core_percent.resize(cpu_count, 0.0);
    if (g_prev_core_ticks.size() == cpu_count) {
        uint64_t total_used_delta = 0, total_all_delta = 0;
        for (natural_t i = 0; i < cpu_count; ++i) {
            uint64_t used_now = (uint64_t)fresh[i].user + fresh[i].system + fresh[i].nice;
            uint64_t used_prev = (uint64_t)g_prev_core_ticks[i].user + g_prev_core_ticks[i].system + g_prev_core_ticks[i].nice;
            uint64_t all_now = used_now + fresh[i].idle;
            uint64_t all_prev = used_prev + g_prev_core_ticks[i].idle;
            uint64_t used_delta = used_now > used_prev ? used_now - used_prev : 0;
            uint64_t all_delta = all_now > all_prev ? all_now - all_prev : 0;
            sample.per_core_percent[i] = all_delta > 0 ? (100.0 * (double)used_delta / (double)all_delta) : 0.0;
            total_used_delta += used_delta;
            total_all_delta += all_delta;
        }
        sample.overall_percent = total_all_delta > 0 ? (100.0 * (double)total_used_delta / (double)total_all_delta) : 0.0;
    }

    g_prev_core_ticks = std::move(fresh);
    vm_deallocate(mach_task_self(), (vm_address_t)cpu_info, cpu_msg_count * sizeof(integer_t));
    return sample;
}

MemSample GetMemSample() {
    MemSample sample;

    int64_t total = 0;
    size_t len = sizeof(total);
    sysctlbyname("hw.memsize", &total, &len, nullptr, 0);
    sample.total_bytes = (uint64_t)total;

    vm_size_t page_size = 4096;
    host_page_size(mach_host_self(), &page_size);

    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
        uint64_t free_bytes = (uint64_t)vm_stats.free_count * page_size;
        sample.used_bytes = sample.total_bytes > free_bytes ? sample.total_bytes - free_bytes : 0;
    }
    return sample;
}

DiskSample GetDiskSample() {
    DiskSample sample;
    uint64_t read_bytes = 0, write_bytes = 0;
    if (!GetDiskCumulativeBytes(read_bytes, write_bytes)) return sample;

    Clock::time_point now = Clock::now();
    if (g_prev_disk_time.time_since_epoch().count() != 0) {
        double dt = std::chrono::duration<double>(now - g_prev_disk_time).count();
        if (dt > 0.0) {
            sample.read_bytes_per_sec = read_bytes > g_prev_disk_read ? (double)(read_bytes - g_prev_disk_read) / dt : 0.0;
            sample.write_bytes_per_sec = write_bytes > g_prev_disk_write ? (double)(write_bytes - g_prev_disk_write) / dt : 0.0;
        }
    }
    g_prev_disk_read = read_bytes;
    g_prev_disk_write = write_bytes;
    g_prev_disk_time = now;
    return sample;
}

NetSample GetNetSample() {
    NetSample sample;
    uint64_t recv_bytes = 0, send_bytes = 0;
    if (!GetNetCumulativeBytes(recv_bytes, send_bytes)) return sample;

    Clock::time_point now = Clock::now();
    if (g_prev_net_time.time_since_epoch().count() != 0) {
        double dt = std::chrono::duration<double>(now - g_prev_net_time).count();
        if (dt > 0.0) {
            sample.recv_bytes_per_sec = recv_bytes > g_prev_net_recv ? (double)(recv_bytes - g_prev_net_recv) / dt : 0.0;
            sample.send_bytes_per_sec = send_bytes > g_prev_net_send ? (double)(send_bytes - g_prev_net_send) / dt : 0.0;
        }
    }
    g_prev_net_recv = recv_bytes;
    g_prev_net_send = send_bytes;
    g_prev_net_time = now;
    return sample;
}

double GetUptimeSeconds() {
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) != 0) return 0.0;
    time_t now = time(nullptr);
    return difftime(now, boottime.tv_sec);
}

bool KillProcess(uint32_t pid) {
    return kill((pid_t)pid, SIGTERM) == 0;
}

bool KillProcessTree(uint32_t root_pid) {
    int n = proc_listallpids(nullptr, 0);
    if (n <= 0) return KillProcess(root_pid);
    std::vector<pid_t> pids(n + 64);
    n = proc_listallpids(pids.data(), (int)(pids.size() * sizeof(pid_t)));
    pids.resize(n > 0 ? n : 0);

    std::unordered_map<pid_t, std::vector<pid_t>> children;
    for (pid_t pid : pids) {
        if (pid <= 0) continue;
        struct proc_bsdinfo bsd_info;
        if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)) <= 0) continue;
        children[(pid_t)bsd_info.pbi_ppid].push_back(pid);
    }

    std::vector<pid_t> victims;
    std::vector<pid_t> stack{(pid_t)root_pid};
    std::unordered_set<pid_t> seen;
    while (!stack.empty()) {
        pid_t cur = stack.back();
        stack.pop_back();
        if (!seen.insert(cur).second) continue;
        victims.push_back(cur);
        auto it = children.find(cur);
        if (it != children.end())
            for (pid_t c : it->second) stack.push_back(c);
    }

    // Kill leaves first (reverse discovery order approximates that).
    bool all_ok = true;
    for (auto rit = victims.rbegin(); rit != victims.rend(); ++rit)
        if (kill(*rit, SIGTERM) != 0) all_ok = false;
    return all_ok;
}

} // namespace sysinfo
