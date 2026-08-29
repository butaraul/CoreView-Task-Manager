// Linux backend: everything comes from /proc.
#include "system_info.h"

#include <dirent.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sysinfo {

namespace {

using Clock = std::chrono::steady_clock;

bool IsAllDigits(const char* s) {
    if (!*s) return false;
    for (const char* p = s; *p; ++p)
        if (!isdigit((unsigned char)*p)) return false;
    return true;
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

ProcessStatus MapStatus(char c) {
    switch (c) {
        case 'R': return ProcessStatus::Running;
        case 'S': return ProcessStatus::Sleeping;
        case 'D': return ProcessStatus::Sleeping;
        case 'T': case 't': return ProcessStatus::Stopped;
        case 'Z': return ProcessStatus::Zombie;
        default:  return ProcessStatus::Unknown;
    }
}

// Whole-disk device names only (skip partitions like sda1, nvme0n1p2).
bool IsWholeDisk(const std::string& name) {
    if (name.size() < 3) return false;
    if (name.compare(0, 2, "sd") == 0 || name.compare(0, 2, "hd") == 0 || name.compare(0, 2, "vd") == 0) {
        for (size_t i = 2; i < name.size(); ++i)
            if (!isalpha((unsigned char)name[i])) return false;
        return true;
    }
    if (name.compare(0, 4, "nvme") == 0) {
        // nvme0n1 (whole disk) vs nvme0n1p1 (partition)
        return name.find('p', name.find('n')) == std::string::npos;
    }
    if (name.compare(0, 6, "mmcblk") == 0) {
        return name.find('p') == std::string::npos;
    }
    return false;
}

long ClockTicksPerSec() {
    static long v = sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100;
    return v;
}

int NumCores() {
    static int n = []() {
        long c = sysconf(_SC_NPROCESSORS_ONLN);
        return c > 0 ? (int)c : 1;
    }();
    return n;
}

bool ReadProcStat(pid_t pid, std::string& comm, char& state, int& ppid,
                   unsigned long& utime, unsigned long& stime,
                   unsigned long& rss_pages) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
    if (!f.is_open()) return false;
    std::string line;
    if (!std::getline(f, line)) return false;

    size_t open_paren = line.find('(');
    size_t close_paren = line.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren < open_paren)
        return false;
    comm = line.substr(open_paren + 1, close_paren - open_paren - 1);

    std::istringstream iss(line.substr(close_paren + 2));
    int pgrp, session, tty_nr, tpgid;
    unsigned flags;
    unsigned long minflt, cminflt, majflt, cmajflt, cutime_u, cstime_u;
    long priority, nice_, num_threads, itrealvalue;
    unsigned long long starttime;
    unsigned long vsize;
    iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
        >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime >> cutime_u >> cstime_u
        >> priority >> nice_ >> num_threads >> itrealvalue >> starttime >> vsize >> rss_pages;
    return true;
}

// --- per-process CPU% state ---
std::unordered_map<pid_t, unsigned long> g_prev_proc_ticks;
Clock::time_point g_prev_wall_time{};

// --- host CPU state ---
struct CoreTicks {
    unsigned long long user = 0, nice_ = 0, system_ = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    unsigned long long Total() const { return user + nice_ + system_ + idle + iowait + irq + softirq + steal; }
    unsigned long long Idle() const { return idle + iowait; }
};
std::unordered_map<std::string, CoreTicks> g_prev_cpu_lines;

// --- disk / network cumulative counters ---
uint64_t g_prev_disk_read = 0, g_prev_disk_write = 0;
Clock::time_point g_prev_disk_time{};
uint64_t g_prev_net_recv = 0, g_prev_net_send = 0;
Clock::time_point g_prev_net_time{};

bool ParseCpuLine(const std::string& line, CoreTicks& t) {
    std::istringstream iss(line);
    std::string label;
    iss >> label;
    if (label.compare(0, 3, "cpu") != 0) return false;
    iss >> t.user >> t.nice_ >> t.system_ >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
    return true;
}

} // namespace

std::vector<ProcessInfo> GetProcesses() {
    std::vector<ProcessInfo> out;

    DIR* dir = opendir("/proc");
    if (!dir) return out;

    Clock::time_point now = Clock::now();
    double wall_dt = (g_prev_wall_time.time_since_epoch().count() != 0)
        ? std::chrono::duration<double>(now - g_prev_wall_time).count() : 0.0;
    double wall_ticks_delta = wall_dt * ClockTicksPerSec();

    std::unordered_map<pid_t, unsigned long> fresh_ticks;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (!IsAllDigits(entry->d_name)) continue;
        pid_t pid = (pid_t)atoi(entry->d_name);

        std::string comm; char state; int ppid;
        unsigned long utime, stime, rss_pages;
        if (!ReadProcStat(pid, comm, state, ppid, utime, stime, rss_pages)) continue;

        struct stat st;
        std::string proc_path = "/proc/" + std::to_string(pid);
        uid_t uid = 0;
        if (stat(proc_path.c_str(), &st) == 0) uid = st.st_uid;

        ProcessInfo p;
        p.pid = (uint32_t)pid;
        p.ppid = (uint32_t)ppid;
        p.name = comm;
        p.status = MapStatus(state);
        p.user = UsernameForUid(uid);
        p.memory_bytes = (uint64_t)rss_pages * (uint64_t)sysconf(_SC_PAGESIZE);

        unsigned long total_ticks = utime + stime;
        fresh_ticks[pid] = total_ticks;
        auto prev_it = g_prev_proc_ticks.find(pid);
        if (prev_it != g_prev_proc_ticks.end() && wall_ticks_delta > 0.0) {
            unsigned long delta = total_ticks > prev_it->second ? total_ticks - prev_it->second : 0;
            p.cpu_percent = 100.0 * (double)delta / wall_ticks_delta / NumCores();
        }
        out.push_back(std::move(p));
    }
    closedir(dir);

    g_prev_proc_ticks = std::move(fresh_ticks);
    g_prev_wall_time = now;
    return out;
}

CpuSample GetCpuSample() {
    CpuSample sample;
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return sample;

    std::string line;
    std::vector<std::pair<std::string, CoreTicks>> lines;
    while (std::getline(f, line)) {
        if (line.compare(0, 3, "cpu") != 0) break;
        CoreTicks t;
        if (!ParseCpuLine(line, t)) continue;
        std::string label = line.substr(0, line.find(' '));
        lines.emplace_back(label, t);
    }
    if (lines.empty()) return sample;

    for (size_t i = 1; i < lines.size(); ++i) { // skip index 0 ("cpu" aggregate)
        const std::string& label = lines[i].first;
        const CoreTicks& cur = lines[i].second;
        double pct = 0.0;
        auto prev_it = g_prev_cpu_lines.find(label);
        if (prev_it != g_prev_cpu_lines.end()) {
            unsigned long long total_delta = cur.Total() - prev_it->second.Total();
            unsigned long long idle_delta = cur.Idle() - prev_it->second.Idle();
            if (total_delta > 0) pct = 100.0 * (1.0 - (double)idle_delta / (double)total_delta);
        }
        sample.per_core_percent.push_back(pct);
    }

    {
        const std::string& label = lines[0].first;
        const CoreTicks& cur = lines[0].second;
        auto prev_it = g_prev_cpu_lines.find(label);
        if (prev_it != g_prev_cpu_lines.end()) {
            unsigned long long total_delta = cur.Total() - prev_it->second.Total();
            unsigned long long idle_delta = cur.Idle() - prev_it->second.Idle();
            if (total_delta > 0) sample.overall_percent = 100.0 * (1.0 - (double)idle_delta / (double)total_delta);
        }
    }

    g_prev_cpu_lines.clear();
    for (auto& kv : lines) g_prev_cpu_lines[kv.first] = kv.second;
    return sample;
}

MemSample GetMemSample() {
    MemSample sample;
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return sample;

    std::string line;
    uint64_t mem_total_kb = 0, mem_available_kb = 0;
    bool has_available = false;
    while (std::getline(f, line)) {
        uint64_t value = 0;
        if (sscanf(line.c_str(), "MemTotal: %lu kB", &value) == 1) mem_total_kb = value;
        else if (sscanf(line.c_str(), "MemAvailable: %lu kB", &value) == 1) { mem_available_kb = value; has_available = true; }
    }
    sample.total_bytes = mem_total_kb * 1024ULL;
    if (has_available) {
        uint64_t avail_bytes = mem_available_kb * 1024ULL;
        sample.used_bytes = sample.total_bytes > avail_bytes ? sample.total_bytes - avail_bytes : 0;
    }
    return sample;
}

DiskSample GetDiskSample() {
    DiskSample sample;
    std::ifstream f("/proc/diskstats");
    if (!f.is_open()) return sample;

    uint64_t sectors_read = 0, sectors_written = 0;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        int major, minor;
        std::string name;
        unsigned long r_completed, r_merged, r_sectors, r_time;
        unsigned long w_completed, w_merged, w_sectors, w_time;
        iss >> major >> minor >> name >> r_completed >> r_merged >> r_sectors >> r_time
            >> w_completed >> w_merged >> w_sectors >> w_time;
        if (!IsWholeDisk(name)) continue;
        sectors_read += r_sectors;
        sectors_written += w_sectors;
    }

    uint64_t read_bytes = sectors_read * 512ULL;
    uint64_t write_bytes = sectors_written * 512ULL;

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
    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return sample;

    std::string line;
    std::getline(f, line); // header 1
    std::getline(f, line); // header 2

    uint64_t recv_bytes = 0, send_bytes = 0;
    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string iface = line.substr(0, colon);
        iface.erase(0, iface.find_first_not_of(" \t"));
        if (iface.compare(0, 2, "lo") == 0) continue;

        std::istringstream iss(line.substr(colon + 1));
        uint64_t rbytes, rpackets, rerrs, rdrop, rfifo, rframe, rcompressed, rmulticast;
        uint64_t tbytes;
        iss >> rbytes >> rpackets >> rerrs >> rdrop >> rfifo >> rframe >> rcompressed >> rmulticast >> tbytes;
        recv_bytes += rbytes;
        send_bytes += tbytes;
    }

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
    std::ifstream f("/proc/uptime");
    if (!f.is_open()) return 0.0;
    double uptime = 0.0;
    f >> uptime;
    return uptime;
}

bool KillProcess(uint32_t pid) {
    return kill((pid_t)pid, SIGTERM) == 0;
}

bool KillProcessTree(uint32_t root_pid) {
    DIR* dir = opendir("/proc");
    if (!dir) return KillProcess(root_pid);

    std::unordered_map<pid_t, std::vector<pid_t>> children;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (!IsAllDigits(entry->d_name)) continue;
        pid_t pid = (pid_t)atoi(entry->d_name);
        std::string comm; char state; int ppid;
        unsigned long utime, stime, rss;
        if (!ReadProcStat(pid, comm, state, ppid, utime, stime, rss)) continue;
        children[(pid_t)ppid].push_back(pid);
    }
    closedir(dir);

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

    bool all_ok = true;
    for (auto rit = victims.rbegin(); rit != victims.rend(); ++rit)
        if (kill(*rit, SIGTERM) != 0) all_ok = false;
    return all_ok;
}

} // namespace sysinfo
