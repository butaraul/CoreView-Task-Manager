// Windows backend: tlhelp32 for the process list, psapi for per-process
// memory/CPU, PDH for system-wide CPU/disk/network counters.
#include "system_info.h"

#include <windows.h>

#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <chrono>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace sysinfo {

namespace {

using Clock = std::chrono::steady_clock;

uint64_t FileTimeToU64(const FILETIME& ft) {
    return (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

int NumCores() {
    static int n = []() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
    }();
    return n;
}

std::string UsernameForProcess(HANDLE process) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return "";

    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<BYTE> buf(needed);
    std::string result;
    if (GetTokenInformation(token, TokenUser, buf.data(), needed, &needed)) {
        auto* user = reinterpret_cast<TOKEN_USER*>(buf.data());
        char name[256] = {};
        char domain[256] = {};
        DWORD name_len = sizeof(name), domain_len = sizeof(domain);
        SID_NAME_USE use;
        if (LookupAccountSidA(nullptr, user->User.Sid, name, &name_len, domain, &domain_len, &use)) {
            result = name;
        }
    }
    CloseHandle(token);
    return result;
}

// --- per-process CPU% state ---
std::unordered_map<DWORD, uint64_t> g_prev_proc_ticks; // kernel+user 100ns units
Clock::time_point g_prev_wall_time{};

// --- Lazy-initialized PDH counter sets ---
struct CpuCounters {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER total = nullptr;
    PDH_HCOUNTER per_core = nullptr;
    bool ready = false;
};
CpuCounters g_cpu;

struct DiskCounters {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER read_bytes = nullptr;
    PDH_HCOUNTER write_bytes = nullptr;
    bool ready = false;
};
DiskCounters g_disk;

struct NetCounters {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER recv_bytes = nullptr;
    PDH_HCOUNTER send_bytes = nullptr;
    bool ready = false;
};
NetCounters g_net;

void InitCpuCounters() {
    if (PdhOpenQueryA(nullptr, 0, &g_cpu.query) != ERROR_SUCCESS) return;
    PdhAddEnglishCounterA(g_cpu.query, "\\Processor(_Total)\\% Processor Time", 0, &g_cpu.total);
    PdhAddEnglishCounterA(g_cpu.query, "\\Processor(*)\\% Processor Time", 0, &g_cpu.per_core);
    PdhCollectQueryData(g_cpu.query);
    g_cpu.ready = true;
}

void InitDiskCounters() {
    if (PdhOpenQueryA(nullptr, 0, &g_disk.query) != ERROR_SUCCESS) return;
    PdhAddEnglishCounterA(g_disk.query, "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &g_disk.read_bytes);
    PdhAddEnglishCounterA(g_disk.query, "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &g_disk.write_bytes);
    PdhCollectQueryData(g_disk.query);
    g_disk.ready = true;
}

void InitNetCounters() {
    if (PdhOpenQueryA(nullptr, 0, &g_net.query) != ERROR_SUCCESS) return;
    PdhAddEnglishCounterA(g_net.query, "\\Network Interface(*)\\Bytes Received/sec", 0, &g_net.recv_bytes);
    PdhAddEnglishCounterA(g_net.query, "\\Network Interface(*)\\Bytes Sent/sec", 0, &g_net.send_bytes);
    PdhCollectQueryData(g_net.query);
    g_net.ready = true;
}

double SumFormattedArray(PDH_HCOUNTER counter) {
    DWORD buf_size = 0, item_count = 0;
    PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE, &buf_size, &item_count, nullptr);
    if (buf_size == 0) return 0.0;
    std::vector<BYTE> buf(buf_size);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_A*>(buf.data());
    if (PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE, &buf_size, &item_count, items) != ERROR_SUCCESS)
        return 0.0;
    double sum = 0.0;
    for (DWORD i = 0; i < item_count; ++i) {
        if (std::strstr(items[i].szName, "Loopback") != nullptr) continue;
        if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
            sum += items[i].FmtValue.doubleValue;
    }
    return sum;
}

} // namespace

std::vector<ProcessInfo> GetProcesses() {
    std::vector<ProcessInfo> out;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;

    Clock::time_point now = Clock::now();
    double wall_dt = (g_prev_wall_time.time_since_epoch().count() != 0)
        ? std::chrono::duration<double>(now - g_prev_wall_time).count() : 0.0;
    uint64_t wall_100ns_delta = (uint64_t)(wall_dt * 1e7);

    std::unordered_map<DWORD, uint64_t> fresh_ticks;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            ProcessInfo p;
            p.pid = pe.th32ProcessID;
            p.ppid = pe.th32ParentProcessID;
            p.name = pe.szExeFile;
            p.status = ProcessStatus::Running;

            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, p.pid);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
                    p.memory_bytes = pmc.WorkingSetSize;

                FILETIME creation, exit, kernel, user;
                if (GetProcessTimes(h, &creation, &exit, &kernel, &user)) {
                    uint64_t total = FileTimeToU64(kernel) + FileTimeToU64(user);
                    fresh_ticks[p.pid] = total;
                    auto prev_it = g_prev_proc_ticks.find(p.pid);
                    if (prev_it != g_prev_proc_ticks.end() && wall_100ns_delta > 0) {
                        uint64_t delta = total > prev_it->second ? total - prev_it->second : 0;
                        p.cpu_percent = 100.0 * (double)delta / (double)wall_100ns_delta / NumCores();
                    }
                }
                p.user = UsernameForProcess(h);
                CloseHandle(h);
            } else {
                p.status = ProcessStatus::Unknown;
            }

            out.push_back(std::move(p));
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);

    g_prev_proc_ticks = std::move(fresh_ticks);
    g_prev_wall_time = now;
    return out;
}

CpuSample GetCpuSample() {
    CpuSample sample;
    if (!g_cpu.ready) InitCpuCounters();
    if (!g_cpu.ready) return sample;

    if (PdhCollectQueryData(g_cpu.query) != ERROR_SUCCESS) return sample;

    PDH_FMT_COUNTERVALUE total_val;
    if (PdhGetFormattedCounterValue(g_cpu.total, PDH_FMT_DOUBLE, nullptr, &total_val) == ERROR_SUCCESS &&
        total_val.CStatus == PDH_CSTATUS_VALID_DATA) {
        sample.overall_percent = total_val.doubleValue;
    }

    DWORD buf_size = 0, item_count = 0;
    PdhGetFormattedCounterArrayA(g_cpu.per_core, PDH_FMT_DOUBLE, &buf_size, &item_count, nullptr);
    if (buf_size > 0) {
        std::vector<BYTE> buf(buf_size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_A*>(buf.data());
        if (PdhGetFormattedCounterArrayA(g_cpu.per_core, PDH_FMT_DOUBLE, &buf_size, &item_count, items) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < item_count; ++i) {
                if (std::strcmp(items[i].szName, "_Total") == 0) continue;
                if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
                    sample.per_core_percent.push_back(items[i].FmtValue.doubleValue);
            }
        }
    }
    return sample;
}

MemSample GetMemSample() {
    MemSample sample;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        sample.total_bytes = statex.ullTotalPhys;
        sample.used_bytes = statex.ullTotalPhys - statex.ullAvailPhys;
    }
    return sample;
}

DiskSample GetDiskSample() {
    DiskSample sample;
    if (!g_disk.ready) InitDiskCounters();
    if (!g_disk.ready) return sample;

    if (PdhCollectQueryData(g_disk.query) != ERROR_SUCCESS) return sample;

    PDH_FMT_COUNTERVALUE read_val, write_val;
    if (PdhGetFormattedCounterValue(g_disk.read_bytes, PDH_FMT_DOUBLE, nullptr, &read_val) == ERROR_SUCCESS &&
        read_val.CStatus == PDH_CSTATUS_VALID_DATA)
        sample.read_bytes_per_sec = read_val.doubleValue;
    if (PdhGetFormattedCounterValue(g_disk.write_bytes, PDH_FMT_DOUBLE, nullptr, &write_val) == ERROR_SUCCESS &&
        write_val.CStatus == PDH_CSTATUS_VALID_DATA)
        sample.write_bytes_per_sec = write_val.doubleValue;
    return sample;
}

NetSample GetNetSample() {
    NetSample sample;
    if (!g_net.ready) InitNetCounters();
    if (!g_net.ready) return sample;

    if (PdhCollectQueryData(g_net.query) != ERROR_SUCCESS) return sample;

    sample.recv_bytes_per_sec = SumFormattedArray(g_net.recv_bytes);
    sample.send_bytes_per_sec = SumFormattedArray(g_net.send_bytes);
    return sample;
}

double GetUptimeSeconds() {
    return (double)GetTickCount64() / 1000.0;
}

bool KillProcess(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    bool ok = TerminateProcess(h, 1) != 0;
    CloseHandle(h);
    return ok;
}

bool KillProcessTree(uint32_t root_pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return KillProcess(root_pid);

    std::unordered_map<DWORD, std::vector<DWORD>> children;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            children[pe.th32ParentProcessID].push_back(pe.th32ProcessID);
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);

    std::vector<DWORD> victims;
    std::vector<DWORD> stack{(DWORD)root_pid};
    std::unordered_set<DWORD> seen;
    while (!stack.empty()) {
        DWORD cur = stack.back();
        stack.pop_back();
        if (!seen.insert(cur).second) continue;
        victims.push_back(cur);
        auto it = children.find(cur);
        if (it != children.end())
            for (DWORD c : it->second) stack.push_back(c);
    }

    bool all_ok = true;
    for (auto rit = victims.rbegin(); rit != victims.rend(); ++rit)
        if (!KillProcess(*rit)) all_ok = false;
    return all_ok;
}

} // namespace sysinfo
